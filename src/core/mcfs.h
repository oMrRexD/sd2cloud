/* SD2Cloud -- the memory card's file system, inside a card's file (mcfs.c) */
#ifndef MCFS_H
#define MCFS_H

#include <stddef.h>
#include <tamtypes.h>
#include "files.h"

/* fingerprint of the card's file system index (root and folder entries). 0 = ok */
int mcfs_fingerprint(const char *path, char hex[65], int *saves);
/* the same, and from the same reading the signature of its root folder (mcfs_root_signature's) */
int mcfs_fingerprint_root(const char *path, char hex[65], int *saves, char rootSig[65]);
/* signature of the root folder only (name + modification time of each entry, in any order): the same one
 * helper.c computes from the memory card the PS2 sees, to tell whether a .mcd is the card the sd2psx is emulating */
#define ROOT_REC 40
int mcfs_root_signature(const char *path, char hex[65]);
void mcfs_sign_records(unsigned char (*rec)[ROOT_REC], int n, char hex[65]);
/* the newest save of the card that has an icon (by modification time, not the B?DATA-SYSTEM folders): its folder and
 * the contents of its icon.sys and of the 3D icon that icon.sys names. 0 = ok */
/* rank(folder) = how much that save is what the card is of: the newest one of the highest rank that has an icon is
 * taken (NULL = the newest one) */
int mcfs_newest_save_icon(const char *path, int (*rank)(const char *folder), char folder[33], buffer_t *iconsys, buffer_t *ico);
/* the saves of a card (its folders, not the B?DATA-SYSTEM ones), the newest first as in the PS2 browser, and its
 * free space (-1 = unknown). Returns how many, or -1 */
#define MCFS_MAX_SAVES 256
typedef struct {
    char folder[33];
    unsigned long long when;       /* modification time, comparable */
    unsigned int cluster, count;   /* where the folder is and how many entries it has */
} mcfs_save_t;
int mcfs_list_saves(const char *path, mcfs_save_t *list, int max, long long *freeBytes);
const char *mcfs_last_error(void);         /* why the last card that couldn't be read couldn't, for the log */
/* the icon.sys and the 3D icon of a save from that list. 0 = ok */
int mcfs_save_icon(const char *path, const mcfs_save_t *save, buffer_t *iconsys, buffer_t *ico);
/* Is that save a program to start, the way the Save Application System keeps one: a title.cfg whose "boot" line names
 * a file of the same folder? 1 = yes, and boot is that file's name as the folder has it */
int mcfs_save_app(const char *path, const mcfs_save_t *save, char *boot, size_t size);
/* What tells each of those saves of a card (as mcfs_list_saves gave them) from another save of the same folder: its
 * files' names and what is in them, in one number. A copy of a save has the same one; a save a game wrote again
 * doesn't, whatever its dates say (a console whose clock stands still dates every save the same). Every file is
 * read for it. sig[i] = 0: that save couldn't be read. Returns 0 when the card was opened */
int mcfs_save_signatures(const char *path, const mcfs_save_t *saves, int n, unsigned long long *sig);
/* changing a card (a .mcd that the sd2psx is NOT using right now): copy a save to another card (read back and
 * compared), delete a save, export a save as a .psu, import one from a .psu file (also read back and compared).
 * 0 = ok, else MCFS_ERR_* (BAD = the file isn't a .psu this can use) */
enum { MCFS_OK = 0, MCFS_ERR_IO = -1, MCFS_ERR_EXISTS = -2, MCFS_ERR_FULL = -3, MCFS_ERR_NOT_FOUND = -4, MCFS_ERR_CHECK = -5,
       MCFS_ERR_BAD = -6, MCFS_ERR_CANCELLED = -7 };
/* How a save's way into a card is going: it is read, written, and read back, and each of those tells its bytes as they
 * go by. Answering != 0 while it is read or written gives up: what was written of it gives its room back and the card
 * is left without the save (MCFS_ERR_CANCELLED). Once it is being read back there is nothing to give up */
enum { MCFS_STEP_READ, MCFS_STEP_WRITE, MCFS_STEP_CHECK };
typedef int (*mcfs_step_cb)(int phase, long long done, long long total);
int mcfs_save_info(const char *path, const char *folder, long long *bytes, int *files);
int mcfs_copy_save(const char *from, const char *folder, const char *to, mcfs_step_cb progress);
int mcfs_delete_save(const char *path, const char *folder);
int mcfs_export_psu(const char *path, const char *folder, buffer_t *out);
/* a new card in that file: empty and formatted, of that size (a power of two from 1 to 128 MB), as the PS2 formats
 * one; an 8 MB one is byte for byte the sd2psx's. progress is told how much of it is written. 0 = ok (what it has was
 * read back), else MCFS_ERR_* (BAD = not a size a card can be) */
#define MCFS_MIN_MB 1
#define MCFS_MAX_MB 128
#define MCFS_CARD_8MB (8LL * 1024 * 1024)
int mcfs_new_card(const char *path, long long size, void (*progress)(long long done, long long total));
/* what a card's folders take of it, in bytes: every cluster in use but the root folder's first (its saves, the
 * system's data, and whatever the FAT says is taken). -1 = it couldn't be read */
long long mcfs_card_used(const char *path);
/* A card's file system looked through, as a PS2 reads it, without anything written: every folder and file from the
 * root, the clusters each one's chain has, and the FAT. What it found: */
typedef struct {
    unsigned int clusters, used;   /* the card's clusters for folders and files; how many of them are in use */
    int folders, files;
    int badChains;    /* folders and files whose chain ends before they do, loops, or leaves the card */
    int crossLinked;  /* clusters that two of them have */
    int lost;         /* clusters the FAT has in use that no folder or file has */
    int badEntries;   /* entries of a folder that aren't what one can be (no name, a name with no end) */
} mcfs_check_t;
/* 0 = looked through (whatever it found: mcfs_check_ok); -1 = it couldn't be read. Not for a Memory Card in a
 * slot: mcman reads those */
int mcfs_check(const char *path, mcfs_check_t *r);
#define mcfs_check_ok(r) (!(r)->badChains && !(r)->crossLinked && !(r)->lost && !(r)->badEntries)
/* Every folder of a card (its saves and the system's B?DATA-SYSTEM) copied into another card, each read back and
 * compared: a card made again of another size. progress(i, n, folder) before each one (and with i = n at the end)
 * gives up when it answers != 0 (MCFS_ERR_CANCELLED: the folders copied so far stay). A card with a file at its root,
 * or a folder inside a save, isn't copied whole: MCFS_ERR_BAD, or the save's error. MCFS_OK or MCFS_ERR_* */
int mcfs_copy_card(const char *from, const char *to, int (*progress)(int i, int n, const char *folder));
/* what an empty card of that size has for saves, in bytes, as the PS2 counts it (8 MB: 8000 KB). 0 = not a size */
long long mcfs_card_room(long long size);
/* what a save of those files (bytes, as their sizes add up) takes of a card at most: its folder's entries, each file
 * to the whole cluster, its entry in the root */
long long mcfs_save_room(long long bytes, int files);
/* the size (MB, from minMb to maxMb) for a new card that saves taking room go into: the smallest they take no more
 * than half of, else the smallest they fit in. 0 = none is big enough */
int mcfs_recommended_mb(long long room, int minMb, int maxMb);
/* what a .psu file holds: the save's folder, when it was last saved, its files' sizes added up, and its icon (the
 * buffers stay empty when it has none). Here and below, a path ending in / is a save copied as a folder (its files
 * as they were on the card), which is taken as its .psu would be */
typedef struct {
    char folder[33];
    unsigned long long when;
    long long bytes;
    int files;
} mcfs_psu_t;
int mcfs_psu_info(const char *psu, mcfs_psu_t *info, buffer_t *iconsys, buffer_t *ico);
int mcfs_import_psu(const char *psu, const char *to, mcfs_step_cb progress);
/* what a .psu holds, handed out piece by piece: first its folder (the folder's own 512-byte entry, as a card has it;
 * data NULL), then each of its files (its entry and its bytes). The callback stops it by answering != 0. Returns
 * MCFS_OK, MCFS_ERR_* for a file that isn't a .psu, or what the callback answered */
typedef int (*mcfs_psu_cb)(const unsigned char *entry, const unsigned char *data, unsigned int len, void *u);
int mcfs_psu_files(const char *psu, mcfs_psu_cb cb, void *u);
/* A card that isn't a .mcd of the microSD: a file of a folder (a .mcd, a MemCard PRO2's .mc2, or a .ps2, which has
 * the ECC bytes after each page), or a card held in memory (mem != NULL, len bytes; file is not used then). The
 * functions that only read a card take MCFS_IMAGE as its path from then on; it is never written to */
#define MCFS_IMAGE "image:"
void mcfs_image(const char *file, const unsigned char *mem, size_t len);
/* A PS2 memory card in a slot of the console, not a file: "mc0:" or "mc1:" as its path. It is read and written through
 * mcman's own calls (the platform's, given once with mcfs_slot), never by its pages: the functions above that take a
 * card's path take it too, and it is written to (a save copied, moved or imported into it, or deleted from it) */
#define MCFS_ENT 512
typedef struct {
    /* the entries of its root (folder NULL) or of a save's folder, as a card has them: the mode, the length, the two
     * dates and the name, the rest zeros. Returns how many, or -1 */
    int (*dir)(int port, const char *folder, unsigned char (*ent)[MCFS_ENT], int max);
    int (*read)(int port, const char *folder, const char *name, buffer_t *out);   /* a file of a save: 0 = ok */
    long long (*free_bytes)(int port);                                            /* -1 = unknown */
    int (*put)(int port, const char *psu);                                         /* a .psu into it: MCFS_OK or MCFS_ERR_* */
    int (*remove)(int port, const char *folder);                                   /* a save out of it: MCFS_OK or MCFS_ERR_* */
} mcfs_slot_t;
void mcfs_slot(const mcfs_slot_t *ops);
int mcfs_slot_port(const char *path);   /* 0 or 1 for "mc0:" and "mc1:"; -1 = a card's file */
/* a .psu held in memory: MCFS_PSU_MEMORY is taken as its path by the functions that read a .psu (until the next call) */
#define MCFS_PSU_MEMORY "psu:"
void mcfs_psu_memory(const unsigned char *d, size_t len);
/* what a card's first 340 bytes (its superblock) say: its size as a .mcd and its pages'. 0 = not a PS2 memory card */
long long mcfs_card_size(const unsigned char *sb, int *page);

#endif
