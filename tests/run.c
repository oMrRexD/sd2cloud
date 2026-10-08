/* SD2Cloud's regression tests -- runs every suite and says how it went. It exits with an error when a check fails,
 * which is what stops a build on GitHub. Run from the folder it may fill and empty (the Makefile uses build/work). */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "t.h"

int tChecks, tFailed;
const char *tName = "";

void t_fail(const char *file, int line, const char *what)
{
    tFailed++;
    fprintf(stderr, "FAILED  %s (%s:%d): %s\n", tName, file, line, what);
}

static void wipe(const char *path)
{
    char c[600];
    struct dirent *e;
    DIR *d = opendir(path);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(c, sizeof(c), "%s/%s", path, e->d_name);
        if (lstat(c, &st) == 0 && S_ISDIR(st.st_mode)) {
            wipe(c);
            rmdir(c);
        } else
            unlink(c);
    }
    closedir(d);
}

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

void t_fresh(void)
{
    wipe(".");
    t_mkdir("sd/SD2Cloud");
    hostMallocLimit = 0;
    googleError[0] = 0;
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

int main(void)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } suites[] = {{"host", suite_host},   {"json", suite_json},   {"ini", suite_ini},             {"state", suite_state},
                  {"mcfs", suite_mcfs},   {"cards", suite_cards}, {"card file", suite_card_file}};
    size_t i;
    for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        int before = tChecks, failed = tFailed;
        suites[i].run();
        printf("%-10s %4d checks%s\n", suites[i].name, tChecks - before, tFailed > failed ? "   <-- FAILED" : "");
    }
    printf("%d checks, %d failed\n", tChecks, tFailed);
    return tFailed ? 1 : 0;
}
