/* SD2Cloud -- the list of the microSD's cards (cards.c) */
#ifndef CARDS_H
#define CARDS_H

#include <stddef.h>
#include <tamtypes.h>
#include "state.h"

typedef struct {
    char folder[48];       /* Card1, SLUS-21065, BOOT, MyCard */
    char base[56];         /* Card1-1, SLUS-21065-1, BootCard-1 */
    char id[96];           /* folder/base: the key in the config and in the state */
    char path[200];        /* the .mcd */
    char name[48];         /* channel name from the CardX.ini, if any */
    char game[64];         /* a game card's game, as the sd2psx names it ("" = not a game card, or not in the list) */
    char moved[48];        /* the folder Game2Folder.ini gives this card's game now, when that is another one: the
                              device no longer opens this card for the game ("" = it does) */
    char rootSig[65];      /* its root folder's signature, from when its index was last read ("" = not read): the card
                              in the device is told from the others by it */
    int type, channel;
    long long size;
    /* computed */
    int included;          /* part of the backup according to the config */
    char fingerprint[65];
    int status;            /* ST_* */
} card_t;
/* NEW: a card the app has never seen (e.g. the card of a new game); NO_BACKUP: known and unchanged since it was
 * seen, but never uploaded (the user said "later" the first time); CHANGED: different from the last backup (or from
 * when it was seen); UP_TO_DATE: same as the last backup. IGR sends NEW and CHANGED; manual sends everything that is
 * not UP_TO_DATE. */
enum { ST_NEW, ST_CHANGED, ST_UP_TO_DATE, ST_NO_BACKUP, ST_ERROR };
extern card_t cards[MAX_CARDS];
extern int nCards;
int cards_scan(void);                       /* lists the .mcd files on the microSD; returns how many (-1 = no SD) */
/* a card just written to the microSD (the name of its folder and of its file) joins the list, in its place: the other
 * cards keep what is known of them, but may have moved in cards[]. Returns it, or NULL */
card_t *cards_add(const char *folder, const char *file);
/* the same, at the end of the list: no other card moves, so whatever is holding on to one keeps the right one.
 * cards_sort puts the list in order again, when nothing is */
card_t *cards_append(const char *folder, const char *file);
void cards_sort(void);
int is_game_id(const char *p);              /* SLUS-21065: the way a game's ID names its folder */
int game_title(const char *id, char *out, size_t size);   /* the game of that ID in the sd2psx's list. 1 = it's there */
/* where the sd2psx keeps a game's cards: the folder Game2Folder.ini gives that ID, or one named after the ID */
void game_folder(const char *id, char *out, size_t size);
int max_channels(const char *folder);       /* how many channels a folder of cards has (its .ini's MaxChannels, or 8) */
int max_channels_set(const char *folder, int n);   /* the sd2psx goes up to that many in that folder from now on. 0 = written */
/* fingerprint of each included card's index; progress(i, n) before each card */
void cards_check(void (*progress)(int i, int n, const card_t *c));
void cards_recheck(card_t *c);              /* one card again (after a save was copied in or out) */
/* The user turns a card's sync on or off, whatever the settings said of it: kept in them ([cards] exclude, which
 * leaves a card out whatever else they say, and include, which takes one in). A card whose whole folder was left
 * out comes back alone: the folder's other cards stay out, each by its own name. 0 = done and written */
int card_set_included(card_t *c, int on);

#endif
