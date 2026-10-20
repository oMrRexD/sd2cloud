/* SD2Cloud -- the places on Drive, in its second layout, and how the first layout's files go into them (drivemap.c) */
#ifndef DRIVEMAP_H
#define DRIVEMAP_H

#include <stddef.h>

/* The first layout: the main folder (cfg.drive_folder, "PS2 Memory Card Backups"), with a folder of each group of cards
 * in it, all side by side (Card1, SLUS-21065, OPL-VMC, Memory Card 1, Library); each card's backups (.zip) in its
 * group's folder, with the other cards of the group, and the saves sent by themselves in the group's Saves folder (the
 * Library's in its own folder).
 *
 * The second: the main folder (SD2Cloud); in it, the device's folder (sd2psx, MemCard PRO2) with its groups (a game's
 * named after the game) and, in each group, a folder for each card, with its own Saves; beside the device's, a folder
 * for the OPL's VMCs (each VMC a card's folder in it), one for the Memory Card in a slot (it is a card's folder) and the
 * Library's, with a folder for each game. Each folder of the second layout is tagged on Drive with its key, the same in
 * every language, and found by it: its name is only for whoever looks at Drive, in the language of the screens when
 * it was made */

#define DRIVE_KEY 160
#define DRIVE_DEPTH 4

enum { DK_DEVICE, DK_VMC, DK_SLOT, DK_LIBRARY };   /* what a card is, for where it goes */
enum { DL_DEVICE, DL_GROUP, DL_CARD, DL_SAVES };   /* what a folder is, for its name */

typedef struct {
    int kind;
    char device[24];   /* DK_DEVICE: the device's name (sd2psx, MemCard PRO2) */
    char group[48];    /* the card's group, named as the first layout's folder (Card1, SLUS-21065, OPL-VMC-HDD,
                          Memory Card 1); DK_LIBRARY: the game's ID, or LIBRARY_OTHER */
    char base[64];     /* the card (Card1-1). "" = only as far as its group (a file of the first layout that tells no
                          card). Not used for DK_SLOT and DK_LIBRARY: the group is the card */
} drive_card_t;

typedef struct {
    char key[DRIVE_KEY];      /* with drivePrefix */
    char parent[DRIVE_KEY];   /* "" = the main folder */
    int level;                /* DL_* */
} drive_place_t;

/* before every key: "debug:" in the debug build, so that its folders are never the ones of the real backups */
extern const char *drivePrefix;

/* the main folder's key */
void drive_root_key(char out[DRIVE_KEY]);

/* the folders from the main one down to that card's own (saves = 1: down to its Saves; the Library has none, its
 * game's folder takes its saves), each with its key and its parent's. How many, at most DRIVE_DEPTH */
int drive_path(const drive_card_t *c, int saves, drive_place_t out[DRIVE_DEPTH]);

/* -------- the move from the first layout */

/* a file or folder as Drive lists it, under the main folder */
typedef struct {
    char id[48], parent[48], name[128];
    char key[DRIVE_KEY];   /* a folder of the second layout: its key ("" = a folder of the first) */
    char card[140];        /* a file: what it is a backup of (its appProperties "card"; "" = none) */
    char device[24];       /* a .zip: its device, from its description ("" = not said) */
    unsigned char folder;
    long long size;        /* a file's bytes (-1 = not said) */
} drive_node_t;

/* Where a file under the main folder goes: 1 = it is in a folder of the first layout (or in the main folder itself),
 * and goes to c (its Saves with *saves = 1); 0 = it is in place already (a folder of the second layout). root = the
 * main folder's id; device = the device a card's backups go under when nothing else tells (the one in use) */
int drive_plan_file(const drive_node_t *nodes, int n, int file, const char *root, const char *device, drive_card_t *c,
                    int *saves);

/* -------- a batch: up to DRIVE_BATCH calls to Drive in one request (multipart/mixed) */

#define DRIVE_BATCH 100

/* the request's body: each call's method, its path (/drive/v3/files/...?...) and its JSON (NULL = none), apart with
 * boundary. Returns the length written to out (0 = it doesn't fit in size) */
size_t drive_batch_body(char *out, size_t size, const char *boundary, int n, const char *const *method,
                        const char *const *path, const char *const *json);

/* its answer: each call's HTTP status in status[k] (0 = none came for it) and, when its JSON has one, the id of what
 * it made or changed in id[k]. Returns how many calls had an answer */
int drive_batch_read(const char *body, size_t len, int n, int *status, char (*id)[48]);

#endif
