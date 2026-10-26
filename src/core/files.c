/* SD2Cloud -- buffer, files (one at a time on the sd2psx), .ini and SHA-256 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
/* only to list a folder (dir_list), with fileXio's own descriptors: never mixed with newlib's */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <iox_stat.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include "common.h"

int buf_append(buffer_t *b, const void *d, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 16 * 1024;
        unsigned char *q;
        while (cap < b->len + n + 1)
            cap *= 2;
        q = realloc(b->data, cap);
        if (!q)
            return -1;
        b->data = q;
        b->cap = cap;
    }
    memcpy(b->data + b->len, d, n);
    b->len += n;
    b->data[b->len] = 0;
    return 0;
}

void buf_free(buffer_t *b)
{
    free(b->data);
    memset(b, 0, sizeof(*b));
}

int file_read(const char *path, buffer_t *b)
{
    static unsigned char block[16 * 1024] __attribute__((aligned(64)));
    long size;
    int fd, n;
    buf_free(b);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return fd;
    size = lseek(fd, 0, SEEK_END);   /* a read that ends before the file does failed: it isn't the file */
    lseek(fd, 0, SEEK_SET);
    /* the whole size at once: grown piece by piece, the buffer of an 8 MB file would end up taking 16 */
    if (size > 0 && (b->data = malloc((size_t)size + 1)) != NULL)
        b->cap = (size_t)size + 1;
    while ((n = read(fd, block, sizeof(block))) > 0) {
        if (buf_append(b, block, n)) {
            n = -1;
            break;
        }
    }
    if (n == 0 && size > 0 && (long)b->len != size)
        n = -1;
    close(fd);
    if (n < 0)
        buf_free(b);   /* half a file is no use to anyone */
    return n < 0 ? n : 0;
}

int file_write(const char *path, const unsigned char *d, size_t n)
{
    int fd, r = 0;
    size_t done = 0;
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return fd;
    while (done < n) {
        size_t k = n - done > 64 * 1024 ? 64 * 1024 : n - done;
        int w = write(fd, d + done, k);
        if (w <= 0) {
            r = w < 0 ? w : -1;
            break;
        }
        done += w;
    }
    if (close(fd) < 0 && r == 0)
        r = -1;
    return r;
}

int file_write_checked(const char *path, const unsigned char *d, size_t n)
{
    buffer_t b = {0};
    int ok = file_write(path, d, n) == 0 && file_read(path, &b) == 0 && b.len == n && (!n || memcmp(b.data, d, n) == 0);
    buf_free(&b);
    return ok;
}

int file_replace(const char *path, const unsigned char *d, size_t n)
{
    char tmp[260], old[260];
    int kept, moved;
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    snprintf(old, sizeof(old), "%s.old", path);
    unlink(tmp);
    if (!file_write_checked(tmp, d, n)) {
        log_msg("files: %s couldn't be written", tmp);
        unlink(tmp);
        return -1;
    }
    /* The file that is there is set aside (.old), not deleted, until the new one is in its place: whatever fails on the
     * way, one of the two stays whole (file_recover). On a file system that can't rename (an sd2psx before its
     * firmware 1.4.0) it is deleted, and the new one written again in its place; the .new stays if that fails */
    unlink(old);
    kept = file_exists(path) ? rename(path, old) : 1;   /* (1: there was none) */
    if (kept < 0)
        unlink(path);
    if ((moved = rename(tmp, path)) == 0 || file_write_checked(path, d, n)) {
        unlink(tmp);
        unlink(old);
        return 0;
    }
    if (kept == 0 && file_exists(old)) {   /* (the old one back in its place) */
        unlink(path);
        rename(old, path);
    }
    log_msg("files: %s couldn't take the place of %s (setting the old one aside: %d; renaming: %d); %s", tmp, path, kept,
            moved, file_exists(path) ? "it is as it was" : "left as .new");
    return -1;
}

/* n bytes of a file from at, the file open only while they are read. Returns how many were read */
#define MOVE_PIECE (64 * 1024)
static int piece_at(const char *path, long long at, unsigned char *d, int n)
{
    int fd = open(path, O_RDONLY), got = 0, k;
    if (fd < 0)
        return -1;
    if (!at || lseek(fd, (long)at, SEEK_SET) == (long)at)
        while (got < n && (k = read(fd, d + got, n - got)) > 0)
            got += k;
    close(fd);
    return got;
}

/* FNV-1a of a piece: what the copy's piece is compared with, read back, without reading the original again */
static u64 piece_sum(const unsigned char *d, int n)
{
    u64 h = 0xcbf29ce484222325ULL;
    while (n--)
        h = (h ^ *d++) * 0x100000001b3ULL;
    return h;
}

int file_copy_shown(const char *from, const char *to, void (*progress)(long long done, long long total))
{
    static unsigned char a[MOVE_PIECE], b[MOVE_PIECE];
    long long at = 0, size;
    int fd, n = 0, k = 0, w = 0, r = 0, i;
    u64 *sums = NULL, start = now_ms();
    const char *why = NULL;   /* (the step that failed, for the log) */
    if (file_exists(to) || (fd = open(from, O_RDONLY)) < 0) {
        log_msg("files: %s not copied to %s: %s", from, to, file_exists(to) ? "it is there already" : "it can't be opened");
        return -1;
    }
    size = lseek(fd, 0, SEEK_END);
    close(fd);
    if (size < 0)
        r = -1, why = "its size can't be told";
    else if (!size)   /* (an empty file: made empty) */
        r = (fd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0 && close(fd) >= 0 ? 0 : -1;
    else
        sums = malloc((size_t)((size + MOVE_PIECE - 1) / MOVE_PIECE) * sizeof(u64));   /* (none: the original is read again) */
    for (at = 0, i = 0; at < size && r == 0; at += n, i++) {
        n = size - at > MOVE_PIECE ? MOVE_PIECE : (int)(size - at);
        if ((k = piece_at(from, at, a, n)) != n) {
            r = -1, why = "reading it";
            break;
        }
        if (sums)
            sums[i] = piece_sum(a, n);
        if ((fd = open(to, at ? O_WRONLY : O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
            r = -1, why = "opening the copy", k = fd;
            break;
        }
        if (at && lseek(fd, (long)at, SEEK_SET) != (long)at) {
            close(fd);
            r = -1, why = "moving in the copy";
            break;
        }
        for (k = 0; k < n && (w = write(fd, a + k, n - k)) > 0; k += w)
            ;
        if (k < n)
            r = -1, why = "writing the copy";
        /* (the sd2psx's close can answer above 0 for a file it has written whole: only below 0 is a failure, and what
         * was written is read back below anyway) */
        if ((w = close(fd)) < 0 && r == 0)
            r = -1, why = "closing the copy";
        else if (w > 0 && !at)
            log_msg("files: closing %s answered %d", to, w);
        if (progress)
            progress(at + n, size * 2);
    }
    for (at = 0, i = 0; at < size && r == 0; at += n, i++) {   /* read back, and compared with the original */
        n = size - at > MOVE_PIECE ? MOVE_PIECE : (int)(size - at);
        if ((k = piece_at(to, at, b, n)) != n || (sums ? piece_sum(b, n) != sums[i]
                                                      : piece_at(from, at, a, n) != n || memcmp(a, b, n) != 0))
            r = -1, why = "comparing it with the copy";
        if (progress)
            progress(size + at + n, size * 2);
    }
    free(sums);
    if (r != 0) {
        unlink(to);
        log_msg("files: %s not copied to %s: %s, at %lld of %lld (%d, %d)", from, to, why ? why : "?", at, size, k, w);
    } else if (size >= MOVE_PIECE)
        log_msg("files: %s copied, %lld bytes in %u ms", to, size, (unsigned)(now_ms() - start));
    return r;
}

int file_copy(const char *from, const char *to) { return file_copy_shown(from, to, NULL); }

int file_move(const char *from, const char *to)
{
    if (file_exists(to))
        return -1;
    if (rename(from, to) == 0)
        return 0;
    /* a file system that can't rename (an sd2psx before its firmware 1.4.0): copied, and only then deleted */
    if (file_copy(from, to) != 0)
        return -1;
    if (unlink(from) != 0 && file_exists(from))   /* (it is where it was meant to go, and still where it was: a copy) */
        log_msg("files: %s copied to %s, but couldn't be deleted", from, to);
    return 0;
}

static int by_kind_and_name(const void *a, const void *b)
{
    const dir_entry_t *x = a, *y = b;
    return x->dir != y->dir ? y->dir - x->dir : strcasecmp(x->name, y->name);
}

/* through fileXio itself: one call gives each entry's name, kind and size (readdir gives only the name, and asking
 * for the rest file by file would take long on the sd2psx). The folder is read whole and closed before returning */
int dir_list(const char *path, dir_entry_t *list, int max)
{
    static iox_dirent_t e;
    char c[400];
    size_t len;
    int fd, n = 0;
    snprintf(c, sizeof(c), "%s", path);
    len = strlen(c);
    if (len > 1 && c[len - 1] == '/' && c[len - 2] != ':')   /* "dev:/folder", but "dev:/" for the root */
        c[len - 1] = 0;
    fd = fileXioDopen(c);
    if (fd < 0)
        return -1;
    while (n < max && fileXioDread(fd, &e) > 0) {
        if (!strcmp(e.name, ".") || !strcmp(e.name, ".."))
            continue;
        if (!strcmp(c, "hdd0:")) {   /* a hard disk formatted APA: its partitions with files, as folders */
            if (e.stat.mode != 0x0100 || (e.stat.attr & 1))   /* (PFS; not a partition's extension) */
                continue;
            snprintf(list[n].name, sizeof(list[0].name), "%s", e.name);
            list[n].dir = 1;
            list[n].size = (long long)e.stat.size * 512;   /* (its size, in sectors) */
            memset(list[n].mtime, 0, sizeof(list[0].mtime));
            n++;
            continue;
        }
        snprintf(list[n].name, sizeof(list[0].name), "%s", e.name);
        list[n].dir = FIO_S_ISDIR(e.stat.mode) ? 1 : 0;
        list[n].size = ((long long)e.stat.hisize << 32) | e.stat.size;
        memcpy(list[n].mtime, e.stat.mtime, sizeof(list[0].mtime));
        n++;
    }
    fileXioDclose(fd);
    qsort(list, n, sizeof(list[0]), by_kind_and_name);
    return n;
}

void ensure_data_dir(void)
{
    char c[40];
    size_t n = strlen(dataDir);
    snprintf(c, sizeof(c), "%.*s", (int)(n && dataDir[n - 1] == '/' ? n - 1 : n), dataDir);
    mkdir(c, 0777);   /* fine if it already exists */
}

void ensure_dir(const char *dir)
{
    char c[260];
    size_t i;
    for (i = 0; dir[i] && i < sizeof(c) - 1; i++) {
        c[i] = dir[i];
        if (dir[i] == '/' && i > 0 && dir[i - 1] != ':') {   /* each level; not the device's root */
            c[i] = 0;
            mkdir(c, 0777);   /* fine if it already exists */
            c[i] = '/';
        }
    }
}

int file_exists(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

void trim(char *s)
{
    char *p = s, *e;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
}

static int utf8_valid(const unsigned char *s)
{
    while (*s) {
        int extra = *s < 0x80 ? 0 : (*s & 0xE0) == 0xC0 ? 1 : (*s & 0xF0) == 0xE0 ? 2 : (*s & 0xF8) == 0xF0 ? 3 : -1;
        if (extra < 0)
            return 0;
        for (s++; extra; extra--, s++)
            if ((*s & 0xC0) != 0x80)
                return 0;
    }
    return 1;
}

void utf8_fix(char *s, size_t size)
{
    char copy[256];
    const unsigned char *p;
    size_t n = 0;
    if (utf8_valid((const unsigned char *)s))
        return;
    snprintf(copy, sizeof(copy), "%s", s);
    for (p = (const unsigned char *)copy; *p && n + 2 < size; p++) {
        if (*p < 0x80)
            s[n++] = *p;
        else {   /* a Latin-1 letter: two bytes in UTF-8 */
            s[n++] = 0xC0 | (*p >> 6);
            s[n++] = 0x80 | (*p & 0x3F);
        }
    }
    s[n] = 0;
}

int file_recover(const char *path)
{
    char tmp[260];
    buffer_t b = {0};
    snprintf(tmp, sizeof(tmp), "%s.new", path);   /* (the new one, read back when it was written; else the old one) */
    if (!file_exists(tmp)) {
        snprintf(tmp, sizeof(tmp), "%s.old", path);
        if (!file_exists(tmp))
            return 0;
    }
    if (file_exists(path))
        return 0;
    if (rename(tmp, path) != 0 && file_read(tmp, &b) == 0 && file_write_checked(path, b.data, b.len))
        unlink(tmp);
    buf_free(&b);
    log_msg("files: %s was only %s, as file_replace left it: %s", path, tmp,
            file_exists(path) ? "put back in its place" : "it can't be put back");
    return file_exists(path) ? 0 : -1;
}

int ini_read(const char *path, void (*cb)(const char *section, const char *key, const char *value, void *u), void *u)
{
    buffer_t b = {0};
    char section[64] = "", *line, *next, tmp[260], old[260];
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    snprintf(old, sizeof(old), "%s.old", path);
    if (file_read(path, &b) != 0 &&
        (file_recover(path) != 0 ? file_read(tmp, &b) != 0 && file_read(old, &b) != 0 : file_read(path, &b) != 0))
        return -1;
    /* skip the UTF-8 BOM, if the file was saved with Notepad */
    line = (char *)b.data;
    if (b.len >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
        line += 3;
    for (; line && *line; line = next) {
        char *eq, *comment;
        next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        /* comment: ; or # at the start, or " ;" in the middle */
        if ((comment = strstr(line, " ;")) != NULL || (comment = strstr(line, "\t;")) != NULL)
            *comment = 0;
        trim(line);
        if (!*line || *line == '#' || *line == ';')
            continue;
        if (*line == '[') {
            char *e = strchr(line, ']');
            if (e) {
                *e = 0;
                snprintf(section, sizeof(section), "%s", line + 1);
                trim(section);
            }
            continue;
        }
        if (!(eq = strchr(line, '=')))
            continue;
        *eq = 0;
        trim(line);
        trim(eq + 1);
        cb(section, line, eq + 1, u);
    }
    buf_free(&b);
    return 0;
}

void hex_of(const unsigned char *d, size_t n, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2] = digits[d[i] >> 4];
        out[i * 2 + 1] = digits[d[i] & 15];
    }
    out[n * 2] = 0;
}

void sha256_hex(const unsigned char *d, size_t n, char *out)
{
    wc_Sha256 s;
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    wc_InitSha256(&s);
    wc_Sha256Update(&s, d, n);
    wc_Sha256Final(&s, h);
    wc_Sha256Free(&s);
    hex_of(h, sizeof(h), out);
}
