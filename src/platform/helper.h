/* SD2Cloud -- what SD2Cloud keeps on the memory card for the automatic sync: the SAS package (helper.c) */
#ifndef HELPER_H
#define HELPER_H

#include <stddef.h>
#include <tamtypes.h>
#include "files.h"

/* What SD2Cloud keeps on the memory card in use for the automatic sync, the "SAS package" of the screens: the
 * APP_SD2CLOUD save folder, with the IGR helper, the shortcut that opens SD2Cloud, an icon and a title.cfg, all
 * embedded in the program. A helper that a version up to 1.5 left alone in BOOT is also there (helperLegacy = 1, set
 * by helper_status): it is written again with the folder, so the path OPL was given keeps working */
enum { HELPER_NO_FILE, HELPER_NOT_INSTALLED, HELPER_DIFFERENT, HELPER_SAME };
int helper_status(void);                    /* what the card has, compared with what this version would write */
extern int helperLegacy;
const char *helper_path(void);              /* the helper on the card, for OPL: "/APP_SD2CLOUD/IGR.ELF" */
const char *helper_place(void);             /* where it is installed: "/APP_SD2CLOUD" */
int mc_root_signature(int port, char hex[65]);   /* mcfs_root_signature of the card in that slot, through mcman */
/* The card in a slot changed as a game changes it, through mcman: what there is for the card the device is using,
 * which it keeps in its own memory and whose file can't be touched. A save folder: is it there (1, 0; -1 = the card
 * can't be asked), deleted (0 = gone), and put there from a .psu, each file read back and with the dates it had
 * (MCFS_OK, or MCFS_ERR_*: EXISTS = the card has that folder, FULL = no room; nothing of it is left then) */
int mc_slot(void);                               /* the slot the device is in */
int mc_has_folder(int port, const char *folder);
int mc_delete_save(int port, const char *folder);
int mc_put_save(int port, const char *psu);
int mc_card_state(int port);                     /* 0 = the card in that slot is the one it was when last asked */
int helper_present(void);                   /* it can be installed (always, unless the program can't be found by it) */
int helper_install(void);                   /* writes it to mc0: 0 = ok, -1 = couldn't write, -3 = not enough room
                                               (helperNeedKb, helperFreeKb) */
/* what installing takes on the memory card in use and what the card has free, for the question before installing:
 * fills helperNeedKb and helperFreeKb (-1 = unknown) */
int helper_space(void);
int helper_uninstall(void);                 /* removes it from mc0: 0 = removed (or it wasn't there) */
extern int helperNeedKb, helperFreeKb;
/* the program on a memory card (appOnCard) */
int card_app_helper(void);                  /* 1 = the IGR helper is in the program's folder, where OPL can run it */
int card_app_read(buffer_t *b);             /* the program itself, whole, as it is on the card. 0 = read */
/* A new version written to that folder: the helper (left alone when it is the same) and the program, each one next
 * to the old one, read back and only then in its place; title.cfg gets the version ("1.6") and its date ("October 7,
 * 2026"; "" = unknown, for either: that line stays as it is), and the folder keeps its dates. progress is told how far the writing and the reading back
 * are. 0 = done; -3 = no room for the new program next to the old one, nothing was touched (helperNeedKb,
 * helperFreeKb); -1 = couldn't write */
int card_app_update(const buffer_t *app, const buffer_t *helper, const char *version, const char *released,
                    void (*progress)(long long done, long long total));

#endif
