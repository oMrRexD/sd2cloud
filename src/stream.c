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

static void hex(const unsigned char *h, char *out)
{
    int i;
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(out + i * 2, "%02x", h[i]);
}

int stream_zip(const card_t *c, const datetime_t *t, stream_t *s, chunk_cb cb, void *u)
{
    static unsigned char block[BLOCK] __attribute__((aligned(64))), comp[BLOCK];
    unsigned dosTime = (t->hour << 11) | (t->minute << 5) | (t->second / 2);
    unsigned dosDate = ((t->year - 1980) << 9) | (t->month << 5) | t->day;
    char name[80];
    size_t nameLen;
    unsigned char hdr[128], h[WC_SHA256_DIGEST_SIZE];
    wc_Sha256 shaMcd;
    uLong crc = crc32(0, NULL, 0);
    z_stream z;
    output_t o;
    long long cdStart;
    int fd, n, r = -1;

    memset(&o, 0, sizeof(o));
    o.s = s;
    o.cb = cb;
    o.u = u;
    s->read = s->sent = 0;
    snprintf(name, sizeof(name), "%s.mcd", c->base);
    nameLen = strlen(name);
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

    /* local header: CRC and sizes come in the descriptor after the data (bit 3) */
    p32(hdr, 0x04034b50); p16(hdr + 4, 20); p16(hdr + 6, 0x0008); p16(hdr + 8, 8); p16(hdr + 10, dosTime);
    p16(hdr + 12, dosDate); p32(hdr + 14, 0); p32(hdr + 18, 0); p32(hdr + 22, 0); p16(hdr + 26, nameLen); p16(hdr + 28, 0);
    emit(&o, hdr, 30);
    emit(&o, (unsigned char *)name, nameLen);

    for (;;) {
        n = read(fd, block, sizeof(block));
        if (n < 0) {
            log_msg("%s: read error at %lld (%d)", c->id, s->read, n);
            goto out_z;
        }
        if (n == 0)
            break;
        s->read += n;
        wc_Sha256Update(&shaMcd, block, n);
        crc = crc32(crc, block, n);
        z.next_in = block;
        z.avail_in = n;
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

    /* data descriptor, central directory (with the real values) and end of central directory */
    p32(hdr, 0x08074b50); p32(hdr + 4, crc); p32(hdr + 8, z.total_out); p32(hdr + 12, s->total);
    emit(&o, hdr, 16);
    cdStart = s->sent;
    p32(hdr, 0x02014b50); p16(hdr + 4, 20); p16(hdr + 6, 20); p16(hdr + 8, 0x0008); p16(hdr + 10, 8);
    p16(hdr + 12, dosTime); p16(hdr + 14, dosDate); p32(hdr + 16, crc); p32(hdr + 20, z.total_out); p32(hdr + 24, s->total);
    p16(hdr + 28, nameLen); p16(hdr + 30, 0); p16(hdr + 32, 0); p16(hdr + 34, 0); p16(hdr + 36, 0); p32(hdr + 38, 0);
    p32(hdr + 42, 0);
    emit(&o, hdr, 46);
    emit(&o, (unsigned char *)name, nameLen);
    p32(hdr, 0x06054b50); p16(hdr + 4, 0); p16(hdr + 6, 0); p16(hdr + 8, 1); p16(hdr + 10, 1);
    p32(hdr + 12, s->sent - cdStart); p32(hdr + 16, cdStart); p16(hdr + 20, 0);
    emit(&o, hdr, 22);
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
