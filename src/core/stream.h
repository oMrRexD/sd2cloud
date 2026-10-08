/* SD2Cloud -- a card read and handed out as a .zip, piece by piece (stream.c) */
#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <tamtypes.h>
#include "system.h"
#include "cards.h"

/* reads the .mcd and hands out the .zip in CHUNK pieces (a multiple of 256 KB; the last one smaller) */
#define CHUNK (1024 * 1024)
typedef int (*chunk_cb)(const unsigned char *d, size_t n, int last, void *u);   /* 0 = keep going */
typedef struct {
    long long read, total;     /* of the .mcd */
    long long sent;            /* of the .zip */
    char sha_mcd[65], sha_zip[65];
    int (*progress)(long long read, long long total);    /* after each block read (may be NULL); != 0 stops */
} stream_t;
int stream_zip(const card_t *c, const datetime_t *t, stream_t *s, chunk_cb cb, void *u);
/* the card's file copied to dest (in a folder of the microSD or of a USB drive) inside a .zip, as .mcd or as .ps2;
 * the .zip is read back and compared. 0 = ok */
int card_export(const card_t *c, const char *dest, int ps2, int (*progress)(long long done, long long total));

#endif
