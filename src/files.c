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
    int fd, n;
    buf_free(b);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return fd;
    while ((n = read(fd, block, sizeof(block))) > 0) {
        if (buf_append(b, block, n)) {
            n = -1;
            break;
        }
    }
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
    char tmp[260];
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    unlink(tmp);
    if (!file_write_checked(tmp, d, n)) {
        unlink(tmp);
        return -1;
    }
    unlink(path);
    if (rename(tmp, path) == 0)
        return 0;
    /* a file system that can't rename (the sd2psx's): written again in its place; the .new stays if that fails */
    if (!file_write_checked(path, d, n))
        return -1;
    unlink(tmp);
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
        snprintf(list[n].name, sizeof(list[0].name), "%s", e.name);
        list[n].dir = FIO_S_ISDIR(e.stat.mode) ? 1 : 0;
        list[n].size = ((long long)e.stat.hisize << 32) | e.stat.size;
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

int ini_read(const char *path, void (*cb)(const char *section, const char *key, const char *value, void *u), void *u)
{
    buffer_t b = {0};
    char section[64] = "", *line, *next;
    if (file_read(path, &b) != 0)
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

void sha256_hex(const unsigned char *d, size_t n, char *out)
{
    wc_Sha256 s;
    unsigned char h[WC_SHA256_DIGEST_SIZE];
    int i;
    wc_InitSha256(&s);
    wc_Sha256Update(&s, d, n);
    wc_Sha256Final(&s, h);
    wc_Sha256Free(&s);
    for (i = 0; i < WC_SHA256_DIGEST_SIZE; i++)
        sprintf(out + i * 2, "%02x", h[i]);
}
