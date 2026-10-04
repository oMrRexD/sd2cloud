/*
 * SD2Cloud -- restores a card from one of its backups on Drive.
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
        if (ps2) {   /* the pages move down over the ECC bytes of the ones before, in place */
            size_t from = 0, to = 0;
            while (from < k) {
                size_t part = (at < 512 ? 512 : 528) - at;
                if (part > k - from)
                    part = k - from;
                if (at < 512) {
                    memmove(buf + to, buf + from, part);
                    to += part;
                }
                from += part;
                at = (at + part) % 528;
            }
            k = to;
        }
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

/* reads the card back and hashes it */
static int hash_file(const char *path, char sha[65], unsigned long size, int (*progress)(int, long long, long long))
{
    static unsigned char buf[BLOCK] __attribute__((aligned(64)));
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 s;
    long long done = 0;
    int fd = open(path, O_RDONLY), n, i;
    if (fd < 0)
        return -1;
    wc_InitSha256(&s);
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        wc_Sha256Update(&s, buf, n);
        done += n;
        if (progress)
            progress(RESTORE_VERIFY, done, size);
    }
    close(fd);
    wc_Sha256Final(&s, h);
    wc_Sha256Free(&s);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(sha + i * 2, "%02x", h[i]);
    return n < 0 ? -1 : 0;
}

int restore_card(card_t *c, const drive_file_t *f, int (*progress)(int phase, long long done, long long total))
{
    download_ctx_t x = {{0}, f->size, progress, 0};
    const unsigned char *data;
    size_t len;
    unsigned long zipCrc, zipSize, crc, size, mcd;
    char sha[65], back[65];
    card_state_t *e;
    int r, fd, ps2;

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
    if ((long long)x.zip.len != f->size || zip_entry(&x.zip, &data, &len, &zipCrc, &zipSize, &ps2) != 0) {
        log_msg("restore: got %u of %lld bytes, or not an SD2Cloud zip", (unsigned)x.zip.len, f->size);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_DOWNLOAD));
        buf_free(&x.zip);
        return -1;
    }

    /* 1st pass: check everything before touching the card */
    r = inflate_all(data, len, -1, RESTORE_CHECK, zipSize, ps2, sha, &crc, &size, &mcd, progress);
    if (r == -2) {
        buf_free(&x.zip);
        return -2;
    }
    if (r != 0 || crc != zipCrc || size != zipSize || (ps2 && size % 528) || (f->sha_mcd[0] && strcasecmp(sha, f->sha_mcd) != 0)) {
        log_msg("restore: the backup doesn't check out (%s, crc %08lx/%08lx, size %lu/%lu, sha %.16s/%.16s)", ps2 ? ".ps2" : ".mcd",
                crc, zipCrc, size, zipSize, sha, f->sha_mcd);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_BAD_BACKUP));
        buf_free(&x.zip);
        return -1;
    }

    /* 2nd pass: over the card. From here on there's no cancelling */
    fd = open(c->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        log_msg("restore: can't open %s for writing (%d)", c->path, fd);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
        buf_free(&x.zip);
        return -1;
    }
    r = inflate_all(data, len, fd, RESTORE_WRITE, zipSize, ps2, back, &crc, &size, &mcd, progress);
    if (close(fd) < 0)
        r = -1;
    buf_free(&x.zip);
    /* read back */
    if (r != 0 || hash_file(c->path, back, mcd, progress) != 0 || strcmp(back, sha) != 0) {
        log_msg("restore: %s didn't check out after writing", c->path);
        snprintf(googleError, sizeof(googleError), "%s", T(T_ERR_WRITE));
        return -1;
    }
    log_msg("restore: %s restored and verified (%lu bytes%s, sha %.16s)", c->id, mcd, ps2 ? ", from a .ps2" : "", sha);

    /* the card now is that backup: same fingerprint and SHA as the last backup, so it shows as up to date and IGR
     * doesn't send it again */
    c->size = mcd;
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
