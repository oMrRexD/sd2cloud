/* SD2Cloud -- a buffer that grows, whole files read and written, an .ini changed in place, a folder's listing (files.c) */
#ifndef FILES_H
#define FILES_H

#include <stddef.h>
#include <tamtypes.h>

typedef struct {
    unsigned char *data;
    size_t len, cap;
} buffer_t;
int buf_append(buffer_t *b, const void *d, size_t n);   /* 0 = ok; keeps a 0 after the end */
void buf_free(buffer_t *b);
int file_read(const char *path, buffer_t *b);
int file_write(const char *path, const unsigned char *d, size_t n);
int file_replace(const char *path, const unsigned char *d, size_t n);   /* .new -> verify -> rename */
int file_write_checked(const char *path, const unsigned char *d, size_t n);   /* written and read back: 1 = it's there, as it should be */
int file_exists(const char *path);
/* one "key = value" of an .ini, changed or added (at the end of its section; a section that isn't there, at the end
 * of the file; a file that isn't there is made): everything else stays as it was. 0 = written */
int ini_set(const char *path, const char *section, const char *key, const char *value);
/* what a folder has (a path ending in /): the folders first, then the files, each by name. Returns how many (the
 * ones past max are left out), or -1 when it can't be read */
typedef struct {
    char name[256];
    int dir;
    long long size;
} dir_entry_t;
int dir_list(const char *path, dir_entry_t *list, int max);
void ensure_data_dir(void);                 /* creates <sd>SD2Cloud/ if needed */
void ensure_dir(const char *dir);           /* creates a folder (a path ending in /) and the ones above it, if needed */
/* reads an .ini: cb(section, key, value) for each "key = value" (section "" before the first one) */
int ini_read(const char *path, void (*cb)(const char *section, const char *key, const char *value, void *u), void *u);
void trim(char *s);
/* a text typed on a PC that isn't valid UTF-8 (a file saved as ANSI by an older editor) is taken as Latin-1 and
 * converted: its accents show on screen, and Google refuses a request that isn't UTF-8 */
void utf8_fix(char *s, size_t size);
void sha256_hex(const unsigned char *d, size_t n, char *out);

#endif
