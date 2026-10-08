/* SD2Cloud's regression tests -- the helpers of t.h: whole files written, read and compared, bytes that look random
 * and are always the same, and a save as a .psu file. Nothing here comes from a game. Shared by the tests (run.c)
 * and by the maker of the PCSX2 scenarios' cards (make_fixtures.c). */
#include <sys/stat.h>
#include "t.h"

#define ENT 512

void t_mkdir(const char *path)
{
    char c[600];
    size_t i;
    snprintf(c, sizeof(c), "%s/", path);
    for (i = 1; c[i]; i++)
        if (c[i] == '/') {
            c[i] = 0;
            mkdir(c, 0777);
            c[i] = '/';
        }
}

void t_write(const char *path, const void *d, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(d, 1, n, f) != n) {
        fprintf(stderr, "the tests can't write %s\n", path);
        exit(2);
    }
    fclose(f);
}

void t_text(const char *path, const char *text) { t_write(path, text, strlen(text)); }

buffer_t t_read(const char *path)
{
    buffer_t b = {0};
    file_read(path, &b);
    return b;
}

long long t_size(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long long)st.st_size : -1;
}

int t_same(const char *a, const char *b)
{
    buffer_t x = t_read(a), y = t_read(b);
    int same = x.len == y.len && x.len && !memcmp(x.data, y.data, x.len);
    buf_free(&x);
    buf_free(&y);
    return same;
}

void t_fill(unsigned char *d, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        seed = seed * 1664525u + 1013904223u;
        d[i] = (unsigned char)(seed >> 24);
    }
}

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

/* an entry as a memory card keeps it (and a .psu after it): 0x8427 = a folder, 0x8497 = a file */
static void entry(unsigned char *e, unsigned int mode, unsigned int len, const char *name)
{
    static const unsigned char when[8] = {0, 5, 4, 3, 2, 1, 0xEA, 0x07};   /* 2026-01-02 03:04:05 */
    memset(e, 0, ENT);
    e[0] = mode;
    e[1] = mode >> 8;
    put32(e + 4, len);
    memcpy(e + 8, when, 8);
    memcpy(e + 24, when, 8);
    snprintf((char *)e + 64, 32, "%s", name);
}

/* a .psu of that save folder, with n files (file0.bin...) of those sizes: file i holds t_fill(seed + i) */
void make_psu(const char *path, const char *folder, const unsigned int *sizes, int n, unsigned int seed)
{
    static const unsigned char pad[1024];
    unsigned char e[ENT];
    buffer_t b = {0};
    char name[32];
    int i;
    entry(e, 0x8427, n + 2, folder);
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, ".");
    buf_append(&b, e, ENT);
    entry(e, 0x8427, 0, "..");
    buf_append(&b, e, ENT);
    for (i = 0; i < n; i++) {
        unsigned char *d = malloc(sizes[i] + 1);
        snprintf(name, sizeof(name), "file%d.bin", i);
        entry(e, 0x8497, sizes[i], name);
        buf_append(&b, e, ENT);
        t_fill(d, sizes[i], seed + i);
        buf_append(&b, d, sizes[i]);
        if (sizes[i] % 1024)
            buf_append(&b, pad, 1024 - sizes[i] % 1024);
        free(d);
    }
    t_write(path, b.data, b.len);
    buf_free(&b);
}
