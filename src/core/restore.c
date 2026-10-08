/*
 * SD2Cloud -- restores a card from one of its backups on Drive, or from the .zip of a card in a folder of the microSD
 * or of a USB drive (the one "Copy to a device" writes).
 *
 * The zip is downloaded to memory (Drive tells its size) and inflated once only to check it: the SHA-256 of the .mcd
 * must match the one recorded when it was uploaded (appProperties), or at least the zip's CRC32 and size. Only then is
 * it inflated again straight over the .mcd, which is read back and checked. There is no temporary file: the MMCE file
 * system can't rename, and keeping two files open on the sd2psx at the same time is asking for trouble.
 *
 * The caller makes sure the card is not the one the sd2psx is emulating right now (the firmware keeps the active card
 * in its own memory and writes back only the sectors the PS2 changes: writing the file under it would mix the two).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include "common.h"

#define MAX_ZIP (20 * 1024 * 1024)   /* the PS2 has 32 MB; an 8 MB card is usually a 1-5 MB zip */
#define BLOCK   (64 * 1024)

static unsigned int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }
static unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

typedef struct {
    buffer_t zip;
    long long total;
    int (*progress)(int phase, long long done, long long total);
    int tooBig;
} download_ctx_t;

static int on_data(const unsigned char *d, size_t n, void *u)
{
    download_ctx_t *x = u;
    if (x->zip.len + n > (size_t)MAX_ZIP || buf_append(&x->zip, d, n)) {
        x->tooBig = 1;
        return -1;
    }
    return x->progress ? x->progress(RESTORE_DOWNLOAD, x->zip.len, x->total) : 0;
}

/* where the deflate data is (after the local header, the name and the extra field), and the CRC32 and size of the
 * file from the central directory; ps2 = it is a .ps2 (the card with 16 bytes of ECC after each page), not a .mcd.
 * Only our own zips: one file, deflate, no comment */
static int zip_entry(const buffer_t *z, const unsigned char **data, size_t *len, unsigned long *crc, unsigned long *size, int *ps2)
{
    const unsigned char *p = z->data, *eocd, *cd;
    size_t start, cdOff, nameLen;
    if (z->len < 30 + 22 || le32(p) != 0x04034b50 || le16(p + 8) != 8)
        return -1;
    nameLen = le16(p + 26);
    *ps2 = nameLen > 4 && 30 + nameLen <= z->len && !strncasecmp((const char *)p + 30 + nameLen - 4, ".ps2", 4);
    start = 30 + nameLen + le16(p + 28);
    eocd = p + z->len - 22;
    if (start >= z->len || le32(eocd) != 0x06054b50)
        return -1;
    cdOff = le32(eocd + 16);
    if (cdOff < start || cdOff + 46 > z->len)
        return -1;
    cd = p + cdOff;
    if (le32(cd) != 0x02014b50)
        return -1;
    *data = p + start;
    *len = cdOff - start;
    *crc = le32(cd + 16);
    *size = le32(cd + 24);
    return 0;
}

/* a piece of a .ps2 loses the 16 bytes of ECC after each 512, in place: the pages move down over the ECC bytes of the
 * ones before. at = where in a page and its ECC (0..527) the piece starts, kept from one piece to the next. Returns
 * how much is left */
static size_t strip_ecc(unsigned char *buf, size_t k, unsigned *at)
{
    size_t from = 0, to = 0;
    while (from < k) {
        size_t part = (*at < 512 ? 512 : 528) - *at;
        if (part > k - from)
            part = k - from;
        if (*at < 512) {
            memmove(buf + to, buf + from, part);
            to += part;
        }
        from += part;
        *at = (*at + part) % 528;
    }
    return to;
}

/* inflates the whole file: checks it (crc and out are of the file as it is in the zip) and gets the .mcd out of it
 * (a .ps2 loses the 16 bytes after each 512), which is hashed (sha, mcd = its size) and, with fd >= 0, written.
 * 0 = ok, -2 = cancelled */
static int inflate_all(const unsigned char *src, size_t n, int fd, int phase, unsigned long size, int ps2, char sha[65],
                       unsigned long *crc, unsigned long *out, unsigned long *mcd, int (*progress)(int, long long, long long))
{
    static unsigned char buf[BLOCK] __attribute__((aligned(64)));
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 s;
    z_stream z;
    int st, r = -1, i;
    unsigned at = 0;   /* a .ps2: where in the page and its 16 bytes (0..527) the next byte is */
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -15) != Z_OK)
        return -1;
    wc_InitSha256(&s);
    *crc = crc32(0, NULL, 0);
    *out = *mcd = 0;
    z.next_in = (unsigned char *)src;
    z.avail_in = n;
    do {
        size_t k;
        z.next_out = buf;
        z.avail_out = sizeof(buf);
        st = inflate(&z, Z_NO_FLUSH);
        if (st != Z_OK && st != Z_STREAM_END) {
            log_msg("restore: inflate %d after %lu bytes", st, *out);
            goto out;
        }
        k = sizeof(buf) - z.avail_out;
        *crc = crc32(*crc, buf, k);
        *out += k;
        if (ps2)
            k = strip_ecc(buf, k, &at);
        wc_Sha256Update(&s, buf, k);
        *mcd += k;
        if (fd >= 0 && k && write(fd, buf, k) != (int)k) {
            log_msg("restore: write failed at %lu", *mcd);
            goto out;
        }
        if (progress && progress(phase, *out, size) && fd < 0) {   /* once it's writing there's no stopping halfway */
            r = -2;
            goto out;
        }
    } while (st != Z_STREAM_END);
    wc_Sha256Final(&s, h);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(sha + i * 2, "%02x", h[i]);
    r = 0;
out:
    inflateEnd(&z);
    wc_Sha256Free(&s);
    return r;
}

/* reads the card back and hashes it. -2 = given up (progress != 0) */
static int hash_file(const char *path, char sha[65], unsigned long size, int (*progress)(int, long long, long long))
{
    static unsigned char buf[BLOCK] __attribute__((aligned(64)));
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 s;
    long long done = 0;
    int fd = open(path, O_RDONLY), n, i, stop = 0;
    if (fd < 0)
        return -1;
    wc_InitSha256(&s);
    while (!stop && (n = read(fd, buf, sizeof(buf))) > 0) {
        wc_Sha256Update(&s, buf, n);
        done += n;
        if (progress)
            stop = progress(RESTORE_VERIFY, done, size) != 0;
    }
    close(fd);
    wc_Sha256Final(&s, h);
    wc_Sha256Free(&s);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(sha + i * 2, "%02x", h[i]);
    return stop ? -2 : n < 0 ? -1 : 0;
}

/* the first n bytes of the file inside the .zip. 0 = got them */
static int first_bytes(const unsigned char *src, size_t len, unsigned char *out, int n)
{
    z_stream z;
    int st;
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -15) != Z_OK)
        return -1;
    z.next_in = (unsigned char *)src;
    z.avail_in = len;
    z.next_out = out;
    z.avail_out = n;
    st = inflate(&z, Z_SYNC_FLUSH);
    inflateEnd(&z);
    return (st == Z_OK || st == Z_STREAM_END) && z.avail_out == 0 ? 0 : -1;
}

/* A card written from its .zip, which is in memory (and is freed here). The .zip is inflated once only to check it,
 * before the card is touched: its CRC32 and size, and the SHA-256 the .mcd has to have (expect) or, when that isn't
 * known ("": a .zip from a folder, not from Drive), that what is inside is a memory card. Then it is inflated over the
 * .mcd, which is read back. badText = what to say of a .zip that isn't one of these.
 * 0 = ok (sha and mcd = the card's), -2 = cancelled (the card didn't change), -1 = error (googleError says why) */
static int restore_zip(card_t *c, buffer_t *zip, const char *expect, int badText, char sha[65], unsigned long *mcd,
                       int (*progress)(int phase, long long done, long long total))
{
    static const char magic[] = "Sony PS2 Memory Card Format ";
    const unsigned char *data;
    unsigned char start[sizeof(magic) - 1];
    size_t len;
    unsigned long zipCrc, zipSize, crc, size;
    char back[65];
    int r, fd, ps2;
    if (zip_entry(zip, &data, &len, &zipCrc, &zipSize, &ps2) != 0 ||
        (!expect[0] && (first_bytes(data, len, start, sizeof(start)) != 0 || memcmp(start, magic, sizeof(start)) != 0))) {
        log_msg("restore: not a .zip of a memory card");
        snprintf(googleError, sizeof(googleError), "%s", T(badText));
        buf_free(zip);
        return -1;
    }

    /* 1st pass: check everything before touching the card */
    r = inflate_all(data, len, -1, RESTORE_CHECK, zipSize, ps2, sha, &crc, &size, mcd, progress);
    if (r == -2) {
        buf_free(zip);
        return -2;
    }
    if (r != 0 || crc != zipCrc || size != zipSize || (ps2 && size % 528) || *mcd % 512 || !*mcd ||
        (expect[0] && strcasecmp(sha, expect) != 0)) {
        log_msg("restore: the backup doesn't check out (%s, crc %08lx/%08lx, size %lu/%lu, sha %.16s/%.16s)", ps2 ? ".ps2" : ".mcd",
                crc, zipCrc, size, zipSize, sha, expect);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_BAD_BACKUP));
        buf_free(zip);
        return -1;
    }

    /* 2nd pass: over the card. From here on there's no cancelling */
    fd = open(c->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        log_msg("restore: can't open %s for writing (%d)", c->path, fd);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
        buf_free(zip);
        return -1;
    }
    r = inflate_all(data, len, fd, RESTORE_WRITE, zipSize, ps2, back, &crc, &size, mcd, progress);
    if (close(fd) < 0)
        r = -1;
    buf_free(zip);
    /* read back */
    if (r != 0 || hash_file(c->path, back, *mcd, progress) != 0 || strcmp(back, sha) != 0) {
        log_msg("restore: %s didn't check out after writing", c->path);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
        return -1;
    }
    log_msg("restore: %s restored and verified (%lu bytes%s, sha %.16s)", c->id, *mcd, ps2 ? ", from a .ps2" : "", sha);
    c->size = *mcd;
    return 0;
}

int restore_card(card_t *c, const drive_file_t *f, int (*progress)(int phase, long long done, long long total))
{
    download_ctx_t x = {{0}, f->size, progress, 0};
    unsigned long mcd = 0;
    char sha[65];
    card_state_t *e;
    int r;

    googleError[0] = 0;
    log_msg("restore: %s from %s (%lld bytes)", c->id, f->name, f->size);
    if (f->size <= 0 || f->size > MAX_ZIP) {
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_TOO_BIG));
        return -1;
    }
    /* the exact size up front: buf_append doesn't have to grow (and copy) it */
    if (!(x.zip.data = malloc((size_t)f->size + 1))) {
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_TOO_BIG));
        return -1;
    }
    x.zip.cap = (size_t)f->size + 1;
    r = google_download(f->id, on_data, &x);
    if (r == -2 && x.tooBig) {
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_TOO_BIG));
        r = -1;
    }
    if (r != 0) {
        if (r == -1 && !googleError[0])
            snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_DOWNLOAD));
        buf_free(&x.zip);
        return r;
    }
    if ((long long)x.zip.len != f->size) {
        log_msg("restore: got %u of %lld bytes", (unsigned)x.zip.len, f->size);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_DOWNLOAD));
        buf_free(&x.zip);
        return -1;
    }
    if ((r = restore_zip(c, &x.zip, f->sha_mcd, T_ERR_DOWNLOAD, sha, &mcd, progress)) != 0)
        return r;

    /* the card now is that backup: same fingerprint and SHA as the last backup, so it shows as up to date and IGR
     * doesn't send it again */
    e = state_card(c->id, 1);
    if (mcfs_fingerprint(c->path, c->fingerprint, NULL) == 0 && e) {
        snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
        e->version = MCFS_VERSION;
        snprintf(e->sha, sizeof(e->sha), "%s", sha);
        c->status = ST_UP_TO_DATE;
        state_write();
    } else
        c->status = ST_ERROR;
    return 0;
}

/* The same from a .zip in a folder of the microSD or of a USB drive: one "Copy to a device" wrote. It is read whole
 * and closed before the card is opened. The card is then whatever that file had, which no backup on Drive knows of:
 * its status is for the caller to work out again */
static int restore_card_file(card_t *c, const char *path, int (*progress)(int phase, long long done, long long total))
{
    buffer_t zip = {0};
    unsigned long mcd = 0;
    char sha[65];
    long long total;
    int fd, n = 0;
    googleError[0] = 0;
    log_msg("restore: %s from the file %s", c->id, path);
    if ((fd = open(path, O_RDONLY)) < 0) {
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_NOT_CARD));
        return -1;
    }
    total = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (total <= 0 || total > MAX_ZIP || !(zip.data = malloc((size_t)total + 1))) {
        close(fd);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_TOO_BIG));
        return -1;
    }
    zip.cap = (size_t)total + 1;
    while ((long long)zip.len < total &&
           (n = read(fd, zip.data + zip.len, total - zip.len > BLOCK ? BLOCK : (size_t)(total - zip.len))) > 0) {
        zip.len += n;
        if (progress && progress(RESTORE_DOWNLOAD, zip.len, total)) {
            close(fd);
            buf_free(&zip);
            return -2;
        }
    }
    close(fd);
    if ((long long)zip.len != total) {
        buf_free(&zip);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_NOT_CARD));
        return -1;
    }
    return restore_zip(c, &zip, "", T_ERR_NOT_CARD, sha, &mcd, progress);
}

/* ------------------------------------------------------------ a card in a file of a folder
 *
 * A .mcd, a MemCard PRO2's .mc2 (the same thing under another name), a .ps2 (the card with its ECC bytes), or the
 * .zip "Copy to a device" writes with one of those inside: looked into as any card is (mcfs reads it where it is),
 * and installed on the microSD as the .mcd of a card.
 *
 * A file that is a card as it is can be read in any order, so it is opened without reading it. A .zip can't: its
 * card is inflated to memory when it is opened, checked as it goes, and stays there. One that doesn't fit in memory
 * can't be looked into, but is installed the way a backup is restored.
 *
 * Installing a card that fits in memory reads it whole before the first byte is written, so a file that can't be
 * read leaves the card it would replace as it was. One that doesn't fit (32 MB and up) goes straight from its file
 * to the card, a piece at a time, read only once: reading it all first just to see that it reads took as long again,
 * minutes of it on the PS2's USB 1.1. A piece that can't be read is asked about (RESTORE_LOST) and tried again, so
 * that a drive that came loose doesn't cost the card. What was written is read back.
 *
 * Only one file of the sd2psx is open at a time: the file and the card may both be on it. A file that is elsewhere
 * (a USB drive) stays open from start to end. */

#define PIECE (512 * 1024)

/* the file being read: left open from one piece to the next (srcKeep) when it isn't on the device the card is written
 * to. Opening it again for each piece means finding the piece's place again, which on a FAT is a walk from the
 * file's first cluster */
static int srcFd = -1, srcKeep;
static long long srcAt;   /* where srcFd is */

static void src_close(void)
{
    if (srcFd >= 0)
        close(srcFd);
    srcFd = -1;
}

/* are both on the same device ("mmce0:", "mass0:")? */
static int same_device(const char *a, const char *b)
{
    const char *c = strchr(a, ':');
    return !c || !strncmp(a, b, c - a + 1);
}

/* the sizes a card of the sd2psx has */
static int size_ok(long long size)
{
    return size >= 512 * 1024 && size <= 1024LL * 1024 * 1024 && !(size & (size - 1));
}

/* a .zip of ours still in its file, from its start: where its deflate data is and how long, the CRC32 and size of the
 * file inside, and whether that is a .ps2 */
static int zip_file_entry(int fd, long long total, long *start, long *len, unsigned long *crc, unsigned long *size, int *ps2)
{
    unsigned char h[30], name[256], e[22], cd[46];
    unsigned nameLen;
    long cdOff;
    if (total < 30 + 22 || read(fd, h, 30) != 30 || le32(h) != 0x04034b50 || le16(h + 8) != 8)
        return -1;
    nameLen = le16(h + 26);
    if (nameLen < 5 || nameLen > sizeof(name) || read(fd, name, nameLen) != (int)nameLen)
        return -1;
    *ps2 = !strncasecmp((const char *)name + nameLen - 4, ".ps2", 4);
    *start = 30 + nameLen + le16(h + 28);
    if (lseek(fd, (long)total - 22, SEEK_SET) < 0 || read(fd, e, 22) != 22 || le32(e) != 0x06054b50)
        return -1;
    cdOff = le32(e + 16);
    if (cdOff < *start || cdOff + 46 > total || lseek(fd, cdOff, SEEK_SET) < 0 || read(fd, cd, 46) != 46 ||
        le32(cd) != 0x02014b50)
        return -1;
    *len = cdOff - *start;
    *crc = le32(cd + 16);
    *size = le32(cd + 24);
    return 0;
}

/* the card of a .zip, inflated to memory straight from the file (the .zip itself is never held whole: a 16 MB card
 * and its .zip wouldn't both fit) and checked: CRC32, size, and that it is a memory card of the size its file has.
 * 0 = ok (f->image is NULL when there was no room for it), -2 = cancelled */
static int open_zip(card_file_t *f, int fd, long long total, int (*progress)(int, long long, long long))
{
    static unsigned char in[BLOCK] __attribute__((aligned(64))), out[BLOCK] __attribute__((aligned(64)));
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    unsigned long zipCrc, zipSize, crc = crc32(0, NULL, 0), done = 0;
    long start, len, left;
    long long got = 0;
    unsigned at = 0;
    wc_Sha256 s;
    z_stream z;
    int st, n, i, r = -1;
    if (zip_file_entry(fd, total, &start, &len, &zipCrc, &zipSize, &f->ecc) != 0 || zipSize % (f->ecc ? 528 : 512))
        return -1;
    f->size = f->ecc ? (long long)zipSize / 528 * 512 : zipSize;
    if (!size_ok(f->size) || lseek(fd, start, SEEK_SET) < 0)
        return -1;
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -15) != Z_OK)
        return -1;
    f->image = malloc((size_t)f->size);
    wc_InitSha256(&s);
    left = len;
    do {
        size_t k;
        if (!z.avail_in && left > 0) {
            if ((n = read(fd, in, left > BLOCK ? BLOCK : left)) <= 0)
                goto out;
            left -= n;
            z.next_in = in;
            z.avail_in = n;
        }
        z.next_out = out;
        z.avail_out = sizeof(out);
        st = inflate(&z, Z_NO_FLUSH);
        if (st != Z_OK && st != Z_STREAM_END)
            goto out;
        k = sizeof(out) - z.avail_out;
        if (!done && (k < 340 || mcfs_card_size(out, NULL) != f->size))
            goto out;
        if (!f->image) {   /* no room to keep it: that it is a card is all there is to say for now */
            r = 0;
            goto out;
        }
        crc = crc32(crc, out, k);
        done += k;
        if (f->ecc)
            k = strip_ecc(out, k, &at);
        if (got + (long long)k > f->size)
            goto out;
        memcpy(f->image + got, out, k);
        wc_Sha256Update(&s, out, k);
        got += k;
        if (progress && progress(RESTORE_DOWNLOAD, done, zipSize)) {
            r = -2;
            goto out;
        }
    } while (st != Z_STREAM_END);
    if (crc == zipCrc && done == zipSize && got == f->size) {
        wc_Sha256Final(&s, h);
        for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
            sprintf(f->sha + i * 2, "%02x", h[i]);
        r = 0;
    }
out:
    inflateEnd(&z);
    wc_Sha256Free(&s);
    if (r != 0) {
        free(f->image);
        f->image = NULL;
    }
    return r;
}

int card_file_open(card_file_t *f, const char *path, int (*progress)(int phase, long long done, long long total))
{
    unsigned char sb[512];
    size_t n = strlen(path);
    long long total = 0;
    int fd, page = 512, r = -1;
    memset(f, 0, sizeof(*f));
    snprintf(f->path, sizeof(f->path), "%s", path);
    googleError[0] = 0;
    f->zip = n > 4 && !strcasecmp(path + n - 4, ".zip");
    if ((fd = open(path, O_RDONLY)) >= 0) {
        total = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (f->zip)
            r = open_zip(f, fd, total, progress);
        else if (read(fd, sb, sizeof(sb)) == (int)sizeof(sb) && size_ok(f->size = mcfs_card_size(sb, &page))) {
            /* with the ECC bytes a .ps2 has, or without them, whatever the file is called */
            f->ecc = page == 512 && total == f->size / 512 * 528;
            if (f->ecc || total == f->size)
                r = 0;
        }
        close(fd);
    }
    log_msg("card file %s (%lld bytes): %d, a %lld byte card%s%s%s", path, total, r, f->size, f->zip ? ", in a .zip" : "",
            f->ecc ? ", with ECC" : "", f->zip && r == 0 && !f->image ? ", too big for memory" : "");
    if (r == -1)
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_NOT_CARD));
    if (r != 0) {
        memset(f, 0, sizeof(*f));
        return r;
    }
    mcfs_image(f->zip ? NULL : f->path, f->image, (size_t)f->size);
    return 0;
}

void card_file_close(card_file_t *f)
{
    free(f->image);
    memset(f, 0, sizeof(*f));
    mcfs_image(NULL, NULL, 0);
}

/* n bytes of the card (as a .mcd) from at, read from its file. out has room for them with their ECC. at and n are
 * whole pages */
static int file_piece(const card_file_t *f, long long at, unsigned char *out, int n)
{
    long long from = f->ecc ? at / 512 * 528 : at;
    int want = f->ecc ? n / 512 * 528 : n, got = 0, k = 0;
    unsigned e = 0;
#ifdef DEBUG_BUILD
    if (at * 2 >= f->size && debug_take('z')) {   /* (script: the file can't be read, halfway through, once) */
        src_close();
        return -1;
    }
#endif
    if (srcFd < 0) {
        if ((srcFd = open(f->path, O_RDONLY)) < 0)
            return -1;
        srcAt = 0;
    }
    if (srcAt == from || lseek(srcFd, (long)from, SEEK_SET) >= 0)
        for (; got < want && (k = read(srcFd, out + got, want - got > BLOCK ? BLOCK : want - got)) > 0; got += k)
            ;
    srcAt = from + got;
    if (!srcKeep || got != want)
        src_close();
    if (got != want)
        return -1;
    return f->ecc ? (int)strip_ecc(out, got, &e) : got;
}

static void sha_hex(wc_Sha256 *s, char sha[65])
{
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    int i;
    wc_Sha256Final(s, h);
    wc_Sha256Free(s);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(sha + i * 2, "%02x", h[i]);
}

/* how long the file took to be read and the card to be written, for the log: on the console those are what an
 * install waits for */
static u64 msRead, msWrite;

/* the whole card read into memory (f->image, which has room for it) before anything is written, and its SHA-256.
 * 0 = ok, -2 = cancelled */
static int file_load(card_file_t *f, unsigned char *piece, int (*progress)(int, long long, long long))
{
    wc_Sha256 s;
    long long at;
    int n, r = 0;
    wc_InitSha256(&s);
    if (progress && progress(RESTORE_DOWNLOAD, 0, f->size))   /* (the screen says what is being done from the start) */
        r = -2;
    for (at = 0; at < f->size && r == 0; at += n) {
        u64 t = now_ms();
        n = f->size - at > PIECE ? PIECE : (int)(f->size - at);
        if (file_piece(f, at, piece, n) != n)
            r = -1;
        else {
            msRead += now_ms() - t;
            wc_Sha256Update(&s, piece, n);
            memcpy(f->image + at, piece, n);
            if (progress && progress(RESTORE_DOWNLOAD, at + n, f->size))
                r = -2;
        }
    }
    sha_hex(&s, f->sha);
    return r;
}

/* The card onto dest, a piece at a time: dest is open only while a piece goes into it. From memory, or (a card that
 * doesn't fit there) straight from its file, hashed on its way (f->sha). A card that is there already, and isn't
 * bigger, is written over where it is: the sd2psx syncs the file after every 4 KB it is given, which on a file that
 * grows means its entry in the folder and the FAT each time, and on one that keeps its size means nothing. *began =
 * dest isn't what it was any more. 0 = ok, -2 = given up (progress != 0), -1 = error (googleError says which) */
static int file_write_card(card_file_t *f, const char *dest, unsigned char *piece, int *began,
                           int (*progress)(int, long long, long long))
{
    wc_Sha256 s;
    long long at;
    int n, fd, done, k = 0, r = 0, flags = O_WRONLY | O_CREAT | O_TRUNC;
    long end;
    if ((fd = open(dest, O_RDONLY)) >= 0) {
        if ((end = lseek(fd, 0, SEEK_END)) >= 0 && end <= f->size)
            flags = O_WRONLY;
        close(fd);
    }
    if (!f->image)
        wc_InitSha256(&s);
    if (progress && progress(RESTORE_WRITE, 0, f->size))
        r = -2;
    for (at = 0; at < f->size && r == 0; at += n) {
        const unsigned char *d = f->image ? f->image + at : piece;
        u64 t = now_ms();
        n = f->size - at > PIECE ? PIECE : (int)(f->size - at);
        if (!f->image) {
            /* a piece that can't be read is asked about, and tried again unless the file is given up on */
            while ((r = file_piece(f, at, piece, n) == n ? 0 : -1) != 0 && progress && !progress(RESTORE_LOST, at, f->size))
                t = now_ms();
            if (r != 0) {
                snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_READ_FILE));
                break;
            }
            msRead += now_ms() - t;
            wc_Sha256Update(&s, piece, n);
            t = now_ms();
        }
        if ((fd = open(dest, at ? O_WRONLY : flags, 0666)) >= 0) {
            *began = 1;
            if (at)
                lseek(fd, (long)at, SEEK_SET);
            for (done = 0; done < n; done += k)
                if ((k = write(fd, d + done, n - done > BLOCK ? BLOCK : n - done)) <= 0)
                    break;
            if (close(fd) < 0 || done < n)
                r = -1;
        } else
            r = -1;
        msWrite += now_ms() - t;
        if (r != 0)
            snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
        else if (progress && progress(RESTORE_WRITE, at + n, f->size))
            r = -2;
    }
    if (!f->image)
        sha_hex(&s, f->sha);
    return r;
}

int card_file_install(card_file_t *f, card_t *to, int (*progress)(int phase, long long done, long long total))
{
    unsigned char *piece;
    char back[65];
    u64 t;
    int r = 0, began = 0, fd, streamed;
    googleError[0] = 0;
    log_msg("install: %s as %s", f->path, to->path);
    if (f->zip && !f->image)   /* a .zip whose card doesn't fit in memory: as a backup from Drive is restored */
        return restore_card_file(to, f->path, progress);
    if (!(piece = malloc(PIECE / 512 * 528))) {
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_TOO_BIG));
        return -1;
    }
    msRead = msWrite = 0;
    srcKeep = !same_device(f->path, to->path);
    if (!f->image && (f->image = malloc((size_t)f->size)) != NULL && (r = file_load(f, piece, progress)) != 0) {
        src_close();
        free(piece);
        free(f->image);
        f->image = NULL;
        if (r == -1)
            snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_READ_FILE));
        return r;
    }
    if (!(streamed = !f->image))
        src_close();
    r = file_write_card(f, to->path, piece, &began, progress);
    src_close();
    free(piece);
    /* (a card written over where it was, and only in part, would still pass for one: it is left empty) */
    if (r != 0 && began && (fd = open(to->path, O_WRONLY | O_TRUNC)) >= 0)
        close(fd);
    t = now_ms();
    if (r == 0 && (r = hash_file(to->path, back, f->size, progress)) == 0 && strcmp(back, f->sha) != 0)
        r = -1;
    if (r == 0) {
        log_msg("install: %s written and verified (%lld bytes, sha %.16s)", to->id, f->size, f->sha);
        to->size = f->size;
    } else if (r == -2)
        log_msg("install: %s given up", to->path);
    else {
        log_msg("install: %s failed (%s)", to->path, googleError[0] ? googleError : "it doesn't read back as it was written");
        if (!googleError[0])
            snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
    }
    log_msg("install: %s, %d s reading the file, %d s writing the card, %d s reading it back",
            streamed ? "straight from the file" : "from memory", (int)(msRead / 1000), (int)(msWrite / 1000), (int)((now_ms() - t) / 1000));
    if (!f->zip) {   /* the file is there to be read again; a .zip's card stays, to be looked into */
        free(f->image);
        f->image = NULL;
    }
    return r;
}
