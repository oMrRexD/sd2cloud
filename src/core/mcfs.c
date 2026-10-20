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
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include "common.h"

/* a memory card in a slot of the console (mcfs_slot): the functions that take a card's path send it there */
static int slot_list(int port, mcfs_save_t *list, int max, long long *freeBytes);
static int slot_icon(int port, const mcfs_save_t *s, buffer_t *iconsys, buffer_t *ico);
static int slot_app(int port, const mcfs_save_t *s, char *boot, size_t size);
static int slot_info(int port, const char *folder, long long *bytes, int *files);
static int slot_copy(const char *from, const char *folder, const char *to, mcfs_step_cb progress);
static int slot_delete(int port, const char *folder);
static int slot_export(int port, const char *folder, buffer_t *out);
static int slot_import(const char *psu, int port);
static int ext_port(const char *path);

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

/* why a card couldn't be read, the last time one couldn't: for the log ("" = it could) */
static char lastError[100];
const char *mcfs_last_error(void) { return lastError; }

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

/* n bytes from byte at. The device can be busy for a moment (reading a card it was told to take into its memory, or
 * writing the one in use), and a read then fails in the middle of cards that are whole: a failed read is tried again,
 * a little later each time, before the card counts as unreadable. 0 = read */
#define READ_TRIES 4
static int read_at(int fd, long at, unsigned char *out, unsigned int n)
{
    int t;
    for (t = 0; t < READ_TRIES; t++) {
        if (t)
            sleep_ms(150 << t);   /* 300, 600, 1200 ms */
        if (lseek(fd, at, SEEK_SET) == at && read_all(fd, out, n) == 0)
            return 0;
    }
    return -1;
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
    if (!m->spare) {
        if (read_at(m->fd, (long)n * m->csz, out, m->csz) != 0) {
            snprintf(lastError, sizeof(lastError), "cluster %u (byte %lu) not read, errno %d", n, (unsigned long)n * m->csz, errno);
            return -1;
        }
        return 0;
    }
    if (read_at(m->fd, (long)n * pages * stride, raw, pages * stride) != 0)
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
        if (e == 0xFFFFFFFF || !(e & 0x80000000) || ++steps > m->clusters) {
            if (!lastError[0])
                snprintf(lastError, sizeof(lastError), "a folder's chain ends at cluster %u (FAT says %08X), %u of %u entries read", c, e, k, n);
            return -1;   /* the folder claims more entries than its chain has */
        }
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
    lastError[0] = 0;
    if (image && flags != O_RDONLY)
        return -1;
    if (image && imageMem) {
        if (imageLen < sizeof(sb))
            return -1;
        memcpy(sb, imageMem, sizeof(sb));
        m->mem = imageMem;
        m->memLen = imageLen;
    } else {
        i = 0;   /* (a file that isn't there isn't waited for: only a device that may be busy, see read_at) */
        while ((m->fd = open(image ? imageFile : path, flags)) < 0 && errno != ENOENT && ++i < READ_TRIES)
            sleep_ms(150 << i);
        if (m->fd < 0) {
            snprintf(lastError, sizeof(lastError), "not opened (%d, errno %d)", m->fd, errno);
            return -1;
        }
        if (read_at(m->fd, 0, sb, sizeof(sb)) != 0) {
            snprintf(lastError, sizeof(lastError), "its first %u bytes not read (errno %d)", (unsigned)sizeof(sb), errno);
            goto bad;
        }
        ioReads--;   /* (what the log counts is the card's clusters read, not its superblock) */
    }
    if (memcmp(sb, MAGIC, sizeof(MAGIC) - 1) != 0) {
        snprintf(lastError, sizeof(lastError), "not a PS2 memory card (it starts with %02X %02X %02X %02X)", sb[0], sb[1], sb[2], sb[3]);
        goto bad;
    }
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

/* a time as a card keeps it (reserved, sec, min, hour, day, month, year in 16 bits) as a number that grows with it */
static unsigned long long when_of(const unsigned char *t)
{
    return ((unsigned long long)le16(t + 6) << 40) | ((unsigned long long)t[5] << 32) | ((unsigned long long)t[4] << 24) |
           (t[3] << 16) | (t[2] << 8) | t[1];
}

static unsigned long long mtime(const unsigned char *e) { return when_of(e + 24); }

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
    int rootdir, r = -1;
    if (ext_port(path) >= 0)
        return slot_list(ext_port(path), list, max, freeBytes);
    rootdir = mc_open(&mc, path);
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

/* the file a title.cfg's "boot" line names ("" = none) */
static void cfg_boot(const buffer_t *cfg, char name[64])
{
    char *line, *end;
    name[0] = 0;
    for (line = (char *)cfg->data; line && *line && !name[0]; line = end ? end + 1 : NULL) {
        end = strchr(line, '\n');
        if (!strncmp(line, "boot=", 5)) {   /* as OPL reads it: "boot", in lowercase, at the start of a line */
            size_t n = strcspn(line + 5, "\r\n");
            while (n && line[5 + n - 1] == ' ')
                n--;
            snprintf(name, 64, "%.*s", (int)(n < 63 ? n : 63), line + 5);
        }
    }
}

int mcfs_save_app(const char *path, const mcfs_save_t *s, char *boot, size_t size)
{
    buffer_t cfg = {0};
    char name[64] = "";
    find_name_t f = {name, boot, size};
    boot[0] = 0;
    if (ext_port(path) >= 0)
        return slot_app(ext_port(path), s, boot, size);
    if (mc_open(&mc, path) < 0)
        return 0;
    if (read_file(&mc, s, "title.cfg", &cfg) == 0 && cfg.len < 16 * 1024) {
        cfg_boot(&cfg, name);
        if (name[0])
            walk_dir(&mc, s->cluster, s->count, on_find_name, &f);
    }
    mc_close();
    buf_free(&cfg);
    return boot[0] != 0;
}

typedef struct {
    unsigned long long sum;
    int n, bad;
} signature_t;

/* (each file's name, size and contents, hashed into one number; the files' numbers are added up, so that their
 * order in the folder doesn't matter) */
static int on_signature(mc_t *m, const unsigned char *e, void *u)
{
    signature_t *g = u;
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 sha;
    buffer_t b = {0};
    unsigned long long v = 0;
    unsigned int length = le32(e + 4);
    int i;
    if (!is_live(e) || g->bad)
        return 0;
    wc_InitSha256(&sha);
    wc_Sha256Update(&sha, e + 64, strnlen((const char *)e + 64, 32));
    if (!(le16(e) & DF_DIRECTORY)) {
        wc_Sha256Update(&sha, e + 4, 4);
        if (length && (read_chain(m, le32(e + 16), length, &b) != 0 || b.len != length))
            g->bad = 1;   /* (a file that can't be read: the save is told from none) */
        else if (length)
            wc_Sha256Update(&sha, b.data, b.len);
        buf_free(&b);
    }
    wc_Sha256Final(&sha, h);
    for (i = 0; i < 8; i++)
        v = v << 8 | h[i];
    g->sum += v;
    g->n++;
    return 0;
}

int mcfs_save_signatures(const char *path, const mcfs_save_t *saves, int n, unsigned long long *sig)
{
    int i;
    for (i = 0; i < n; i++)
        sig[i] = 0;
    if (mc_open(&mc, path) < 0)
        return -1;
    for (i = 0; i < n; i++) {
        signature_t g = {0, 0, 0};
        if (walk_dir(&mc, saves[i].cluster, saves[i].count, on_signature, &g) == 0 && !g.bad) {
            sig[i] = g.sum + (unsigned long long)g.n;
            if (!sig[i])
                sig[i] = 1;
        }
    }
    mc_close();
    return 0;
}

int mcfs_save_icon(const char *path, const mcfs_save_t *s, buffer_t *iconsys, buffer_t *ico)
{
    int r;
    if (ext_port(path) >= 0)
        return slot_icon(ext_port(path), s, iconsys, ico);
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

int mcfs_newest_save_icon(const char *path, int (*rank)(const char *folder), char folder[33], buffer_t *iconsys, buffer_t *ico)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    static unsigned char ranks[MCFS_MAX_SAVES];
    list_ctx_t x = {list, 0, MCFS_MAX_SAVES};
    int rootdir, r = -1, i, top, port = ext_port(path);
    if (port >= 0) {   /* (the Library, or a memory card in a slot: its saves listed, the newest first) */
        int n = slot_list(port, list, MCFS_MAX_SAVES, NULL);
        for (i = 0, top = 0; i < n; i++)
            if ((ranks[i] = rank ? rank(list[i].folder) : 0) > top)
                top = ranks[i];
        for (; top >= 0 && r != 0; top--)
            for (i = 0; i < n && r != 0; i++)
                if (ranks[i] == top && slot_icon(port, &list[i], iconsys, ico) == 0) {
                    snprintf(folder, 33, "%s", list[i].folder);
                    r = 0;
                }
        return r;
    }
    if ((rootdir = mc_open(&mc, path)) < 0)
        return -1;
    /* the newest save that has an icon (a folder without icon.sys, like some games' extra data, is skipped), of the
     * highest rank there is */
    if (walk_root(&mc, rootdir, on_save_dir, &x) == 0) {
        qsort(list, x.n, sizeof(list[0]), newer_first);
        for (i = 0, top = 0; i < x.n; i++)
            if ((ranks[i] = rank ? rank(list[i].folder) : 0) > top)
                top = ranks[i];
        for (; top >= 0 && r != 0; top--)
            for (i = 0; i < x.n && r != 0; i++)
                if (ranks[i] == top && read_icon(&mc, &list[i], iconsys, ico) == 0) {
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
    if (ext_port(path) >= 0)
        return slot_info(ext_port(path), folder, bytes, files);
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
    if (ext_port(from) >= 0 || ext_port(to) >= 0)   /* (to or from a memory card in a slot, or the Library) */
        return slot_copy(from, folder, to, progress);
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

/* ------------------------------------------------------------ a whole card into another one */

static struct {
    char name[MAX_ROOT][33];
    int n, other;   /* other: something at the root that isn't a folder */
} rootList;

static int on_root_folder(mc_t *m, const unsigned char *e, void *u)
{
    (void)m;
    (void)u;
    if (!is_live(e))
        return 0;
    if (!(le16(e) & DF_DIRECTORY))
        rootList.other++;
    else if (rootList.n < MAX_ROOT)
        snprintf(rootList.name[rootList.n++], 33, "%.32s", (const char *)e + 64);
    return 0;
}

long long mcfs_card_used(const char *path)
{
    long long used = -1;
    if (mc_open(&mc, path) < 0)
        return -1;
    if (mc.allocEnd > 1)   /* (the root folder's own cluster is no part of it: a new card has it too) */
        used = (long long)(mc.allocEnd - free_clusters(&mc) - 1) * mc.csz;
    mc_close();
    return used;
}

/* ------------------------------------------------------------ a card looked through */

typedef struct {
    mcfs_check_t *r;
    unsigned char *seen;   /* a bit per cluster: a folder or a file has it */
    int depth;
} check_t;

/* the n clusters of a chain from c, as one folder's or file's: each one taken as theirs. 0 = the chain is whole */
static int check_chain(mc_t *m, check_t *k, unsigned int c, unsigned int n)
{
    unsigned int steps = 0, e;
    while (n) {
        if (c >= m->allocEnd)
            return -1;
        e = fat(m, c);
        if (!(e & FAT_ALLOC))
            return -1;   /* (a cluster the FAT says is free) */
        if (k->seen[c / 8] & (1 << (c % 8)))
            k->r->crossLinked++;
        else {
            k->seen[c / 8] |= 1 << (c % 8);
            k->r->used++;
        }
        if (!--n)
            break;
        if (e == FAT_END || ++steps > m->clusters)
            return -1;   /* (it ends before they do, or goes round) */
        c = e & ~FAT_ALLOC;
    }
    return 0;
}

static int on_check_entry(mc_t *m, const unsigned char *e, void *u);

/* a folder (its first cluster, how many entries it has): its chain, then each of its entries */
static int check_folder(mc_t *m, check_t *k, unsigned int c0, unsigned int n)
{
    unsigned int need = (n * 512 + m->csz - 1) / m->csz;
    k->r->folders++;
    if (!n || n > m->clusters * (m->csz / 512) || check_chain(m, k, c0, need) != 0) {
        k->r->badChains++;
        return 0;   /* (its entries can't be gone through: the rest of the card can) */
    }
    if (k->depth >= 8)
        return 0;
    k->depth++;
    if (walk_dir(m, c0, n, on_check_entry, k) != 0)
        k->r->badChains++;
    k->depth--;
    return 0;
}

static int on_check_entry(mc_t *m, const unsigned char *e, void *u)
{
    check_t *k = u;
    const char *name = (const char *)e + 64;
    unsigned int length = le32(e + 4), c = le32(e + 16);
    if (!(le16(e) & DF_EXISTS) || !strcmp(name, ".") || !strcmp(name, ".."))
        return 0;
    if (!name[0] || !memchr(name, 0, 32)) {
        k->r->badEntries++;
        return 0;
    }
    if (le16(e) & DF_DIRECTORY)
        return check_folder(m, k, c, length);
    k->r->files++;
    if (length && check_chain(m, k, c, (length + m->csz - 1) / m->csz) != 0)
        k->r->badChains++;
    return 0;
}

int mcfs_check(const char *path, mcfs_check_t *r)
{
    check_t k;
    const unsigned char *root;
    int rootdir;
    unsigned int n;
    memset(r, 0, sizeof(*r));
    if (ext_port(path) >= 0 || (rootdir = mc_open(&mc, path)) < 0)
        return -1;
    memset(&k, 0, sizeof(k));
    k.r = r;
    r->clusters = mc.allocEnd;
    if (!(k.seen = calloc(mc.allocEnd / 8 + 1, 1)) || !(root = cluster(&mc, rootdir + mc.alloc))) {
        free(k.seen);
        mc_close();
        return -1;
    }
    check_folder(&mc, &k, rootdir, le32(root + 4));
    r->folders--;   /* (the root isn't one of its folders) */
    for (n = 0; n < mc.allocEnd; n++)   /* in use by the FAT, and nobody's */
        if ((fat(&mc, n) & FAT_ALLOC) && !(k.seen[n / 8] & (1 << (n % 8))))
            r->lost++;
    free(k.seen);
    log_msg("mcfs: %s checked: %d folder(s), %d file(s), %u of %u clusters in use; %d broken chain(s), %d cross-linked, "
            "%d lost, %d bad entr(ies)", path, r->folders, r->files, r->used, r->clusters, r->badChains, r->crossLinked,
            r->lost, r->badEntries);
    mc_close();
    return 0;
}

int mcfs_copy_card(const char *from, const char *to, int (*progress)(int i, int n, const char *folder))
{
    int rootdir = mc_open(&mc, from), r = MCFS_OK, i;
    if (rootdir < 0)
        return MCFS_ERR_IO;
    rootList.n = rootList.other = 0;
    if (walk_root(&mc, rootdir, on_root_folder, NULL) != 0)
        r = MCFS_ERR_IO;
    mc_close();
    if (r == MCFS_OK && rootList.other) {
        log_msg("mcfs: %s has %d file(s) at its root, not in a folder: not copied whole", from, rootList.other);
        r = MCFS_ERR_BAD;
    }
    for (i = 0; r == MCFS_OK && i < rootList.n; i++) {
        if (progress && progress(i, rootList.n, rootList.name[i]))
            r = MCFS_ERR_CANCELLED;
        else
            r = mcfs_copy_save(from, rootList.name[i], to, NULL);
    }
    if (r == MCFS_OK && progress)
        progress(rootList.n, rootList.n, "");
    log_msg("mcfs: %s into %s: %d folder(s), %d", from, to, rootList.n, r);
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
    if (ext_port(path) >= 0)
        return slot_delete(ext_port(path), folder);
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
/* a save as it was read (s), as a .psu */
static int psu_of(const save_raw_t *s, buffer_t *out)
{
    static const unsigned char pad[1024];
    static unsigned char e[ENT];
    int i;
    for (i = -1; i < s->n; i++) {
        unsigned int len;
        memcpy(e, i < 0 ? s->root : s->ent[i], ENT);
        if (i < 0)
            put32(e + 4, s->n);
        put32(e + 16, 0);
        put32(e + 20, 0);
        if (buf_append(out, e, ENT))
            return MCFS_ERR_IO;
        if (i < 2 || (le16(e) & DF_DIRECTORY))
            continue;
        len = le32(e + 4);
        if ((len && buf_append(out, s->data[i].data, len)) || (len % 1024 && buf_append(out, pad, 1024 - len % 1024)))
            return MCFS_ERR_IO;
    }
    return MCFS_OK;
}

int mcfs_export_psu(const char *path, const char *folder, buffer_t *out)
{
    int r;
    buf_free(out);
    if (ext_port(path) >= 0)
        return slot_export(ext_port(path), folder, out);
    if (mc_open(&mc, path) < 0)
        return MCFS_ERR_IO;
    r = read_save(&mc, folder, &srcSave);
    mc_close();
    if (r == MCFS_OK)
        r = psu_of(&srcSave, out);
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

/* a date a device gave that is a date: PCSX2's host: gives the year as years since 1900 in one byte, the next one left
 * as it was, and an sd2psx gives what its firmware sends */
static int date_ok(const unsigned char *t)
{
    unsigned int year = le16(t + 6);
    return year >= 1980 && year <= 2099 && t[5] >= 1 && t[5] <= 12 && t[4] >= 1 && t[4] <= 31 && t[3] < 24 && t[2] < 60 &&
           t[1] < 60;
}

/* A save copied as a folder: its files as they were on the card, the way a file manager copies a save from a memory
 * card to a USB drive. Read as a .psu is (path ends in /), its entries made from the files and their dates, the
 * folder's own from the newest of them; checked the same way: names a card can hold, nothing in it but files */
static int read_folder(const char *path, save_raw_t *s)
{
    static dir_entry_t list[MAX_ENTRIES];
    char file[700];
    unsigned char now[8];
    const unsigned char *newest = NULL;
    const char *end = path + strlen(path) - 1, *name = end;
    long long total = 0;
    int n = dir_list(path, list, MAX_ENTRIES), i, k;
    s->n = 0;
    if (n < 0)
        return MCFS_ERR_IO;
    for (i = k = 0; i < n; i++)   /* what a computer leaves in a folder it opened isn't the save's */
        if (strncmp(list[i].name, "._", 2) && strcasecmp(list[i].name, ".DS_Store") && strcasecmp(list[i].name, "Thumbs.db") &&
            strcasecmp(list[i].name, "desktop.ini"))
            list[k++] = list[i];
    n = k;
    while (name > path && name[-1] != '/' && name[-1] != ':')
        name--;
    if (!n || n > MAX_ENTRIES - 2 || end - name < 1 || end - name > 32)
        return MCFS_ERR_BAD;
    for (i = 0; i < n; i++) {
        if (list[i].dir || list[i].size > 0x7FFFFFFF || strlen(list[i].name) > 32)
            return MCFS_ERR_BAD;
        total += list[i].size;
        if (date_ok(list[i].mtime) && (!newest || when_of(list[i].mtime) > when_of(newest)))
            newest = list[i].mtime;
    }
    if (!newest) {   /* a device that gives no dates (or none that are): the save is dated now */
        card_time_now(now);
        newest = now;
    }
    step_phase(MCFS_STEP_READ, total);
    memset(s->root, 0, ENT);
    s->root[0] = 0x27, s->root[1] = 0x84;   /* a folder that is there, as a save's is on a card */
    put32(s->root + 4, n + 2);
    memcpy(s->root + 8, newest, 8);
    memcpy(s->root + 24, newest, 8);
    memcpy(s->root + 64, name, end - name);
    if (!name_ok(s->root))
        return MCFS_ERR_BAD;
    for (i = 0; i < 2; i++) {   /* "." and "..": where they point is set when the save goes into a card */
        memcpy(s->ent[i], s->root, ENT);
        put32(s->ent[i] + 4, 0);
        memset(s->ent[i] + 64, 0, 32);
        strcpy((char *)s->ent[i] + 64, i ? ".." : ".");
        buf_free(&s->data[i]);
    }
    s->n = 2;
    for (i = 0; i < n; i++) {
        unsigned char *e = s->ent[s->n];
        const unsigned char *t = date_ok(list[i].mtime) ? list[i].mtime : newest;
        memset(e, 0, ENT);
        e[0] = 0x97, e[1] = 0x84;   /* a file that is there */
        put32(e + 4, (unsigned int)list[i].size);
        memcpy(e + 8, t, 8);
        memcpy(e + 24, t, 8);
        memcpy(e + 64, list[i].name, strlen(list[i].name));
        snprintf(file, sizeof(file), "%s%s", path, list[i].name);
        buf_free(&s->data[s->n]);
        if (!name_ok(e) || file_read(file, &s->data[s->n]) != 0 || (long long)s->data[s->n].len != list[i].size) {
            save_free(s);
            return name_ok(e) ? MCFS_ERR_IO : MCFS_ERR_BAD;
        }
        s->n++;
        if (step(list[i].size)) {
            save_free(s);
            return MCFS_ERR_CANCELLED;
        }
    }
    return MCFS_OK;
}

/* the .psu as a save read from a card (srcSave), checked whole before anything is written anywhere: the sizes
 * against the file's own, the names, no folder inside it, no name twice. The file is closed again before this
 * returns: on the sd2psx it must not be open while a card is. A path ending in / is a save copied as a folder */
static const unsigned char *psuMem;   /* a .psu held in memory (MCFS_PSU_MEMORY) */
static size_t psuMemLen, psuAt;

void mcfs_psu_memory(const unsigned char *d, size_t len)
{
    psuMem = d;
    psuMemLen = len;
}

/* (read and lseek, on a .psu's file or on the one held in memory: fd = -2) */
static int psu_read(int fd, void *d, size_t n)
{
    if (fd != -2)
        return read(fd, d, n);
    if (psuAt >= psuMemLen)
        return 0;
    if (n > psuMemLen - psuAt)
        n = psuMemLen - psuAt;
    memcpy(d, psuMem + psuAt, n);
    psuAt += n;
    return (int)n;
}

static long psu_seek(int fd, long at)
{
    if (fd != -2)
        return lseek(fd, at, SEEK_SET);
    psuAt = (size_t)at;
    return at;
}

static int read_psu(const char *path, save_raw_t *s)
{
    long size, at = ENT;
    int fd, r = MCFS_ERR_BAD, i, k, n;
    s->n = 0;
    if (path[0] && path[strlen(path) - 1] == '/')
        return read_folder(path, s);
    if (!strcmp(path, MCFS_PSU_MEMORY)) {
        if (!psuMem)
            return MCFS_ERR_IO;
        fd = -2;
        psuAt = 0;
        size = (long)psuMemLen;
    } else {
        if ((fd = open(path, O_RDONLY)) < 0)
            return MCFS_ERR_IO;
        size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
    }
    step_phase(MCFS_STEP_READ, size);
    if (size < 3 * ENT || psu_read(fd, s->root, ENT) != ENT)
        goto out;
    n = (int)le32(s->root + 4);
    if (!(le16(s->root) & DF_DIRECTORY) || !name_ok(s->root) || n < 2 || n > MAX_ENTRIES)
        goto out;
    s->root[1] |= DF_EXISTS >> 8;
    for (i = 0; i < n; i++) {
        unsigned char *e = s->ent[i];
        unsigned int len, done;
        if (at + ENT > size || psu_read(fd, e, ENT) != ENT)
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
                int got = psu_read(fd, s->data[i].data + done, len - done > 64 * 1024 ? 64 * 1024 : len - done);
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
        if (len % 1024 && at <= size && psu_seek(fd, at) < 0)
            goto out;
    }
    r = MCFS_OK;
out:
    if (fd >= 0)
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

int mcfs_psu_bytes(const char *psu, buffer_t *out)
{
    int r = MCFS_OK;
    buf_free(out);
    if (!strcmp(psu, MCFS_PSU_MEMORY))
        return psuMem && !buf_append(out, psuMem, psuMemLen) ? MCFS_OK : MCFS_ERR_IO;
    if (psu[0] && psu[strlen(psu) - 1] == '/') {   /* (a save copied as a folder: made into a .psu) */
        if ((r = read_psu(psu, &srcSave)) == MCFS_OK)
            r = psu_of(&srcSave, out);
        save_free(&srcSave);
        return r;
    }
    return file_read(psu, out) == 0 ? MCFS_OK : MCFS_ERR_IO;
}

int mcfs_psu_files(const char *psu, mcfs_psu_cb cb, void *u)
{
    int r = read_psu(psu, &srcSave), i;
    if (r == MCFS_OK)
        r = cb(srcSave.root, NULL, 0, u);
    for (i = 2; r == 0 && i < srcSave.n; i++)
        if (!(le16(srcSave.ent[i]) & DF_DIRECTORY))
            r = cb(srcSave.ent[i], srcSave.data[i].data, (unsigned int)srcSave.data[i].len, u);
    save_free(&srcSave);
    return r;
}

int mcfs_import_psu(const char *psu, const char *to, mcfs_step_cb progress)
{
    u64 start = now_ms();
    int r;
    if (ext_port(to) >= 0)
        return slot_import(psu, ext_port(to));
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

   An empty, formatted card of any size a memory card can be, laid out the way the PS2's own mcman formats one (the
   ps2sdk's mcman_format2) and the sd2psx makes one when it is asked for a card that isn't there yet (its firmware's
   ps2_cardman.c: build_superblock, blockRoot, genblock). Clusters of 1 KB: 16 kept for the superblock, then the
   indirect FAT, then the FAT, and from there on what can be allocated, the root folder first, up to the two blocks
   kept as backup at the end. Everything else is 0xFF. An 8 MB one is byte for byte the sd2psx's: 8192 clusters, the
   indirect FAT in cluster 16, the FAT in the 32 after it, the root folder in cluster 49. */

#define NEW_RESERVED 16
#define NEW_PIECE    (512 * 1024)

typedef struct {
    unsigned int clusters, fat, ifc, alloc, backup1, backup2, end, maxAlloc;
    size_t head;   /* through the root folder's cluster: all that isn't 0xFF */
} new_layout_t;

/* 0 = that isn't a size a memory card can be (a power of two from 1 to 128 MB) */
static int new_layout(long long size, new_layout_t *l)
{
    long long mb = size / (1024 * 1024);
    if (size % (1024 * 1024) || mb < MCFS_MIN_MB || mb > MCFS_MAX_MB || (mb & (mb - 1)))
        return 0;
    l->clusters = (unsigned int)(size / 1024);
    l->fat = (l->clusters * 4 + 1023) / 1024;
    l->ifc = (l->fat * 4 + 1023) / 1024;
    l->alloc = NEW_RESERVED + l->ifc + l->fat;
    l->backup1 = l->clusters / 8 - 1;
    l->backup2 = l->backup1 - 1;
    l->end = l->backup2 * 8 - l->alloc;
    /* what mcman writes there: where the first clusters / 1000 * 1000 + 1 clusters that can be allocated end, or,
     * when the card has fewer than that (only a 1 MB one), where the backup blocks start */
    l->maxAlloc = l->clusters / 1000 * 1000 + 1;
    if (l->maxAlloc > l->end)
        l->maxAlloc = l->backup2 * 8;
    l->head = (size_t)(l->alloc + 1) * 1024;
    return 1;
}

long long mcfs_card_room(long long size)
{
    new_layout_t l;
    unsigned int n;
    if (!new_layout(size, &l))
        return 0;
    /* (the PS2 counts no more than those clusters/1000*1000+1 as free, the root folder's one among them: an 8 MB
     * card has 8000 KB for saves) */
    n = l.clusters / 1000 * 1000 + 1;
    if (n > l.end)
        n = l.end;
    return (long long)(n - 1) * 1024;
}

long long mcfs_save_room(long long bytes, int files)
{
    /* its folder's entries (two in a cluster, "." and ".." among them), its files (none takes less than a whole
     * cluster, so each may waste up to one), and its entry in the root, which may take a cluster of its own */
    return ((long long)(files + 2 + 1) / 2 + bytes / 1024 + files + 1) * 1024;
}

int mcfs_recommended_mb(long long room, int minMb, int maxMb)
{
    int mb;
    /* the smallest one the saves take no more than half of, for the game to have room to save more; else the smallest
     * one they fit in */
    for (mb = minMb; mb <= maxMb; mb *= 2)
        if (room * 2 <= mcfs_card_room((long long)mb << 20))
            return mb;
    for (mb = minMb; mb <= maxMb; mb *= 2)
        if (room <= mcfs_card_room((long long)mb << 20))
            return mb;
    return 0;
}

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

/* the start of that card, l->head bytes */
static void new_head(const new_layout_t *l, unsigned char *h)
{
    /* the time the sd2psx gives the root folder: 2000-01-12 06:00:41 */
    static const unsigned char when[8] = {0x00, 0x29, 0x00, 0x06, 0x0C, 0x01, 0xD0, 0x07};
    unsigned char *fat = h + (NEW_RESERVED + l->ifc) * 1024, *dir = h + l->alloc * 1024;
    unsigned int i;
    memset(h, 0xFF, l->head);
    /* the superblock */
    memset(h, 0, 0xD0);
    memcpy(h, MAGIC, 28);
    memcpy(h + 0x1C, "1.2.0.0", 7);
    new16(h + 0x28, 512);              /* a page */
    new16(h + 0x2A, 2);                /* pages in a cluster */
    new16(h + 0x2C, 16);               /* pages in an erase block */
    new16(h + 0x2E, 0xFF00);
    new32(h + 0x30, l->clusters);
    new32(h + 0x34, l->alloc);
    new32(h + 0x38, l->end);
    new32(h + 0x3C, 0);                /* the root folder's cluster, counted from where the allocatable area starts */
    new32(h + 0x40, l->backup1);
    new32(h + 0x44, l->backup2);
    for (i = 0; i < l->ifc; i++)
        new32(h + 0x50 + i * 4, NEW_RESERVED + i);
    memset(h + 0x150, 0, 0x2C);
    h[0x150] = 2;                      /* a PS2 card */
    h[0x151] = 0x2B;                   /* what it can do */
    new32(h + 0x154, 1024);            /* a cluster */
    new32(h + 0x158, 256);             /* FAT entries in a cluster */
    new32(h + 0x15C, 8);               /* clusters in a block */
    new32(h + 0x160, 0xFFFFFFFFu);
    new32(h + 0x170, l->maxAlloc);
    /* the indirect FAT: where each cluster of the FAT is */
    for (i = 0; i < l->fat; i++)
        new32(h + NEW_RESERVED * 1024 + i * 4, NEW_RESERVED + l->ifc + i);
    /* the FAT: the root folder's one cluster, and everything else that can be allocated, free */
    new32(fat, FAT_END);
    for (i = 1; i < l->end; i++)
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

int mcfs_new_card(const char *path, long long size, void (*progress)(long long done, long long total))
{
    new_layout_t l;
    unsigned char *piece, *head, *back;
    long long at;
    int fd, done, k = 0, r = MCFS_OK;
    if (!new_layout(size, &l))
        return MCFS_ERR_BAD;
    piece = malloc(NEW_PIECE), head = malloc(l.head), back = malloc(l.head);
    if (!piece || !head || !back) {
        free(piece);
        free(head);
        free(back);
        return MCFS_ERR_IO;
    }
    new_head(&l, head);
    /* a piece at a time, the file open only while one goes into it (the way a whole card is written when a backup is
     * restored) */
    for (at = 0; at < size && r == MCFS_OK; at += NEW_PIECE) {
        memset(piece, 0xFF, NEW_PIECE);
        if ((size_t)at < l.head)   /* (a big card's start goes on into the next piece) */
            memcpy(piece, head + at, l.head - at < NEW_PIECE ? l.head - at : NEW_PIECE);
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
            progress(at + NEW_PIECE, size);
    }
    /* what isn't 0xFF is read back, and the size is looked at */
    if (r == MCFS_OK) {
        long end = -1;
        if ((fd = open(path, O_RDONLY)) >= 0) {
            for (done = 0; (size_t)done < l.head; done += k)
                if ((k = read(fd, back + done, l.head - done)) <= 0)
                    break;
            end = lseek(fd, 0, SEEK_END);
            close(fd);
            if ((size_t)done < l.head || memcmp(back, head, l.head) != 0 || end != size)
                r = MCFS_ERR_CHECK;
        } else
            r = MCFS_ERR_IO;
    }
    free(piece);
    free(head);
    free(back);
    log_msg("mcfs: new card %s of %lld MB: %d", path, size >> 20, r);
    return r;
}

/* ------------------------------------------------------------ a memory card in a slot of the console

   Not a file: mcman's own calls read and write it (mcfs_slot, the platform's), by its files and folders. Its saves are
   listed from its root's entries, and a save is read whole into srcSave, its entries as the card has them (their
   modes and dates), to go into a card's file the way a save of another card does. What goes into it goes as a .psu,
   which mcman writes file by file. */

static const mcfs_slot_t *slotOps, *libOps;
static unsigned char slotRoot[MAX_ROOT][ENT], slotDir[MAX_ENTRIES][ENT];
static char libScope[16];   /* (what followed "lib:" in the path the Library was asked by) */
#define LIB_PORT 2

void mcfs_slot(const mcfs_slot_t *ops) { slotOps = ops; }
void mcfs_library(const mcfs_slot_t *ops) { libOps = ops; }
const char *mcfs_library_scope(void) { return libScope; }

int mcfs_slot_port(const char *path)
{
    return path && path[0] == 'm' && path[1] == 'c' && (path[2] == '0' || path[2] == '1') && path[3] == ':' && !path[4]
               ? path[2] - '0' : -1;
}

/* a slot's port (0, 1), LIB_PORT for the Library (its scope kept), or -1 for a card's file */
static int ext_port(const char *path)
{
    if (library_path(path)) {
        snprintf(libScope, sizeof(libScope), "%s", path + sizeof(LIBRARY_PATH) - 1);
        return LIB_PORT;
    }
    return mcfs_slot_port(path);
}

/* the calls of that port's storage (NULL = none is there) */
static const mcfs_slot_t *ops_of(int port) { return port == LIB_PORT ? libOps : slotOps; }

static int slot_list(int port, mcfs_save_t *list, int max, long long *freeBytes)
{
    int n, i, k = 0;
    if (!ops_of(port) || (n = ops_of(port)->dir(port, NULL, slotRoot, MAX_ROOT)) < 0)
        return -1;
    for (i = 0; i < n && k < max; i++) {
        const unsigned char *e = slotRoot[i];
        if (!is_live(e) || !(le16(e) & DF_DIRECTORY) || is_system_dir((const char *)e + 64))
            continue;
        memset(&list[k], 0, sizeof(list[0]));
        snprintf(list[k].folder, sizeof(list[0].folder), "%.32s", (const char *)e + 64);
        list[k].when = mtime(e);
        k++;
    }
    if (list)
        qsort(list, k, sizeof(list[0]), newer_first);
    if (freeBytes)
        *freeBytes = ops_of(port)->free_bytes(port);
    return k;
}

static int slot_icon(int port, const mcfs_save_t *s, buffer_t *iconsys, buffer_t *ico)
{
    char icoName[65];
    buf_free(iconsys);
    buf_free(ico);
    if (!ops_of(port) || ops_of(port)->read(port, s->folder, "icon.sys", iconsys) != 0 || iconsys->len < 964 ||
        memcmp(iconsys->data, "PS2D", 4) != 0) {
        buf_free(iconsys);
        return -1;
    }
    snprintf(icoName, sizeof(icoName), "%.64s", (const char *)iconsys->data + 260);
    if (ops_of(port)->read(port, s->folder, icoName, ico) != 0 || !ico->len) {
        buf_free(iconsys);
        buf_free(ico);
        return -1;
    }
    return 0;
}

/* the save's own files, as entries (".", ".." and folders left out). -1 = it couldn't be read */
static int slot_files(int port, const char *folder)
{
    int n, i, k = 0;
    if (!ops_of(port) || (n = ops_of(port)->dir(port, folder, slotDir, MAX_ENTRIES)) < 0)
        return -1;
    for (i = 0; i < n; i++)
        if (is_live(slotDir[i]) && !(le16(slotDir[i]) & DF_DIRECTORY))
            memmove(slotDir[k++], slotDir[i], ENT);
    return k;
}

static int slot_app(int port, const mcfs_save_t *s, char *boot, size_t size)
{
    buffer_t cfg = {0};
    char name[64] = "";
    int n, i;
    boot[0] = 0;
    if (port != LIB_PORT && slotOps && slotOps->read(port, s->folder, "title.cfg", &cfg) == 0 && cfg.len < 16 * 1024)
        cfg_boot(&cfg, name);
    buf_free(&cfg);
    if (name[0] && (n = slot_files(port, s->folder)) > 0)
        for (i = 0; i < n && !boot[0]; i++)   /* (whatever its letters' case, as on a card's file) */
            if (le32(slotDir[i] + 4) && !strncasecmp((const char *)slotDir[i] + 64, name, 32))
                snprintf(boot, size, "%.32s", (const char *)slotDir[i] + 64);
    return boot[0] != 0;
}

static int slot_info(int port, const char *folder, long long *bytes, int *files)
{
    int n = slot_files(port, folder), i;
    if (n < 0)
        return MCFS_ERR_IO;
    for (i = 0; i < n; i++)
        *bytes += le32(slotDir[i] + 4), (*files)++;
    return MCFS_OK;
}

/* the save whole into s: its entry in the root, ".", ".." and its files, read; their bytes told to the progress */
static int slot_read(int port, const char *folder, save_raw_t *s)
{
    long long total = 0;
    int n, i, k;
    s->n = 0;
    if (!ops_of(port) || (n = ops_of(port)->dir(port, NULL, slotRoot, MAX_ROOT)) < 0)
        return MCFS_ERR_IO;
    for (i = 0; i < n && (!is_live(slotRoot[i]) || strncmp((const char *)slotRoot[i] + 64, folder, 32)); i++)
        ;
    if (i == n || !(le16(slotRoot[i]) & DF_DIRECTORY))
        return MCFS_ERR_NOT_FOUND;
    memcpy(s->root, slotRoot[i], ENT);
    if ((n = slot_files(port, folder)) < 0 || n > MAX_ENTRIES - 2)
        return MCFS_ERR_IO;
    for (i = 0; i < 2; i++) {   /* "." and "..": where they point is set when the save goes into a card */
        memcpy(s->ent[i], s->root, ENT);
        put32(s->ent[i] + 4, 0);
        memset(s->ent[i] + 64, 0, 32);
        strcpy((char *)s->ent[i] + 64, i ? ".." : ".");
    }
    for (i = 0; i < n; i++)
        total += le32(slotDir[i] + 4);
    step_phase(stepPhase, total);
    s->n = 2;
    for (i = 0; i < n; i++) {
        unsigned int len = le32(slotDir[i] + 4);
        char name[33];
        k = s->n++;
        memcpy(s->ent[k], slotDir[i], ENT);
        buf_free(&s->data[k]);
        snprintf(name, sizeof(name), "%.32s", (const char *)slotDir[i] + 64);
        if (len && (ops_of(port)->read(port, folder, name, &s->data[k]) != 0 || s->data[k].len != len)) {
            save_free(s);
            s->n = 0;
            return MCFS_ERR_IO;
        }
        if (step(len)) {
            save_free(s);
            s->n = 0;
            return MCFS_ERR_CANCELLED;
        }
    }
    put32(s->root + 4, s->n);
    return MCFS_OK;
}

/* a save from a memory card in a slot (or the Library) into a card's file (written as any copy is: read back and
 * compared), or from a card's file (or a card held in memory) into the memory card (or the Library), as a .psu; or from
 * one of those two into the other, as a .psu */
static int slot_copy(const char *from, const char *folder, const char *to, mcfs_step_cb progress)
{
    int fromPort = ext_port(from), toPort = ext_port(to), r, i;
    buffer_t psu = {0};
    u64 start = now_ms();
    char scope[sizeof(libScope)];
    if (fromPort >= 0 && toPort >= 0 && (fromPort == LIB_PORT) == (toPort == LIB_PORT))
        return MCFS_ERR_IO;   /* (there is one memory card besides the device's, and one Library) */
    ext_port(fromPort == LIB_PORT ? from : to);   /* (the Library's end: its scope, for its calls) */
    snprintf(scope, sizeof(scope), "%s", libScope);
    onStep = progress;
    stepStop = 0;
    step_phase(MCFS_STEP_READ, 0);
    if (fromPort >= 0 && toPort >= 0) {   /* the memory card and the Library: the save read from one, put into the other */
        r = slot_read(fromPort, folder, &srcSave);
        if (r == MCFS_OK)
            r = psu_of(&srcSave, &psu);
        save_free(&srcSave);
        if (r == MCFS_OK) {
            onStep = NULL;
            if (progress)
                progress(MCFS_STEP_CHECK, 0, (long long)psu.len);
            mcfs_psu_memory(psu.data, psu.len);
            snprintf(libScope, sizeof(libScope), "%s", scope);
            r = ops_of(toPort) ? ops_of(toPort)->put(toPort, MCFS_PSU_MEMORY) : MCFS_ERR_IO;
            mcfs_psu_memory(NULL, 0);
            if (progress)
                progress(MCFS_STEP_CHECK, (long long)psu.len, (long long)psu.len);
        }
        buf_free(&psu);
    } else if (fromPort >= 0) {
        r = slot_read(fromPort, folder, &srcSave);
        for (i = 2; r == MCFS_OK && i < srcSave.n; i++)
            if (le16(srcSave.ent[i]) & DF_DIRECTORY)
                r = MCFS_ERR_IO;
        if (r == MCFS_OK)
            r = write_save(to);
        save_free(&srcSave);
    } else {
        if (mc_open(&mc, from) < 0) {
            onStep = NULL;
            return MCFS_ERR_IO;
        }
        r = read_save(&mc, folder, &srcSave);
        mc_close();
        if (stepStop)
            r = MCFS_ERR_CANCELLED;
        if (r == MCFS_OK)
            r = psu_of(&srcSave, &psu);
        save_free(&srcSave);
        if (r == MCFS_OK) {
            /* mcman writes it and reads it back, file by file: nothing to give up any more (and what it reads of the
             * .psu, through these same functions, isn't the copy's progress) */
            onStep = NULL;
            if (progress)
                progress(MCFS_STEP_CHECK, 0, (long long)psu.len);
            mcfs_psu_memory(psu.data, psu.len);
            r = ops_of(toPort) ? ops_of(toPort)->put(toPort, MCFS_PSU_MEMORY) : MCFS_ERR_IO;
            mcfs_psu_memory(NULL, 0);
            if (progress)
                progress(MCFS_STEP_CHECK, (long long)psu.len, (long long)psu.len);
        }
        buf_free(&psu);
    }
    if (r == MCFS_OK && stepStop)
        r = MCFS_ERR_CANCELLED;
    onStep = NULL;
    log_msg("mcfs: copy of %s from %s to %s: %d in %u ms", folder, from, to, r, (unsigned)(now_ms() - start));
    return r;
}

static int slot_delete(int port, const char *folder)
{
    int r = ops_of(port) ? ops_of(port)->remove(port, folder) : MCFS_ERR_IO;
    log_msg("mcfs: %s deleted from %s: %d", folder, port == LIB_PORT ? "the Library" : port ? "mc1" : "mc0", r);
    return r;
}

static int slot_export(int port, const char *folder, buffer_t *out)
{
    int r = slot_read(port, folder, &srcSave);
    if (r == MCFS_OK)
        r = psu_of(&srcSave, out);
    save_free(&srcSave);
    if (r != MCFS_OK)
        buf_free(out);
    return r;
}

static int slot_import(const char *psu, int port)
{
    int r;
    onStep = NULL;
    r = ops_of(port) ? ops_of(port)->put(port, psu) : MCFS_ERR_IO;
    log_msg("mcfs: import of %s into %s: %d", psu, port == LIB_PORT ? "the Library" : port ? "mc1" : "mc0", r);
    return r;
}
