/*
 * SD2Cloud -- reads the .mcd and builds the .zip as a stream, without keeping the card in memory (they go up to
 * 128 MB, the PS2 has 32): 64 KB blocks -> SHA-256 and CRC32 of the .mcd -> deflate level 1 -> the .zip comes out in
 * CHUNK pieces (1 MiB, a multiple of the 256 KB the Drive resumable upload requires; the last one is smaller). Since
 * the compressed size is only known at the end, the local header has bit 3 ("data descriptor") set and the CRC/sizes
 * come after the data, the way PKZIP does it when streaming. Windows and 7-Zip open it normally.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include "common.h"

#define BLOCK (64 * 1024)

typedef struct {
    unsigned char *buf;
    size_t used;
    wc_Sha256 sha;
    stream_t *s;
    chunk_cb cb;
    void *u;
    int error;
} output_t;

static void p16(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void p32(unsigned char *p, unsigned long v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* adds bytes to the .zip; hands out every full CHUNK */
static void emit(output_t *o, const unsigned char *d, size_t n)
{
    while (n && !o->error) {
        size_t k = CHUNK - o->used < n ? CHUNK - o->used : n;
        memcpy(o->buf + o->used, d, k);
        wc_Sha256Update(&o->sha, d, k);
        o->used += k;
        o->s->sent += k;
        d += k;
        n -= k;
        if (o->used == CHUNK) {
            if (o->cb(o->buf, CHUNK, 0, o->u))
                o->error = 1;
            o->used = 0;
        }
    }
}

/* the .zip's only file starts: its local header. The CRC and the sizes come in the descriptor after the data (bit 3) */
static void zip_head(output_t *o, const char *name, unsigned dosTime, unsigned dosDate)
{
    unsigned char hdr[30];
    size_t nameLen = strlen(name);
    p32(hdr, 0x04034b50); p16(hdr + 4, 20); p16(hdr + 6, 0x0008); p16(hdr + 8, 8); p16(hdr + 10, dosTime);
    p16(hdr + 12, dosDate); p32(hdr + 14, 0); p32(hdr + 18, 0); p32(hdr + 22, 0); p16(hdr + 26, nameLen); p16(hdr + 28, 0);
    emit(o, hdr, 30);
    emit(o, (const unsigned char *)name, nameLen);
}

/* and ends: the data descriptor, the central directory (with the real values) and the end of central directory */
static void zip_tail(output_t *o, const char *name, unsigned dosTime, unsigned dosDate, unsigned long crc, unsigned long comp,
                     unsigned long unc)
{
    unsigned char hdr[46];
    size_t nameLen = strlen(name);
    long long cdStart;
    p32(hdr, 0x08074b50); p32(hdr + 4, crc); p32(hdr + 8, comp); p32(hdr + 12, unc);
    emit(o, hdr, 16);
    cdStart = o->s->sent;
    p32(hdr, 0x02014b50); p16(hdr + 4, 20); p16(hdr + 6, 20); p16(hdr + 8, 0x0008); p16(hdr + 10, 8);
    p16(hdr + 12, dosTime); p16(hdr + 14, dosDate); p32(hdr + 16, crc); p32(hdr + 20, comp); p32(hdr + 24, unc);
    p16(hdr + 28, nameLen); p16(hdr + 30, 0); p16(hdr + 32, 0); p16(hdr + 34, 0); p16(hdr + 36, 0); p32(hdr + 38, 0);
    p32(hdr + 42, 0);
    emit(o, hdr, 46);
    emit(o, (const unsigned char *)name, nameLen);
    p32(hdr, 0x06054b50); p16(hdr + 4, 0); p16(hdr + 6, 0); p16(hdr + 8, 1); p16(hdr + 10, 1);
    p32(hdr + 12, o->s->sent - cdStart); p32(hdr + 16, cdStart); p16(hdr + 20, 0);
    emit(o, hdr, 22);
}

static void hex(const unsigned char *h, char *out)
{
    int i;
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(out + i * 2, "%02x", h[i]);
}

/* The .ps2 of PCSX2 is the card as its flash chip holds it: after each 512-byte page come 16 bytes, 3 of error
 * correction for each 128 bytes of the page and 4 unused. The .mcd of the sd2psx is the same card without them */
#define PAGE     512
#define PAGE_ECC (PAGE + 16)

static unsigned char parityOf[256], columnsOf[256];

static void ecc_tables(void)
{
    static const unsigned char masks[7] = {0x55, 0x33, 0x0F, 0x00, 0xAA, 0xCC, 0xF0};
    int b, i;
    if (columnsOf[1])
        return;
    for (b = 0; b < 256; b++) {
        int a = b ^ (b >> 1);
        a ^= a >> 2;
        a ^= a >> 4;
        parityOf[b] = a & 1;
    }
    for (b = 0; b < 256; b++)
        for (i = 0; i < 7; i++)
            columnsOf[b] |= parityOf[b & masks[i]] << i;
}

/* n bytes of whole pages -> the same pages with their 16 bytes each; returns how many bytes that makes */
static int add_ecc(const unsigned char *in, int n, unsigned char *out)
{
    int page, part, i, made = 0;
    for (page = 0; page + PAGE <= n; page += PAGE) {
        unsigned char *spare = out + made + PAGE;
        memcpy(out + made, in + page, PAGE);
        for (part = 0; part < 4; part++) {
            const unsigned char *d = in + page + part * 128;
            int columns = 0x77, lines0 = 0x7F, lines1 = 0x7F;
            for (i = 0; i < 128; i++) {
                columns ^= columnsOf[d[i]];
                if (parityOf[d[i]]) {
                    lines0 ^= ~i;
                    lines1 ^= i;
                }
            }
            spare[part * 3] = columns;
            spare[part * 3 + 1] = lines0 & 0x7F;
            spare[part * 3 + 2] = lines1;
        }
        memset(spare + 12, 0, 4);
        made += PAGE_ECC;
    }
    return made;
}

int stream_zip(const card_t *c, const datetime_t *t, stream_t *s, chunk_cb cb, void *u)
{
    static unsigned char block[BLOCK] __attribute__((aligned(64))), comp[BLOCK], withEcc[BLOCK / PAGE * PAGE_ECC];
    const unsigned char *in;
    int ps2 = cfg.ps2, inLen;
    unsigned long unc = 0;   /* the size of the file inside the zip: the card's, or more with the ECC */
    unsigned dosTime = (t->hour << 11) | (t->minute << 5) | (t->second / 2);
    unsigned dosDate = ((t->year - 1980) << 9) | (t->month << 5) | t->day;
    char name[80];
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 shaMcd;
    uLong crc = crc32(0, NULL, 0);
    z_stream z;
    output_t o;
    int fd, n, r = -1;

    memset(&o, 0, sizeof(o));
    o.s = s;
    o.cb = cb;
    o.u = u;
    s->read = s->sent = 0;
    snprintf(name, sizeof(name), "%s%s", c->base, ps2 ? ".ps2" : dev->ext);
    ecc_tables();
    if (!(o.buf = malloc(CHUNK)))
        return -1;
    fd = open(c->path, O_RDONLY);
    if (fd < 0) {
        free(o.buf);
        return -1;
    }
    s->total = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    memset(&z, 0, sizeof(z));
    if (deflateInit2(&z, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        goto out;
    wc_InitSha256(&o.sha);
    wc_InitSha256(&shaMcd);

    zip_head(&o, name, dosTime, dosDate);
    for (;;) {
        n = read(fd, block, sizeof(block));
        if (n < 0) {
            log_msg("%s: read error at %lld (%d)", c->id, s->read, n);
            goto out_z;
        }
        if (n == 0)
            break;
        s->read += n;
        wc_Sha256Update(&shaMcd, block, n);   /* always the .mcd's: what the card is compared by, whatever the format */
        in = block;
        inLen = n;
        if (ps2) {
            if (n % PAGE) {
                log_msg("%s: %d bytes read, not whole pages: can't be sent as .ps2", c->id, n);
                goto out_z;
            }
            in = withEcc;
            inLen = add_ecc(block, n, withEcc);
        }
        unc += inLen;
        crc = crc32(crc, in, inLen);
        z.next_in = (unsigned char *)in;
        z.avail_in = inLen;
        do {
            z.next_out = comp;
            z.avail_out = sizeof(comp);
            deflate(&z, Z_NO_FLUSH);
            emit(&o, comp, sizeof(comp) - z.avail_out);
        } while (z.avail_out == 0 && !o.error);
        if (o.error)
            goto out_z;
        if (s->progress && s->progress(s->read, s->total)) {
            log_msg("%s: stopped at %lld bytes", c->id, s->read);
            goto out_z;
        }
    }
    if (s->read != s->total) {
        log_msg("%s: read %lld of %lld bytes", c->id, s->read, s->total);
        goto out_z;
    }
    for (;;) {
        int st;
        z.next_out = comp;
        z.avail_out = sizeof(comp);
        st = deflate(&z, Z_FINISH);
        emit(&o, comp, sizeof(comp) - z.avail_out);
        if (st == Z_STREAM_END)
            break;
        if (st != Z_OK && st != Z_BUF_ERROR) {
            log_msg("%s: zlib error %d", c->id, st);
            goto out_z;
        }
    }

    zip_tail(&o, name, dosTime, dosDate, crc, z.total_out, unc);
    if (o.error)
        goto out_z;

    wc_Sha256Final(&shaMcd, h);
    hex(h, s->sha_mcd);
    wc_Sha256Final(&o.sha, h);
    hex(h, s->sha_zip);
    /* the last chunk (smaller than CHUNK, or even empty if the zip ended right on a boundary) */
    if (cb(o.buf, o.used, 1, u) == 0)
        r = 0;
out_z:
    deflateEnd(&z);
    wc_Sha256Free(&shaMcd);
    wc_Sha256Free(&o.sha);
out:
    close(fd);
    free(o.buf);
    return r;
}

/* A card's file copied to a folder of the microSD or of a USB drive (dest) inside a .zip, as the backups on Drive
 * are: as it is (.mcd) or with the ECC bytes a .ps2 has. Copying it as it was took three passes of the whole card
 * over the sd2psx's slow bus (read, write, read back); a card is mostly empty, so its .zip is a fraction of that to
 * write and to read back. The .zip is read back and compared by its SHA-256. Only one of the two files is open at
 * a time, a piece of the card each turn: on the sd2psx the card and its copy can't both be open. 0 = ok (a copy
 * that failed is removed) */
#define PIECE (512 * 1024)

typedef struct {
    const char *dest;
    long long wrote;
} zip_file_t;

/* a piece of the .zip onto the end of its file, which is only open for that */
static int zip_to_file(const unsigned char *d, size_t n, int last, void *u)
{
    zip_file_t *f = u;
    size_t done = 0;
    int fd, got = 0;
    (void)last;
    if (!n)
        return 0;
    if ((fd = open(f->dest, f->wrote ? O_WRONLY : O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        return -1;
    if (f->wrote)
        lseek(fd, (long)f->wrote, SEEK_SET);
    for (; done < n; done += got)
        if ((got = write(fd, d + done, n - done > BLOCK ? BLOCK : n - done)) <= 0)
            break;
    if (close(fd) < 0 || done < n)
        return -1;
    f->wrote += n;
    return 0;
}

int card_export(const card_t *c, const char *dest, int ps2, int (*progress)(long long done, long long total))
{
    static unsigned char comp[BLOCK];
    unsigned char *in = malloc(PIECE), *ecc = ps2 ? malloc(PIECE / PAGE * PAGE_ECC) : NULL;
    unsigned char h1[WC_SHA256_DIGEST_SIZE], h2[WC_SHA256_DIGEST_SIZE];
    zip_file_t f = {dest, 0};
    datetime_t t;
    stream_t s;
    output_t o;
    z_stream z;
    wc_Sha256 sha;
    uLong crc = crc32(0, NULL, 0);
    unsigned long unc = 0;
    unsigned dosTime, dosDate;
    long long total = c->size, at = 0, back = 0;
    char name[80];
    int r = -1, fd, n = 0, k, got, st;
    local_time(&t);
    dosTime = (t.hour << 11) | (t.minute << 5) | (t.second / 2);
    dosDate = ((t.year - 1980) << 9) | (t.month << 5) | t.day;
    snprintf(name, sizeof(name), "%s%s", c->base, ps2 ? ".ps2" : dev->ext);
    memset(&s, 0, sizeof(s));
    memset(&o, 0, sizeof(o));
    memset(&z, 0, sizeof(z));
    o.s = &s;
    o.cb = zip_to_file;
    o.u = &f;
    o.buf = malloc(CHUNK);
    ecc_tables();
    if (!in || !o.buf || (ps2 && !ecc) || total <= 0 || deflateInit2(&z, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        goto end;
    wc_InitSha256(&o.sha);
    zip_head(&o, name, dosTime, dosDate);
    while (at < total) {
        const unsigned char *d = in;
        if ((fd = open(c->path, O_RDONLY)) < 0)
            goto end_z;
        lseek(fd, (long)at, SEEK_SET);
        for (n = 0; n < PIECE && (got = read(fd, in + n, BLOCK)) > 0; n += got)
            ;
        close(fd);   /* before any of the .zip is written */
        if (n <= 0 || (ps2 && n % PAGE))
            goto end_z;
        k = n;
        if (ps2) {
            k = add_ecc(in, n, ecc);
            d = ecc;
        }
        unc += k;
        crc = crc32(crc, d, k);
        z.next_in = (unsigned char *)d;
        z.avail_in = k;
        do {
            z.next_out = comp;
            z.avail_out = sizeof(comp);
            deflate(&z, Z_NO_FLUSH);
            emit(&o, comp, sizeof(comp) - z.avail_out);
        } while (z.avail_out == 0 && !o.error);
        if (o.error)
            goto end_z;
        at += n;
        if (progress)
            progress(at, total * 2);
    }
    do {
        z.next_out = comp;
        z.avail_out = sizeof(comp);
        st = deflate(&z, Z_FINISH);
        emit(&o, comp, sizeof(comp) - z.avail_out);
    } while (st == Z_OK || st == Z_BUF_ERROR);
    if (st != Z_STREAM_END)
        goto end_z;
    zip_tail(&o, name, dosTime, dosDate, crc, z.total_out, unc);
    if (o.error || zip_to_file(o.buf, o.used, 1, &f) != 0)
        goto end_z;
    wc_Sha256Final(&o.sha, h1);
    /* read back: the file there has to be the .zip that was made */
    wc_InitSha256(&sha);
    if ((fd = open(dest, O_RDONLY)) >= 0) {
        while ((n = read(fd, in, BLOCK)) > 0) {
            wc_Sha256Update(&sha, in, n);
            back += n;
            if (progress)
                progress(total + back * total / f.wrote, total * 2);
        }
        close(fd);
        wc_Sha256Final(&sha, h2);
        if (n == 0 && back == f.wrote && !memcmp(h1, h2, sizeof(h1)))
            r = 0;
    }
    wc_Sha256Free(&sha);
end_z:
    deflateEnd(&z);
    wc_Sha256Free(&o.sha);
end:
    free(in);
    free(ecc);
    free(o.buf);
    if (r != 0)
        unlink(dest);
    return r;
}
