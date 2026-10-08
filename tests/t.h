/* SD2Cloud's regression tests -- the little they share: checks that count and say where they failed, and a few
 * helpers to make and compare files. Each test_*.c has one suite_*() that runs its tests; run.c calls them all. */
#ifndef T_H
#define T_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern int tChecks, tFailed;
extern const char *tName;

void t_fail(const char *file, int line, const char *what);

#define CHECK(cond)                                  \
    do {                                             \
        tChecks++;                                   \
        if (!(cond))                                 \
            t_fail(__FILE__, __LINE__, #cond);       \
    } while (0)

#define CHECK_INT(got, want)                                                              \
    do {                                                                                  \
        long long g_ = (long long)(got), w_ = (long long)(want);                          \
        tChecks++;                                                                        \
        if (g_ != w_) {                                                                   \
            char m_[200];                                                                 \
            snprintf(m_, sizeof(m_), "%s is %lld, not %lld (%s)", #got, g_, w_, #want);   \
            t_fail(__FILE__, __LINE__, m_);                                               \
        }                                                                                 \
    } while (0)

#define CHECK_STR(got, want)                                                      \
    do {                                                                          \
        const char *g_ = (got), *w_ = (want);                                     \
        tChecks++;                                                                \
        if (strcmp(g_, w_) != 0) {                                                \
            char m_[400];                                                         \
            snprintf(m_, sizeof(m_), "%s is \"%s\", not \"%s\"", #got, g_, w_);   \
            t_fail(__FILE__, __LINE__, m_);                                       \
        }                                                                         \
    } while (0)

#define RUN(test)        \
    do {                 \
        tName = #test;   \
        t_fresh();       \
        test();          \
    } while (0)

/* the folder the tests work in is emptied and made again, with the "microSD" (sd/) and its SD2Cloud folder in it */
void t_fresh(void);
void t_mkdir(const char *path);                               /* with the folders above it */
void t_write(const char *path, const void *d, size_t n);      /* a whole file (the test stops if it can't) */
void t_text(const char *path, const char *text);
buffer_t t_read(const char *path);                            /* a whole file (empty when it isn't there) */
long long t_size(const char *path);                           /* -1 = it isn't there */
int t_same(const char *a, const char *b);                     /* do both files hold the same bytes? */
void t_fill(unsigned char *d, size_t n, unsigned int seed);   /* bytes that look random and are always the same */
/* a .psu of that save folder, with n files (file0.bin...) of those sizes: file i holds t_fill(seed + i) */
void make_psu(const char *path, const char *folder, const unsigned int *sizes, int n, unsigned int seed);

/* what the tests can ask of the stand-ins for the PS2 (host/host.c) */
extern size_t hostMallocLimit;   /* a malloc bigger than this fails (0 = none does): "it doesn't fit in memory" */

void suite_host(void);
void suite_json(void);
void suite_ini(void);
void suite_state(void);
void suite_mcfs(void);
void suite_cards(void);
void suite_card_file(void);
void suite_templates(void);

#endif
