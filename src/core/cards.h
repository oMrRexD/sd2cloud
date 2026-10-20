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
    int resolved;          /* marked as resolved as it is now (card_state_t.resolved): the automatic sync leaves it */
    int rereads;           /* ST_ERROR: how many times it was read again while the list of cards waited */
} card_t;
/* NEW: a card the app has never seen (e.g. the card of a new game); NO_BACKUP: known and unchanged since it was
 * seen, but never uploaded (the user said "later" the first time); CHANGED: different from the last backup (or from
 * when it was seen); UP_TO_DATE: same as the last backup. IGR sends NEW and CHANGED; manual sends everything that is
 * not UP_TO_DATE. */
enum { ST_NEW, ST_CHANGED, ST_UP_TO_DATE, ST_NO_BACKUP, ST_ERROR };
extern card_t cards[MAX_CARDS];
/* the cards listed are those of the device that isn't in the slot (a microSD with both devices' cards): never synced,
 * and nothing of them is kept in the state, whose names are the device's own cards' */
extern int cardsOther;
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
/* the name the sd2psx shows for the card (its folder's .ini, [ChannelName]: the file keeps its own). 0 = written */
int card_set_name(card_t *c, const char *name);
/* can a folder of cards be called something else? Not one the sd2psx finds by its name (a numbered card's, the boot
 * cards', a game's ID): a folder of the user's own (named, or that Game2Folder.ini gives games to) */
int folder_renamable(const char *folder);
/* A folder of cards called name now (up to 15 characters, which the sd2psx reads, and a name folder_renamable takes):
 * the folder, and its files named after it (its cards, its .ini, its pictures). Renamed where that can be; else
 * every file copied and read back first, and only then the old ones deleted. Game2Folder.ini's games of it go with
 * it, and so do its cards in cards[] and in the state. 0 = done; -1 = not (a folder of that name is there, the name
 * can't be, or it couldn't be copied: it is as it was) */
int cards_rename_folder(const char *folder, const char *name);
/* A card's file deleted from the microSD, and the card out of cards[] (the ones after it move up: whatever holds on
 * to a card has to find it again) and out of the state; its backups stay on Drive. 0 = done */
int cards_delete(card_t *c);
/* a folder of cards deleted whole, with whatever else it has (its .ini, its pictures); its cards out of cards[] and
 * the state. 0 = done; -1 = it couldn't be read, or something was left in it (the cards gone are out anyway) */
int cards_delete_folder(const char *folder);
/* The OPL's VMCs on the microSD (VMC/<name>.bin, where the OPL keeps them on an MMCE) are cards too, of their own
 * kind (TYPE_VMC) and group, VMC_FOLDER: a name of its own (a folder of the sd2psx's cards may be called VMC), which
 * their backups' folder on Drive has too.
 * The OPL keeps them on the device it runs a game from: the VMC folders of the others (the hard disk's, an MX4SIO's, a
 * USB drive's) are the ones the user added in the settings ([vmc] in sd2cloud.ini). Each is a place of its own, with
 * a group of its own (VMC_FOLDER-HDD...); the OPL's settings of the games that use them are in the CFG folder beside
 * its VMC folder, as they are on the microSD */
#define VMC_FOLDER "OPL-VMC"
#define VMC_PLACES 6
typedef struct {
    char key[8];       /* its key in [vmc] ("" = the microSD's, always looked at) */
    char folder[48];   /* its VMCs' group: OPL-VMC, OPL-VMC-HDD... */
    char dir[160];     /* its VMC folder as it is read, without a / at the end: "mmce0:/VMC", "pfs1:/VMC" */
    char cfg[160];     /* and the OPL's settings of the games, beside it: "mmce0:/CFG" */
} vmc_place_t;
extern vmc_place_t vmcPlaces[VMC_PLACES];
extern int nVmcPlaces;   /* the places whose VMC folder is there (the VMCs' group is shown, to make one in it) */
/* the folders the user added, found on their devices (the app's, which loads their drivers; none without it): up to
 * max in more, with their key, folder, dir and cfg. Returns how many */
extern int (*vmcMore)(vmc_place_t *more, int max);
const vmc_place_t *vmc_place(const char *folder);   /* a group's place (NULL = none is) */
/* the OPL's settings of its games, in each place (CFG/<game>.cfg): which VMC each one uses ($VMC_0, $VMC_1). They
 * take seconds on an sd2psx's microSD (a file each), so they are read a few at a time while the screens wait for a
 * button: vmc_links_step reads for ms milliseconds at most, and returns 1 once all are read (vmc_links_read reads
 * them all at once). Read once, until the cards are looked for again */
int vmc_links_step(int ms);
void vmc_links_read(void);
/* a VMC just made (<place's dir>/<name>.bin) joins cards[], at its end: no other card moves. Returns it, or NULL */
card_t *vmc_append(const char *folder, const char *name);
/* how many games use that VMC (-1 = their settings aren't all read yet); their IDs (SLUS-21065) in ids, up to max */
int vmc_games(const card_t *c, char (*ids)[12], int max);
/* a VMC called name now: its file, and the settings of the games that use it. 0 = done; -1 = not (it is as it was):
 * -2 = a VMC of that name is there */
int vmc_rename(card_t *c, const char *name);
int max_channels(const char *folder);       /* how many channels a folder of cards has (its .ini's MaxChannels, or 8) */
int max_channels_set(const char *folder, int n);   /* the sd2psx goes up to that many in that folder from now on. 0 = written */
/* the size, in MB, the sd2psx makes a card it doesn't find (a game's first one): its settings.ini's, which it reads
 * when it starts. 0 = not said there (it makes them of 8 MB then) */
int device_card_mb(void);
int device_card_mb_set(int mb);                    /* 0 = written: the sd2psx takes it the next time it starts */
/* fingerprint of each included card's index; progress(i, n) before each card */
void cards_check(void (*progress)(int i, int n, const card_t *c));
void cards_recheck(card_t *c);
/* the card was changed by SD2Cloud with nothing a backup needs (a template's saves; the same saves, made again on a
 * card of another size): it is taken as synced as it is now, when it was before (synced), and as resolved, when it
 * was (resolved) */
void card_keep_synced(card_t *c, int synced, int resolved);              /* one card again (after a save was copied in or out) */
/* The user turns a card's sync on or off, whatever the settings said of it: kept in them ([cards] exclude, which
 * leaves a card out whatever else they say, and include, which takes one in). A card whose whole folder was left
 * out comes back alone: the folder's other cards stay out, each by its own name. 0 = done and written */
int card_set_included(card_t *c, int on);

#endif
