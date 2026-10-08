/*
 * SD2Cloud -- fingerprint of the file system index of a PS2 memory card (an sd2psx .mcd, without ECC).
 *
 * Instead of reading the whole card (16 MiB take ~20 s over MMCE), it reads only the superblock, the FAT and the
 * folders: each entry has the mode, size, creation and modification times (written by the PS2 on every save), cluster
 * and name. Saving, deleting or copying a save changes the index. On a 16 MiB Card1-1 that's ~330 KB read.
 * Raw page writes (FMCB/KELF installers) don't go through the index: "back up all cards" covers that.
 *
 * Layout (the same as mymcplus/ps2mc.py): superblock on page 0, "Sony PS2 Memory Card Format "; page size at 40,
 * pages per cluster at 42, clusters per card at 48, start of the allocatable area at 52, root cluster at 60, list of
 * indirect FAT clusters at 80 (32 x u32). Directory entry: 512 bytes, name at 64.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include "common.h"

#define MAGIC         "Sony PS2 Memory Card Format "
#define MAX_CLUSTER   2048     /* bytes */
#define CACHE         32
#define RUN           (32 * 1024)   /* clusters in a row are read and written together, up to this many bytes */
#define DF_EXISTS     0x8000
#define DF_DIRECTORY  0x0020
#define FAT_ALLOC     0x80000000u   /* a FAT entry: the cluster is in use; the other bits are the next one of its chain */
#define FAT_END       0xFFFFFFFFu   /* in use and the last of its chain (also what fat() answers on a read error) */
#define FAT_FREE      0x7FFFFFFFu

typedef struct {
    int fd;
    unsigned int csz, alloc, clusters;
    unsigned int ifc[32];
    struct {
        unsigned int n;
        int valid;
        unsigned char d[MAX_CLUSTER];
    } cache[CACHE];
    int next;
    long long bytesRead;
    unsigned int allocEnd, rootdir;   /* end of the allocatable area; the root folder's first cluster */
    unsigned int page, spare;         /* a page's size; the bytes of ECC after each page in the file (a .ps2: 16) */
    const unsigned char *mem;         /* the card is in memory, not in a file (fd = -1) */
    size_t memLen;
    /* While a card is being changed, the clusters of its FAT that were needed stay here, by their place in the FAT,
     * with what was changed in them (set_fat). They are written in one go when the change is whole (fat_flush), or
     * forgotten (fat_drop): a cluster of the FAT written for every entry changed was most of the writing */
    unsigned char **fatc;
    unsigned char *fatDirty;
    unsigned int nfat;
} mc_t;

static mc_t mc;   /* the card being read or changed: one at a time (the sd2psx can't take more anyway) */
static unsigned int ioReads, ioWrites;   /* how many times the card's file was read and written, for the log */

/* done with it (a card in memory has no file to close) */
static void mc_close(void)
{
    unsigned int i;
    if (mc.fd >= 0)
        close(mc.fd);
    mc.fd = -1;
    for (i = 0; mc.fatc && i < mc.nfat; i++)
        free(mc.fatc[i]);
    free(mc.fatc);
    free(mc.fatDirty);
    mc.fatc = NULL;
    mc.fatDirty = NULL;
    mc.nfat = 0;
}

/* the card MCFS_IMAGE stands for (mcfs_image) */
static char imageFile[700];
static const unsigned char *imageMem;
static size_t imageLen;

void mcfs_image(const char *file, const unsigned char *mem, size_t len)
{
    snprintf(imageFile, sizeof(imageFile), "%s", file ? file : "");
    imageMem = mem;
    imageLen = mem ? len : 0;
}

static unsigned int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }
static unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

/* n bytes from where the file is, however many reads it takes. 0 = all of them */
static int read_all(int fd, unsigned char *out, unsigned int n)
{
    while (n) {
        int got = read(fd, out, n);
        if (got <= 0)
            return -1;
        out += got;
        n -= got;
    }
    ioReads++;
    return 0;
}

/* the bytes of "physical" cluster n: from memory, or from the file, where a .ps2 has its ECC after each page */
static int read_cluster(mc_t *m, unsigned int n, unsigned char *out)
{
    unsigned char raw[MAX_CLUSTER + MAX_CLUSTER / 32];
    unsigned int pages = m->csz / m->page, stride = m->page + m->spare, p;
    if (m->mem) {
        if ((unsigned long long)(n + 1) * m->csz > m->memLen)
            return -1;
        memcpy(out, m->mem + (size_t)n * m->csz, m->csz);
        return 0;
    }
    if (!m->spare)
        return lseek(m->fd, (long)n * m->csz, SEEK_SET) < 0 || read_all(m->fd, out, m->csz) != 0 ? -1 : 0;
    if (lseek(m->fd, (long)n * pages * stride, SEEK_SET) < 0 || read_all(m->fd, raw, pages * stride) != 0)
        return -1;
    for (p = 0; p < pages; p++)
        memcpy(out + p * m->page, raw + p * stride, m->page);
    return 0;
}

/* "physical" cluster n (counting from the start of the card). NULL = read error */
static const unsigned char *cluster(mc_t *m, unsigned int n)
{
    int i;
    for (i = 0; i < CACHE; i++)
        if (m->cache[i].valid && m->cache[i].n == n)
            return m->cache[i].d;
    i = m->next;
    m->next = (m->next + 1) % CACHE;
    if (read_cluster(m, n, m->cache[i].d) != 0) {
        m->cache[i].valid = 0;
        return NULL;
    }
    m->bytesRead += m->csz;
    m->cache[i].n = n;
    m->cache[i].valid = 1;
    return m->cache[i].d;
}

/* FAT entry n, through the indirect FAT. 0xFFFFFFFF = error */
static unsigned int fat(mc_t *m, unsigned int n)
{
    unsigned int per = m->csz / 4, ind = n / per / per, i2 = (n / per) % per, i3 = n % per;
    const unsigned char *c;
    unsigned int icl;
    if (m->fatc && n / per < m->nfat && m->fatc[n / per])   /* as it is being changed */
        return le32(m->fatc[n / per] + i3 * 4);
    if (ind >= 32 || !(c = cluster(m, m->ifc[ind])))
        return 0xFFFFFFFF;
    icl = le32(c + i2 * 4);
    if (!(c = cluster(m, icl)))
        return 0xFFFFFFFF;
    return le32(c + i3 * 4);
}

/* walks the entries of a folder (starting at relative cluster c0, with n entries) and hands each one to cb */
static int walk_dir(mc_t *m, unsigned int c0, unsigned int n, int (*cb)(mc_t *m, const unsigned char *e, void *u), void *u)
{
    unsigned char entries[MAX_CLUSTER];
    unsigned int per = m->csz / 512, k = 0, c = c0, steps = 0;
    while (k < n) {
        const unsigned char *d = cluster(m, c + m->alloc);
        unsigned int i, e;
        if (!d)
            return -1;
        /* a copy: cb may read other clusters (a save's own folder), and the cache then reuses this one's place */
        memcpy(entries, d, m->csz);
        for (i = 0; i < per && k < n; i++, k++)
            if (cb(m, entries + i * 512, u))
                return -1;
        if (k >= n)
            break;
        e = fat(m, c);
        if (e == 0xFFFFFFFF || !(e & 0x80000000) || ++steps > m->clusters)
            return -1;   /* the folder claims more entries than its chain has */
        c = e & 0x7FFFFFFF;
    }
    return 0;
}

struct root_ctx;
typedef struct {
    wc_Sha256 *sha;
    int saves;
    struct root_ctx *root;   /* the root folder's records are gathered too, for its signature (NULL = not) */
} ctx_t;
static int on_root_sig(mc_t *m, const unsigned char *e, void *u);

static int is_live(const unsigned char *e)
{
    const char *name = (const char *)e + 64;
    return (le16(e) & DF_EXISTS) && strcmp(name, ".") && strcmp(name, "..");
}

static void hash_entry(wc_Sha256 *sha, const unsigned char *e)
{
    size_t n = strnlen((const char *)e + 64, 32);
    wc_Sha256Update(sha, e, 48);            /* mode, size, creation, cluster, entry, modification, attributes */
    wc_Sha256Update(sha, e + 64, n);        /* name */
}

static int on_save_entry(mc_t *m, const unsigned char *e, void *u)
{
    (void)m;
    if (is_live(e))
        hash_entry(((ctx_t *)u)->sha, e);
    return 0;
}

/* B?DATA-SYSTEM (A = USA, E = Europe, I = Japan, C = China): system data, not saves. The PS2 creates the folder
 * (icon.sys + history) the first time a game starts with the card, and the PS2/OPL rewrites "history" every time a
 * game starts (that's how the sd2psx detects the Game ID). It's left out of the fingerprint entirely; when the rest
 * of the card changes, the backup takes it along. */
static int is_system_dir(const char *name)
{
    return name[0] == 'B' && name[1] && !strcmp(name + 2, "DATA-SYSTEM");
}

static int on_root_entry(mc_t *m, const unsigned char *e, void *u)
{
    ctx_t *x = u;
    const char *name = (const char *)e + 64;
    if (!is_live(e))
        return 0;
    if (x->root)
        on_root_sig(m, e, x->root);
    if ((le16(e) & DF_DIRECTORY) && is_system_dir(name))
        return 0;   /* system data: not part of the fingerprint */
    hash_entry(x->sha, e);
    if (le16(e) & DF_DIRECTORY) {
        x->saves++;
        return walk_dir(m, le32(e + 16), le32(e + 4), on_save_entry, u);
    }
    return 0;
}

/* opens the card and reads the superblock. Returns the root's first cluster, or -1 (the file is closed then).
 * MCFS_IMAGE is the card mcfs_image told of, which is only read: a file anywhere, with or without the ECC bytes a
 * .ps2 has (told by its size), or a card in memory */
static int mc_open_mode(mc_t *m, const char *path, int flags)
{
    unsigned char sb[340];
    unsigned int i;
    int image = !strcmp(path, MCFS_IMAGE);
    memset(m, 0, sizeof(*m));
    m->fd = -1;
    if (image && flags != O_RDONLY)
        return -1;
    if (image && imageMem) {
        if (imageLen < sizeof(sb))
            return -1;
        memcpy(sb, imageMem, sizeof(sb));
        m->mem = imageMem;
        m->memLen = imageLen;
    } else {
        m->fd = open(image ? imageFile : path, flags);
        if (m->fd < 0)
            return -1;
        if (read(m->fd, sb, sizeof(sb)) != (int)sizeof(sb))
            goto bad;
    }
    if (memcmp(sb, MAGIC, sizeof(MAGIC) - 1) != 0)
        goto bad;
    m->page = le16(sb + 40);
    m->csz = m->page * le16(sb + 42);   /* page size * pages per cluster */
    m->clusters = le32(sb + 48);
    m->alloc = le32(sb + 52);
    m->allocEnd = le32(sb + 56);
    m->rootdir = le32(sb + 60);
    for (i = 0; i < 32; i++)
        m->ifc[i] = le32(sb + 80 + i * 4);
    if (m->page < 512 || m->csz < 512 || m->csz > MAX_CLUSTER || m->csz % 512)
        goto bad;
    if (m->allocEnd > m->clusters)   /* never past the card: the loops over the FAT stop there */
        m->allocEnd = m->clusters;
    if (image && m->fd >= 0) {
        long long size = lseek(m->fd, 0, SEEK_END), pages = (long long)m->clusters * le16(sb + 42);
        if (size == pages * (m->page + m->page / 32))
            m->spare = m->page / 32;
        else if (size != pages * m->page)
            goto bad;   /* not the size the card says it has, either way */
    }
    return (int)le32(sb + 60);
bad:
    if (m->fd >= 0)
        close(m->fd);
    m->fd = -1;
    return -1;
}

static int mc_open(mc_t *m, const char *path) { return mc_open_mode(m, path, O_RDONLY); }

long long mcfs_card_size(const unsigned char *sb, int *page)
{
    unsigned int p = le16(sb + 40), csz = p * le16(sb + 42);
    if (memcmp(sb, MAGIC, sizeof(MAGIC) - 1) != 0 || p < 512 || csz < 512 || csz > MAX_CLUSTER || csz % 512 || !le32(sb + 48))
        return 0;
    if (page)
        *page = p;
    return (long long)le32(sb + 48) * csz;
}

/* walks the root folder (its "." entry says how many entries it has) */
static int walk_root(mc_t *m, unsigned int rootdir, int (*cb)(mc_t *m, const unsigned char *e, void *u), void *u)
{
    const unsigned char *root = cluster(m, rootdir + m->alloc);
    return root ? walk_dir(m, rootdir, le32(root + 4), cb, u) : -1;
}

/* ------------------------------------------------------------ root signature */

static int compare_rec(const void *a, const void *b) { return memcmp(a, b, ROOT_REC); }

void mcfs_sign_records(unsigned char (*rec)[ROOT_REC], int n, char hex[65])
{
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 sha;
    int i;
    qsort(rec, n, ROOT_REC, compare_rec);
    wc_InitSha256(&sha);
    for (i = 0; i < n; i++)
        wc_Sha256Update(&sha, rec[i], ROOT_REC);
    wc_Sha256Final(&sha, h);
    wc_Sha256Free(&sha);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(hex + i * 2, "%02x", h[i]);
}

#define MAX_ROOT 512
typedef struct root_ctx {
    unsigned char (*rec)[ROOT_REC];
    int n;
} root_ctx_t;
static unsigned char rootRec[MAX_ROOT][ROOT_REC];

/* a record: the name (32 bytes, zero padded) and the modification time without its reserved byte (7 bytes) */
static int on_root_sig(mc_t *m, const unsigned char *e, void *u)
{
    root_ctx_t *x = u;
    (void)m;
    if (!is_live(e) || x->n >= MAX_ROOT)
        return 0;
    memset(x->rec[x->n], 0, ROOT_REC);
    memcpy(x->rec[x->n], e + 64, strnlen((const char *)e + 64, 32));
    memcpy(x->rec[x->n] + 32, e + 25, 7);
    x->n++;
    return 0;
}

int mcfs_root_signature(const char *path, char hex[65])
{
    root_ctx_t x = {rootRec, 0};
    int rootdir = mc_open(&mc, path), r = -1;
    if (rootdir < 0)
        return -1;
    if (walk_root(&mc, rootdir, on_root_sig, &x) == 0) {
        mcfs_sign_records(rootRec, x.n, hex);
        r = 0;
    }
    mc_close();
    return r;
}

/* ------------------------------------------------------------ the saves and their icons */

#define MAX_FILE (512 * 1024)

static unsigned long long mtime(const unsigned char *e)
{
    const unsigned char *t = e + 24;   /* reserved, sec, min, hour, day, month, year (16 bits) */
    return ((unsigned long long)le16(t + 6) << 40) | ((unsigned long long)t[5] << 32) | ((unsigned long long)t[4] << 24) |
           (t[3] << 16) | (t[2] << 8) | t[1];
}

typedef struct {
    mcfs_save_t *list;
    int n, max;
} list_ctx_t;

static int on_save_dir(mc_t *m, const unsigned char *e, void *u)
{
    list_ctx_t *x = u;
    const char *name = (const char *)e + 64;
    (void)m;
    if (!is_live(e) || !(le16(e) & DF_DIRECTORY) || is_system_dir(name) || x->n >= x->max)
        return 0;
    memset(&x->list[x->n], 0, sizeof(x->list[0]));
    snprintf(x->list[x->n].folder, sizeof(x->list[0].folder), "%.32s", name);
    x->list[x->n].when = mtime(e);
    x->list[x->n].cluster = le32(e + 16);
    x->list[x->n].count = le32(e + 4);
    x->n++;
    return 0;
}

/* the clusters the FAT doesn't mark as in use, up to the end of the allocatable area (like mymc); a FAT entry that
 * can't be read counts as in use */
static unsigned int free_clusters(mc_t *m)
{
    unsigned int n, k = 0;
    for (n = 0; n < m->allocEnd; n++)
        if (!(fat(m, n) & FAT_ALLOC))
            k++;
    return k;
}

/* the newest first, as the PS2 browser shows them; two of the same moment, by name */
static int newer_first(const void *a, const void *b)
{
    const mcfs_save_t *x = a, *y = b;
    return x->when < y->when ? 1 : x->when > y->when ? -1 : strcmp(x->folder, y->folder);
}

int mcfs_list_saves(const char *path, mcfs_save_t *list, int max, long long *freeBytes)
{
    list_ctx_t x = {list, 0, max};
    int rootdir = mc_open(&mc, path), r = -1;
    if (rootdir < 0)
        return -1;
    if (walk_root(&mc, rootdir, on_save_dir, &x) == 0) {
        r = x.n;
        if (list)
            qsort(list, x.n, sizeof(list[0]), newer_first);
        if (freeBytes)
            *freeBytes = (long long)free_clusters(&mc) * mc.csz;
    }
    mc_close();
    return r;
}

typedef struct {
    const char *name;
    unsigned int cluster, length;
    int found;
} find_t;

static int on_find(mc_t *m, const unsigned char *e, void *u)
{
    find_t *f = u;
    (void)m;
    if (!f->found && is_live(e) && !(le16(e) & DF_DIRECTORY) && !strncmp((const char *)e + 64, f->name, 32)) {
        f->cluster = le32(e + 16);
        f->length = le32(e + 4);
        f->found = 1;
    }
    return 0;
}

/* How a save's way into a card is going (mcfs_copy_save, mcfs_import_psu): the bytes of each phase, told every few KB */
static mcfs_step_cb onStep;
static long long stepDone, stepTotal, stepTold;
static int stepPhase, stepStop;

static void step_phase(int phase, long long total)
{
    stepPhase = phase;
    stepDone = stepTold = 0;
    stepTotal = total;
}

/* n more bytes read, written or read back. 1 = it was asked to give up (which is only heard before the reading back) */
static int step(size_t n)
{
    if (!onStep)
        return 0;
    stepDone += n;
    if (!stepStop && (stepDone - stepTold >= 8192 || stepDone >= stepTotal)) {
        stepTold = stepDone;
        if (onStep(stepPhase, stepDone, stepTotal) && stepPhase != MCFS_STEP_CHECK)
            stepStop = 1;
    }
    return stepStop;
}

/* the length bytes of a file, following its FAT chain from cluster c. The clusters it has in a row are read together,
 * straight into the buffer (from a card's own file; a .ps2's ECC bytes, or a card in memory, go cluster by cluster) */
static int read_chain(mc_t *m, unsigned int c, unsigned int length, buffer_t *out)
{
    unsigned int left, steps = 0;
    buf_free(out);
    /* the whole size at once: growing the buffer piece by piece would take twice the memory for a big file */
    if (length && length < 0x7FFFFFFF && (out->data = malloc(length + 1)) != NULL)
        out->cap = length + 1;
    for (left = length; left;) {
        unsigned int run = 1, k, e;
        while (run * m->csz < left && run < RUN / m->csz) {   /* how many in a row, from this one */
            e = fat(m, c + run - 1);
            if (e == FAT_END || !(e & FAT_ALLOC))
                return -1;   /* the chain ends before the file does */
            if ((e & ~FAT_ALLOC) != c + run)
                break;
            run++;
        }
        k = left < run * m->csz ? left : run * m->csz;
        if (run > 1 && !m->mem && !m->spare && out->cap > out->len + k) {
            if (lseek(m->fd, (long)(c + m->alloc) * m->csz, SEEK_SET) < 0 || read_all(m->fd, out->data + out->len, k) != 0)
                return -1;
            out->len += k;
            out->data[out->len] = 0;
            m->bytesRead += k;
        } else {
            const unsigned char *d = cluster(m, c + m->alloc);
            run = 1;
            k = left < m->csz ? left : m->csz;
            if (!d || buf_append(out, d, k))
                return -1;
        }
        if (step(k))
            return -1;
        if (!(left -= k))
            break;
        e = fat(m, c + run - 1);   /* on to what follows the last one read */
        if (e == FAT_END || !(e & FAT_ALLOC) || ++steps > m->clusters)
            return -1;
        c = e & ~FAT_ALLOC;
    }
    return 0;
}

/* reads a file of a save folder */
static int read_file(mc_t *m, const mcfs_save_t *dir, const char *name, buffer_t *out)
{
    find_t f = {name, 0, 0, 0};
    buf_free(out);
    if (walk_dir(m, dir->cluster, dir->count, on_find, &f) != 0 || !f.found || !f.length || f.length > MAX_FILE)
        return -1;
    return read_chain(m, f.cluster, f.length, out);
}

static int read_icon(mc_t *m, const mcfs_save_t *s, buffer_t *iconsys, buffer_t *ico)
{
    char icoName[65];
    /* icon.sys: "PS2D", ..., the name of the normal icon at 260 (64 bytes) */
    if (read_file(m, s, "icon.sys", iconsys) != 0 || iconsys->len < 964 || memcmp(iconsys->data, "PS2D", 4) != 0)
        return -1;
    snprintf(icoName, sizeof(icoName), "%.64s", (const char *)iconsys->data + 260);
    return read_file(m, s, icoName, ico);
}

typedef struct {
    const char *name;
    char *real;
    size_t size;
} find_name_t;

/* the file of that name whatever its letters' case, as a title.cfg may write it: its name as the folder has it */
static int on_find_name(mc_t *m, const unsigned char *e, void *u)
{
    find_name_t *f = u;
    (void)m;
    if (!f->real[0] && is_live(e) && !(le16(e) & DF_DIRECTORY) && le32(e + 4) && !strncasecmp((const char *)e + 64, f->name, 32))
        snprintf(f->real, f->size, "%.32s", (const char *)e + 64);
    return 0;
}

int mcfs_save_app(const char *path, const mcfs_save_t *s, char *boot, size_t size)
{
    buffer_t cfg = {0};
    char name[64] = "", *line, *end;
    find_name_t f = {name, boot, size};
    boot[0] = 0;
    if (mc_open(&mc, path) < 0)
        return 0;
    if (read_file(&mc, s, "title.cfg", &cfg) == 0 && cfg.len < 16 * 1024) {
        for (line = (char *)cfg.data; line && *line && !name[0]; line = end ? end + 1 : NULL) {
            end = strchr(line, '\n');
            if (!strncmp(line, "boot=", 5)) {   /* as OPL reads it: "boot", in lowercase, at the start of a line */
                size_t n = strcspn(line + 5, "\r\n");
                while (n && line[5 + n - 1] == ' ')
                    n--;
                snprintf(name, sizeof(name), "%.*s", (int)(n < sizeof(name) - 1 ? n : sizeof(name) - 1), line + 5);
            }
        }
        if (name[0])
            walk_dir(&mc, s->cluster, s->count, on_find_name, &f);
    }
    mc_close();
    buf_free(&cfg);
    return boot[0] != 0;
}

int mcfs_save_icon(const char *path, const mcfs_save_t *s, buffer_t *iconsys, buffer_t *ico)
{
    int r;
    if (mc_open(&mc, path) < 0)
        return -1;
    r = read_icon(&mc, s, iconsys, ico);
    mc_close();
    if (r != 0) {
        buf_free(iconsys);
        buf_free(ico);
    }
    return r;
}

int mcfs_newest_save_icon(const char *path, char folder[33], buffer_t *iconsys, buffer_t *ico)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    list_ctx_t x = {list, 0, MCFS_MAX_SAVES};
    int rootdir = mc_open(&mc, path), r = -1, i;
    if (rootdir < 0)
        return -1;
    /* the newest save that has an icon (a folder without icon.sys, like some games' extra data, is skipped) */
    if (walk_root(&mc, rootdir, on_save_dir, &x) == 0) {
        qsort(list, x.n, sizeof(list[0]), newer_first);
        for (i = 0; i < x.n && r != 0; i++)
            if (read_icon(&mc, &list[i], iconsys, ico) == 0) {
                snprintf(folder, 33, "%s", list[i].folder);
                r = 0;
            }
    }
    mc_close();
    if (r != 0) {
        buf_free(iconsys);
        buf_free(ico);
    }
    return r;
}

/* ------------------------------------------------------------ fingerprint */

int mcfs_fingerprint(const char *path, char hex[65], int *saves) { return mcfs_fingerprint_root(path, hex, saves, NULL); }

int mcfs_fingerprint_root(const char *path, char hex[65], int *saves, char rootSig[65])
{
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 sha;
    root_ctx_t root = {rootRec, 0};
    ctx_t x = {&sha, 0, rootSig ? &root : NULL};
    int rootdir = mc_open(&mc, path), r = -1, i;
    if (rootSig)
        rootSig[0] = 0;
    if (rootdir < 0)
        return -1;
    wc_InitSha256(&sha);
    if (walk_root(&mc, rootdir, on_root_entry, &x) == 0) {
        wc_Sha256Final(&sha, h);
        for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
            sprintf(hex + i * 2, "%02x", h[i]);
        if (saves)
            *saves = x.saves;
        if (rootSig)
            mcfs_sign_records(rootRec, root.n, rootSig);
        r = 0;
    }
    wc_Sha256Free(&sha);
    mc_close();
    return r;
}

/* ------------------------------------------------------------ changing a card: copy, delete, export a save
 *
 * The same rules as mymc/mymcplus (which can check a card after it): allocating takes the first free cluster of the
 * FAT and marks it as the end of a chain right away; freeing only clears the "allocated" bit of each cluster of the
 * chain; a folder is its entry in the root (length = how many entries it has) plus its own entries: "." (the parent's
 * first cluster and this folder's place in it), ".." (zeros) and the files. Deleting clears the entry's "exists" bit
 * first and frees the clusters after, so a write cut halfway leaks clusters instead of leaving a save pointing to
 * freed ones. A copy is read back and compared with the original before it counts. */

#define ENT       512
#define MAX_ENTRIES 256

static void put32(unsigned char *p, unsigned int v) { p[0] = v, p[1] = v >> 8, p[2] = v >> 16, p[3] = v >> 24; }

/* count clusters in a row, the first being "physical" cluster n, in one write */
static int put_run(mc_t *m, unsigned int n, const unsigned char *d, unsigned int count)
{
    unsigned int left = count * m->csz, k;
    const unsigned char *p = d;
    int i;
    if (lseek(m->fd, (long)n * m->csz, SEEK_SET) < 0)
        return -1;
    while (left) {
        int put = write(m->fd, p, left);
        if (put <= 0)
            return -1;
        p += put;
        left -= put;
    }
    ioWrites++;
    for (i = 0; i < CACHE; i++)
        if (m->cache[i].valid && (k = m->cache[i].n - n) < count)   /* (one before n comes out huge: unsigned) */
            memcpy(m->cache[i].d, d + k * m->csz, m->csz);
    return 0;
}

static int put_cluster(mc_t *m, unsigned int n, const unsigned char *d) { return put_run(m, n, d, 1); }

/* the cluster that holds FAT entry n (FAT_END = it can't be told) */
static unsigned int fat_cluster(mc_t *m, unsigned int n)
{
    unsigned int per = m->csz / 4, ind = n / per / per, i2 = (n / per) % per;
    const unsigned char *c;
    if (ind >= 32 || !(c = cluster(m, m->ifc[ind])))
        return FAT_END;
    return le32(c + i2 * 4);
}

/* changes FAT entry n in memory (see mc_t): nothing is written before fat_flush */
static int set_fat(mc_t *m, unsigned int n, unsigned int v)
{
    unsigned int per = m->csz / 4, k = n / per, icl;
    const unsigned char *c;
    if (!m->fatc) {
        m->nfat = (m->clusters + per - 1) / per;
        m->fatc = calloc(m->nfat, sizeof(*m->fatc));
        m->fatDirty = calloc(m->nfat, 1);
        if (!m->fatc || !m->fatDirty) {
            free(m->fatc);
            free(m->fatDirty);
            m->fatc = NULL;
            m->fatDirty = NULL;
            m->nfat = 0;
            return -1;
        }
    }
    if (k >= m->nfat)
        return -1;
    if (!m->fatc[k]) {
        if ((icl = fat_cluster(m, n)) == FAT_END || !(c = cluster(m, icl)) || !(m->fatc[k] = malloc(m->csz)))
            return -1;
        memcpy(m->fatc[k], c, m->csz);
    }
    put32(m->fatc[k] + (n % per) * 4, v);
    m->fatDirty[k] = 1;
    return 0;
}

/* writes the clusters of the FAT that were changed. 0 = they are all in the card */
static int fat_flush(mc_t *m)
{
    unsigned int per = m->csz / 4, k, icl;
    for (k = 0; m->fatc && k < m->nfat; k++)
        if (m->fatDirty[k]) {
            if ((icl = fat_cluster(m, k * per)) == FAT_END || put_cluster(m, icl, m->fatc[k]) != 0)
                return -1;
            m->fatDirty[k] = 0;
        }
    return 0;
}

/* forgets the changes to the FAT that weren't written: the card's own FAT stands */
static void fat_drop(mc_t *m)
{
    unsigned int k;
    for (k = 0; m->fatc && k < m->nfat; k++)
        if (m->fatDirty[k]) {
            free(m->fatc[k]);
            m->fatc[k] = NULL;
            m->fatDirty[k] = 0;
        }
}

static unsigned int alloc_cluster(mc_t *m, unsigned int *cursor)
{
    unsigned int n;
    for (n = *cursor; n < m->allocEnd; n++)
        if (!(fat(m, n) & FAT_ALLOC)) {
            if (set_fat(m, n, FAT_END) != 0)
                return FAT_END;
            *cursor = n + 1;
            return n;
        }
    return FAT_END;
}

/* data into a new chain of clusters; first = FAT_END when there's no data. The clusters it gets in a row are written
 * together; the chain itself is only in the FAT kept in memory (set_fat) until the caller has it written */
static int write_chain(mc_t *m, unsigned int *cursor, const unsigned char *d, size_t n, unsigned int *first)
{
    static unsigned char buf[RUN];
    unsigned int prev = FAT_END, c, start = 0, count = 0, bytes = 0;
    size_t off;
    *first = FAT_END;
    for (off = 0; off < n; off += m->csz) {
        size_t k = n - off < m->csz ? n - off : m->csz;
        if ((c = alloc_cluster(m, cursor)) == FAT_END)
            return -1;
        if (count && (c != start + count || count == RUN / m->csz)) {   /* not next to the ones waiting, or no more room */
            if (put_run(m, start + m->alloc, buf, count) != 0 || step(bytes))
                return -1;
            count = bytes = 0;
        }
        if (!count)
            start = c;
        memset(buf + count * m->csz, 0, m->csz);
        memcpy(buf + count * m->csz, d + off, k);
        count++;
        bytes += k;
        if (prev == FAT_END)
            *first = c;
        else if (set_fat(m, prev, FAT_ALLOC | c) != 0)
            return -1;
        prev = c;
    }
    if (count && (put_run(m, start + m->alloc, buf, count) != 0 || step(bytes)))
        return -1;
    return 0;
}

static void free_chain(mc_t *m, unsigned int c)
{
    unsigned int steps = 0;
    while (c != FAT_END && c != FAT_FREE && c < m->allocEnd && steps++ < m->clusters) {
        unsigned int next = fat(m, c);
        if (!(next & FAT_ALLOC))
            break;
        next &= ~FAT_ALLOC;
        set_fat(m, c, next);
        c = next;
    }
}

/* the cluster that holds entry i of the folder starting at c0 (FAT_END = the chain is shorter) */
static unsigned int entry_cluster(mc_t *m, unsigned int c0, unsigned int i)
{
    unsigned int k = i / (m->csz / ENT), c = c0;
    while (k--) {
        unsigned int e = fat(m, c);
        if (e == FAT_END || !(e & FAT_ALLOC))
            return FAT_END;
        c = e & ~FAT_ALLOC;
    }
    return c;
}

static int put_entry(mc_t *m, unsigned int c0, unsigned int i, const unsigned char *ent)
{
    static unsigned char buf[MAX_CLUSTER];
    unsigned int c = entry_cluster(m, c0, i);
    const unsigned char *d;
    if (c == FAT_END || !(d = cluster(m, c + m->alloc)))
        return -1;
    memcpy(buf, d, m->csz);
    memcpy(buf + (i % (m->csz / ENT)) * ENT, ent, ENT);
    return put_cluster(m, c + m->alloc, buf);
}

/* a save as it is on a card: its root entry and where it is, its entries ("." and ".." first, then the files that
 * exist) and the files' contents */
typedef struct {
    unsigned char root[ENT];
    unsigned int slot;              /* the entry's place in the root */
    unsigned char ent[MAX_ENTRIES][ENT];
    int n;
    buffer_t data[MAX_ENTRIES];
} save_raw_t;

typedef struct {
    const char *name;
    unsigned int i, slot, freeSlot, count;
    unsigned char ent[ENT];
    int found;
} root_find_t;

static int on_root_find(mc_t *m, const unsigned char *e, void *u)
{
    root_find_t *f = u;
    (void)m;
    if (f->i >= 2 && !(le16(e) & DF_EXISTS) && f->freeSlot == FAT_END)
        f->freeSlot = f->i;   /* a deleted entry can be used again */
    if (!f->found && is_live(e) && (le16(e) & DF_DIRECTORY) && !strncmp((const char *)e + 64, f->name, 32)) {
        memcpy(f->ent, e, ENT);
        f->slot = f->i;
        f->found = 1;
    }
    f->i++;
    return 0;
}

static int find_in_root(mc_t *m, const char *name, root_find_t *f)
{
    const unsigned char *root = cluster(m, m->rootdir + m->alloc);
    memset(f, 0, sizeof(*f));
    f->name = name;
    f->freeSlot = FAT_END;
    if (!root)
        return -1;
    f->count = le32(root + 4);
    return walk_dir(m, m->rootdir, f->count, on_root_find, f);
}

typedef struct {
    save_raw_t *s;
    unsigned int i;
} gather_t;

static int on_gather(mc_t *m, const unsigned char *e, void *u)
{
    gather_t *g = u;
    (void)m;
    if (g->i < 2 || is_live(e)) {
        if (g->s->n == MAX_ENTRIES)
            return -1;   /* more files than fit here: an error, rather than a save handled in part */
        memcpy(g->s->ent[g->s->n++], e, ENT);
    }
    g->i++;
    return 0;
}

static void save_free(save_raw_t *s)
{
    int i;
    for (i = 0; i < MAX_ENTRIES; i++)
        buf_free(&s->data[i]);
}

/* the save's entry in the root and its own entries, without the files' contents */
static int read_entries(mc_t *m, const char *folder, save_raw_t *s)
{
    root_find_t f;
    gather_t g = {s, 0};
    s->n = 0;
    if (find_in_root(m, folder, &f) != 0)
        return MCFS_ERR_IO;
    if (!f.found)
        return MCFS_ERR_NOT_FOUND;
    memcpy(s->root, f.ent, ENT);
    s->slot = f.slot;
    if (walk_dir(m, le32(f.ent + 16), le32(f.ent + 4), on_gather, &g) != 0 || s->n < 2)
        return MCFS_ERR_IO;
    return MCFS_OK;
}

static int read_save(mc_t *m, const char *folder, save_raw_t *s)
{
    int i, r = read_entries(m, folder, s);
    if (r == MCFS_OK && onStep) {   /* how much there is to read, in the phase the caller is in */
        long long bytes = 0;
        for (i = 2; i < s->n; i++)
            bytes += le32(s->ent[i] + 4);
        step_phase(stepPhase, bytes);
    }
    for (i = 2; r == MCFS_OK && i < s->n; i++)
        if (!(le16(s->ent[i]) & DF_DIRECTORY) && le32(s->ent[i] + 4) &&
            read_chain(m, le32(s->ent[i] + 16), le32(s->ent[i] + 4), &s->data[i]) != 0)
            r = MCFS_ERR_IO;
    return r;
}

static save_raw_t srcSave, checkSave;

int mcfs_save_info(const char *path, const char *folder, long long *bytes, int *files)
{
    int r, i;
    *bytes = 0, *files = 0;
    if (mc_open(&mc, path) < 0)
        return MCFS_ERR_IO;
    r = read_entries(&mc, folder, &srcSave);   /* the sizes are in the entries: the files themselves aren't read */
    mc_close();
    for (i = 2; r == MCFS_OK && i < srcSave.n; i++)
        *bytes += le32(srcSave.ent[i] + 4), (*files)++;
    return r;
}

/* srcSave (read from another card or from a .psu) into a card, read back and compared before it counts */
static int write_save(const char *to)
{
    static unsigned char dir[MAX_ENTRIES][ENT];
    char folder[33];
    root_find_t f;
    unsigned int cursor = 0, need, perCl, slot, first = FAT_END, dirFirst = FAT_END, i, k, written = 2;
    long long bytes = 0;
    int r = MCFS_ERR_IO, known = 0, flushed = 0;
    snprintf(folder, sizeof(folder), "%.32s", (const char *)srcSave.root + 64);
    if (mc_open_mode(&mc, to, O_RDWR) < 0)
        return MCFS_ERR_IO;
    if (find_in_root(&mc, folder, &f) != 0)
        goto out;
    if (f.found) {
        r = MCFS_ERR_EXISTS;
        goto out;
    }
    /* room: the folder's entries, each file, and one more cluster when the root has to grow */
    perCl = mc.csz / ENT;
    need = (srcSave.n + perCl - 1) / perCl;
    for (i = 2; i < (unsigned int)srcSave.n; i++) {
        need += (le32(srcSave.ent[i] + 4) + mc.csz - 1) / mc.csz;
        bytes += le32(srcSave.ent[i] + 4);
    }
    slot = f.freeSlot != FAT_END ? f.freeSlot : f.count;
    if (slot == f.count && f.count % perCl == 0)
        need++;
    if (free_clusters(&mc) < need) {
        r = MCFS_ERR_FULL;
        goto out;
    }
    /* the files */
    step_phase(MCFS_STEP_WRITE, bytes);
    memcpy(dir, srcSave.ent, ENT * srcSave.n);
    for (i = 2; i < (unsigned int)srcSave.n; i++) {
        if (le16(dir[i]) & DF_DIRECTORY)
            continue;
        if (write_chain(&mc, &cursor, srcSave.data[i].data, le32(dir[i] + 4), &first) != 0)
            goto out;
        put32(dir[i] + 16, first);
        put32(dir[i] + 20, 0);
        first = FAT_END;
        written = i + 1;
    }
    /* the folder's own entries: "." knows where the folder is in the root */
    put32(dir[0] + 4, 0);
    put32(dir[0] + 16, mc.rootdir);
    put32(dir[0] + 20, slot);
    put32(dir[1] + 16, 0);
    put32(dir[1] + 20, 0);
    if (write_chain(&mc, &cursor, dir[0], (size_t)srcSave.n * ENT, &dirFirst) != 0)
        goto out;
    /* a new place at the end of the root: a new cluster for it when the last one is full */
    if (slot == f.count && f.count % perCl == 0) {
        static unsigned char zero[MAX_CLUSTER];
        unsigned int last = entry_cluster(&mc, mc.rootdir, f.count - 1), c = alloc_cluster(&mc, &cursor);
        if (last == FAT_END || c == FAT_END || put_cluster(&mc, c + mc.alloc, zero) != 0 || set_fat(&mc, last, FAT_ALLOC | c) != 0)
            goto out;
    }
    /* the FAT, which was only changed in memory until here: the card has those clusters taken from now on */
    flushed = 1;
    if (fat_flush(&mc) != 0)
        goto out;
    /* last of all, the entry in the root: until here the card doesn't know the save */
    memcpy(dir[0], srcSave.root, ENT);
    put32(dir[0] + 4, srcSave.n);
    put32(dir[0] + 16, dirFirst);
    put32(dir[0] + 20, 0);
    if (put_entry(&mc, mc.rootdir, slot, dir[0]) != 0)
        goto out;
    known = 1;
    if (slot == f.count) {   /* the root has one more entry: its "." says how many */
        const unsigned char *rc = cluster(&mc, mc.rootdir + mc.alloc);
        static unsigned char dot[ENT];
        if (!rc)
            goto out;
        memcpy(dot, rc, ENT);
        put32(dot + 4, f.count + 1);
        if (put_entry(&mc, mc.rootdir, 0, dot) != 0)
            goto out;
    }
    r = MCFS_OK;
out:
    if (r != MCFS_OK && !known) {
        /* given up, or it failed, before the card knew the save. With the FAT still only in memory there is nothing
         * to undo: the clusters that were written are free in the card's own FAT. Else what was written of the save
         * gives its room back (the files written whole, the one under way, the folder's entries) */
        if (flushed) {
            for (k = 2; k < written; k++)
                if (!(le16(dir[k]) & DF_DIRECTORY))
                    free_chain(&mc, le32(dir[k] + 16));
            free_chain(&mc, first);
            free_chain(&mc, dirFirst);
            fat_flush(&mc);
        } else
            fat_drop(&mc);
        if (stepStop)
            r = MCFS_ERR_CANCELLED;
    }
    mc_close();
    if (r == MCFS_OK) {   /* read the copy back and compare it with the original */
        stepPhase = MCFS_STEP_CHECK;
        if (mc_open(&mc, to) < 0 || read_save(&mc, folder, &checkSave) != MCFS_OK || checkSave.n != srcSave.n)
            r = MCFS_ERR_CHECK;
        for (i = 2; r == MCFS_OK && i < (unsigned int)srcSave.n; i++)
            if (checkSave.data[i].len != srcSave.data[i].len ||
                (srcSave.data[i].len && memcmp(checkSave.data[i].data, srcSave.data[i].data, srcSave.data[i].len)))
                r = MCFS_ERR_CHECK;
        if (mc.fd >= 0)
            mc_close();
        save_free(&checkSave);
    }
    return r;
}

int mcfs_copy_save(const char *from, const char *folder, const char *to, mcfs_step_cb progress)
{
    u64 start = now_ms();
    int r, i;
    /* the save, from its card */
    if (mc_open(&mc, from) < 0)
        return MCFS_ERR_IO;
    ioReads = ioWrites = 0;
    onStep = progress;
    stepStop = 0;
    step_phase(MCFS_STEP_READ, 0);
    r = read_save(&mc, folder, &srcSave);
    mc_close();
    if (stepStop)
        r = MCFS_ERR_CANCELLED;
    /* a folder inside the save (the PS2 never makes one) would be copied as an entry pointing nowhere */
    for (i = 2; r == MCFS_OK && i < srcSave.n; i++)
        if (le16(srcSave.ent[i]) & DF_DIRECTORY)
            r = MCFS_ERR_IO;
    if (r == MCFS_OK)
        r = write_save(to);
    save_free(&srcSave);
    onStep = NULL;
    log_msg("mcfs: copy of %s: %d, %u reads and %u writes of the cards in %u ms", folder, r, ioReads, ioWrites, (unsigned)(now_ms() - start));
    return r;
}

int mcfs_delete_save(const char *path, const char *folder)
{
    static unsigned char ent[ENT];
    root_find_t f;
    gather_t g = {&srcSave, 0};
    u64 start = now_ms();
    int r = MCFS_ERR_IO, i;
    srcSave.n = 0;
    if (mc_open_mode(&mc, path, O_RDWR) < 0)
        return MCFS_ERR_IO;
    ioReads = ioWrites = 0;
    if (find_in_root(&mc, folder, &f) != 0)
        goto out;
    if (!f.found) {
        r = MCFS_ERR_NOT_FOUND;
        goto out;
    }
    if (walk_dir(&mc, le32(f.ent + 16), le32(f.ent + 4), on_gather, &g) != 0)
        goto out;
    /* first the entry stops existing, then its clusters are freed */
    memcpy(ent, f.ent, ENT);
    ent[1] &= ~(DF_EXISTS >> 8);
    if (put_entry(&mc, mc.rootdir, f.slot, ent) != 0)
        goto out;
    for (i = 2; i < srcSave.n; i++)
        if (!(le16(srcSave.ent[i]) & DF_DIRECTORY))
            free_chain(&mc, le32(srcSave.ent[i] + 16));
    free_chain(&mc, le32(f.ent + 16));
    if (fat_flush(&mc) == 0)   /* the FAT with all of them free, in one go */
        r = MCFS_OK;
out:
    mc_close();
    log_msg("mcfs: %s deleted: %d, %u reads and %u writes of the card in %u ms", folder, r, ioReads, ioWrites, (unsigned)(now_ms() - start));
    return r;
}

/* a .psu (the format uLaunchELF and others use): the folder's entry, "." and "..", then each file's entry followed by
 * its contents padded to 1 KB */
int mcfs_export_psu(const char *path, const char *folder, buffer_t *out)
{
    static const unsigned char pad[1024];
    static unsigned char e[ENT];
    int r, i;
    buf_free(out);
    if (mc_open(&mc, path) < 0)
        return MCFS_ERR_IO;
    r = read_save(&mc, folder, &srcSave);
    mc_close();
    for (i = -1; r == MCFS_OK && i < srcSave.n; i++) {
        unsigned int len;
        memcpy(e, i < 0 ? srcSave.root : srcSave.ent[i], ENT);
        if (i < 0)
            put32(e + 4, srcSave.n);
        put32(e + 16, 0);
        put32(e + 20, 0);
        if (buf_append(out, e, ENT)) {
            r = MCFS_ERR_IO;
            break;
        }
        if (i < 2 || (le16(e) & DF_DIRECTORY))
            continue;
        len = le32(e + 4);
        if ((len && buf_append(out, srcSave.data[i].data, len)) || (len % 1024 && buf_append(out, pad, 1024 - len % 1024)))
            r = MCFS_ERR_IO;
    }
    save_free(&srcSave);
    if (r != MCFS_OK)
        buf_free(out);
    return r;
}

/* ------------------------------------------------------------ a save from a .psu file into a card */

/* a name the card can hold: 1 to 32 bytes, none of them a control character or a slash */
static int name_ok(const unsigned char *e)
{
    const char *s = (const char *)e + 64;
    size_t n = strnlen(s, 33), i;
    if (!n || n > 32 || !strcmp(s, ".") || !strcmp(s, ".."))
        return 0;
    for (i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || s[i] == '/')
            return 0;
    return 1;
}

/* the .psu as a save read from a card (srcSave), checked whole before anything is written anywhere: the sizes
 * against the file's own, the names, no folder inside it, no name twice. The file is closed again before this
 * returns: on the sd2psx it must not be open while a card is */
static int read_psu(const char *path, save_raw_t *s)
{
    long size, at = ENT;
    int fd = open(path, O_RDONLY), r = MCFS_ERR_BAD, i, k, n;
    s->n = 0;
    if (fd < 0)
        return MCFS_ERR_IO;
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    step_phase(MCFS_STEP_READ, size);
    if (size < 3 * ENT || read(fd, s->root, ENT) != ENT)
        goto out;
    n = (int)le32(s->root + 4);
    if (!(le16(s->root) & DF_DIRECTORY) || !name_ok(s->root) || n < 2 || n > MAX_ENTRIES)
        goto out;
    s->root[1] |= DF_EXISTS >> 8;
    for (i = 0; i < n; i++) {
        unsigned char *e = s->ent[i];
        unsigned int len, done;
        if (at + ENT > size || read(fd, e, ENT) != ENT)
            goto out;
        at += ENT;
        s->n = i + 1;
        if (i < 2) {   /* the folder's own two entries: where they point is set when the save goes into a card */
            if (!(le16(e) & DF_DIRECTORY))
                goto out;
            memset(e + 64, 0, 32);
            strcpy((char *)e + 64, i ? ".." : ".");
            e[1] |= DF_EXISTS >> 8;
            continue;
        }
        if ((le16(e) & DF_DIRECTORY) || !name_ok(e))
            goto out;
        for (k = 2; k < i; k++)
            if (!strncmp((const char *)e + 64, (const char *)s->ent[k] + 64, 32))
                goto out;
        e[1] |= DF_EXISTS >> 8;
        len = le32(e + 4);
        if (len > (unsigned long)(size - at))
            goto out;
        buf_free(&s->data[i]);
        if (len) {
            if (!(s->data[i].data = malloc(len + 1))) {
                r = MCFS_ERR_IO;
                goto out;
            }
            s->data[i].cap = len + 1;
            for (done = 0; done < len;) {
                int got = read(fd, s->data[i].data + done, len - done > 64 * 1024 ? 64 * 1024 : len - done);
                if (got <= 0)
                    goto out;
                done += got;
                if (step(got)) {
                    r = MCFS_ERR_CANCELLED;
                    goto out;
                }
            }
            s->data[i].len = len;
        }
        at += (len + 1023) & ~1023u;   /* each file is padded to 1 KB (the last one's padding may be missing) */
        if (len % 1024 && at <= size && lseek(fd, at, SEEK_SET) < 0)
            goto out;
    }
    r = MCFS_OK;
out:
    close(fd);
    if (r != MCFS_OK)
        save_free(s);
    return r;
}

/* the file of srcSave with that name (-1 = it has none) */
static int psu_file(const char *name)
{
    int i;
    for (i = 2; i < srcSave.n; i++)
        if (!strncmp((const char *)srcSave.ent[i] + 64, name, 32))
            return i;
    return -1;
}

int mcfs_psu_info(const char *psu, mcfs_psu_t *info, buffer_t *iconsys, buffer_t *ico)
{
    int r = read_psu(psu, &srcSave), i, k;
    buf_free(iconsys);
    buf_free(ico);
    memset(info, 0, sizeof(*info));
    if (r != MCFS_OK)
        return r;
    snprintf(info->folder, sizeof(info->folder), "%.32s", (const char *)srcSave.root + 64);
    info->when = mtime(srcSave.root);
    for (i = 2; i < srcSave.n; i++)
        info->bytes += le32(srcSave.ent[i] + 4), info->files++;
    /* its icon, found the way a card's is: icon.sys names it */
    if ((i = psu_file("icon.sys")) >= 0 && srcSave.data[i].len >= 964 && !memcmp(srcSave.data[i].data, "PS2D", 4)) {
        char name[65];
        snprintf(name, sizeof(name), "%.64s", (const char *)srcSave.data[i].data + 260);
        if ((k = psu_file(name)) < 0 || !srcSave.data[k].len || srcSave.data[k].len > MAX_FILE ||
            buf_append(iconsys, srcSave.data[i].data, srcSave.data[i].len) || buf_append(ico, srcSave.data[k].data, srcSave.data[k].len)) {
            buf_free(iconsys);
            buf_free(ico);
        }
    }
    save_free(&srcSave);
    return r;
}

int mcfs_import_psu(const char *psu, const char *to, mcfs_step_cb progress)
{
    u64 start = now_ms();
    int r;
    ioReads = ioWrites = 0;
    onStep = progress;
    stepStop = 0;
    r = read_psu(psu, &srcSave);
    if (r == MCFS_OK)
        r = write_save(to);
    save_free(&srcSave);
    onStep = NULL;
    log_msg("mcfs: import of %s: %d, %u reads and %u writes of the card in %u ms", psu, r, ioReads, ioWrites, (unsigned)(now_ms() - start));
    return r;
}

/* ------------------------------------------------------------ a new card

   An empty, formatted 8 MB card, byte for byte the one the sd2psx makes when it is asked for a card that isn't there
   yet (its firmware's ps2_cardman.c: build_superblock, blockRoot, genblock). 8192 clusters of 1 KB: 16 kept for the
   superblock, the indirect FAT in cluster 16, the FAT in the 32 after it, and from cluster 49 on what can be
   allocated, the root folder first, up to the two blocks kept as backup at the end. Everything else is 0xFF. */

#define NEW_SIZE     (8 * 1024 * 1024)
#define NEW_CLUSTERS (NEW_SIZE / 1024)
#define NEW_RESERVED 16
#define NEW_FAT      ((NEW_CLUSTERS * 4 + 1023) / 1024)   /* 32 clusters */
#define NEW_IFC      ((NEW_FAT * 4 + 1023) / 1024)        /* 1 */
#define NEW_ALLOC    (NEW_RESERVED + NEW_IFC + NEW_FAT)   /* 49 */
#define NEW_BACKUP1  (NEW_CLUSTERS / 8 - 1)
#define NEW_BACKUP2  (NEW_BACKUP1 - 1)
#define NEW_END      (NEW_BACKUP2 * 8 - NEW_ALLOC)        /* 8127 */
#define NEW_HEAD     ((NEW_ALLOC + 1) * 1024)             /* through the root folder's cluster: all that isn't 0xFF */
#define NEW_PIECE    (512 * 1024)

static void new16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void new32(unsigned char *p, unsigned int v)
{
    new16(p, v);
    new16(p + 2, v >> 16);
}

/* the start of that card, NEW_HEAD bytes */
static void new_head(unsigned char *h)
{
    /* the time the sd2psx gives the root folder: 2000-01-12 06:00:41 */
    static const unsigned char when[8] = {0x00, 0x29, 0x00, 0x06, 0x0C, 0x01, 0xD0, 0x07};
    unsigned char *fat = h + (NEW_RESERVED + NEW_IFC) * 1024, *dir = h + NEW_ALLOC * 1024;
    unsigned int i;
    memset(h, 0xFF, NEW_HEAD);
    /* the superblock */
    memset(h, 0, 0xD0);
    memcpy(h, MAGIC, 28);
    memcpy(h + 0x1C, "1.2.0.0", 7);
    new16(h + 0x28, 512);              /* a page */
    new16(h + 0x2A, 2);                /* pages in a cluster */
    new16(h + 0x2C, 16);               /* pages in an erase block */
    new16(h + 0x2E, 0xFF00);
    new32(h + 0x30, NEW_CLUSTERS);
    new32(h + 0x34, NEW_ALLOC);
    new32(h + 0x38, NEW_END);
    new32(h + 0x3C, 0);                /* the root folder's cluster, counted from where the allocatable area starts */
    new32(h + 0x40, NEW_BACKUP1);
    new32(h + 0x44, NEW_BACKUP2);
    for (i = 0; i < NEW_IFC; i++)
        new32(h + 0x50 + i * 4, NEW_RESERVED + i);
    memset(h + 0x150, 0, 0x2C);
    h[0x150] = 2;                      /* a PS2 card */
    h[0x151] = 0x2B;                   /* what it can do */
    new32(h + 0x154, 1024);            /* a cluster */
    new32(h + 0x158, 256);             /* FAT entries in a cluster */
    new32(h + 0x15C, 8);               /* clusters in a block */
    new32(h + 0x160, 0xFFFFFFFFu);
    new32(h + 0x170, NEW_CLUSTERS / 1000 * 1000 + 1);
    /* the indirect FAT: where each cluster of the FAT is */
    for (i = 0; i < NEW_FAT; i++)
        new32(h + NEW_RESERVED * 1024 + i * 4, NEW_RESERVED + NEW_IFC + i);
    /* the FAT: the root folder's one cluster, and everything else that can be allocated, free */
    new32(fat, FAT_END);
    for (i = 1; i < NEW_END; i++)
        new32(fat + i * 4, FAT_FREE);
    /* the root folder: "." (which says how many entries it has, 2) and ".." */
    memset(dir, 0, 1024);
    new16(dir, 0x8427);
    new32(dir + 4, 2);
    memcpy(dir + 8, when, 8);
    memcpy(dir + 24, when, 8);
    dir[64] = '.';
    new16(dir + 512, 0xA426);
    memcpy(dir + 512 + 8, when, 8);
    memcpy(dir + 512 + 24, when, 8);
    dir[512 + 64] = dir[512 + 65] = '.';
}

int mcfs_new_card(const char *path, void (*progress)(long long done, long long total))
{
    unsigned char *piece = malloc(NEW_PIECE), *back = malloc(NEW_HEAD);
    long long at;
    int fd, done, k = 0, r = MCFS_OK;
    if (!piece || !back) {
        free(piece);
        free(back);
        return MCFS_ERR_IO;
    }
    /* a piece at a time, the file open only while one goes into it (the way a whole card is written when a backup is
     * restored) */
    for (at = 0; at < NEW_SIZE && r == MCFS_OK; at += NEW_PIECE) {
        memset(piece, 0xFF, NEW_PIECE);
        if (!at)
            new_head(piece);
        if ((fd = open(path, at ? O_WRONLY : O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
            r = MCFS_ERR_IO;
            break;
        }
        if (at)
            lseek(fd, (long)at, SEEK_SET);
        for (done = 0; done < NEW_PIECE; done += k)
            if ((k = write(fd, piece + done, NEW_PIECE - done > 64 * 1024 ? 64 * 1024 : NEW_PIECE - done)) <= 0)
                break;
        if (close(fd) < 0 || done < NEW_PIECE)
            r = MCFS_ERR_IO;
        if (progress)
            progress(at + NEW_PIECE, NEW_SIZE);
    }
    /* what isn't 0xFF is read back, and the size is looked at */
    if (r == MCFS_OK) {
        long size = -1;
        new_head(piece);
        if ((fd = open(path, O_RDONLY)) >= 0) {
            for (done = 0; done < NEW_HEAD; done += k)
                if ((k = read(fd, back + done, NEW_HEAD - done)) <= 0)
                    break;
            size = lseek(fd, 0, SEEK_END);
            close(fd);
            if (done < NEW_HEAD || memcmp(back, piece, NEW_HEAD) != 0 || size != NEW_SIZE)
                r = MCFS_ERR_CHECK;
        } else
            r = MCFS_ERR_IO;
    }
    free(piece);
    free(back);
    log_msg("mcfs: new card %s: %d", path, r);
    return r;
}
