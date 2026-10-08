/* SD2Cloud -- a card restored from a backup, or installed from a file (restore.c) */
#ifndef RESTORE_H
#define RESTORE_H

#include <stddef.h>
#include <tamtypes.h>
#include "cards.h"
#include "google.h"

enum { RESTORE_DOWNLOAD, RESTORE_CHECK, RESTORE_WRITE, RESTORE_VERIFY, RESTORE_LOST };
/* replaces the card on the microSD with a backup from Drive, checked before and after writing.
 * progress(phase, done, total) != 0 cancels (only before RESTORE_WRITE).
 * 0 = restored; -2 = cancelled (the card didn't change); -1 = error (googleError says why) */
int restore_card(card_t *c, const drive_file_t *f, int (*progress)(int phase, long long done, long long total));
/* A card in a file of a folder of the microSD or of a USB drive: a .mcd, a MemCard PRO2's .mc2, a .ps2, or the .zip
 * "Copy to a device" writes with one of those inside */
typedef struct {
    char path[700];
    int zip, ecc;              /* it is a .zip; the card in the file has the ECC bytes of a .ps2 */
    long long size;            /* of the card, as a .mcd */
    unsigned char *image;      /* the card in memory, once it was read whole (a .zip's: when it is opened) */
    char sha[65];              /* its SHA-256, then */
} card_file_t;
/* Checks that the file is a PS2 memory card and tells mcfs of it: MCFS_IMAGE is this card until it is closed. A .zip's
 * card is inflated to memory (RESTORE_DOWNLOAD; progress != 0 cancels); when it doesn't fit there, image stays NULL
 * and the card can't be looked into, only installed. 0 = ok, -2 = cancelled, -1 = error (googleError says why) */
int card_file_open(card_file_t *f, const char *path, int (*progress)(int phase, long long done, long long total));
void card_file_close(card_file_t *f);
/* writes it as the .mcd of a card of the microSD (to->path; the caller makes sure the sd2psx isn't using it) and reads
 * it back. The same phases and answers as restore_card, and two things more. A card that doesn't fit in memory
 * goes straight from its file, with no RESTORE_DOWNLOAD: a piece of it that can't be read is asked about,
 * progress(RESTORE_LOST) != 0 gives the file up (-1) and 0 has it tried again. And progress != 0 while it is written
 * or read back gives up as well (-2), which is for the caller to allow only of a card that wasn't there, and to
 * delete it then. A card that was there and didn't get written whole is left empty */
int card_file_install(card_file_t *f, card_t *to, int (*progress)(int phase, long long done, long long total));

#endif
