/* SD2Cloud -- what the screens share (src/app): the state of the program, and what each file has that another one
 * uses. Each file says what it is for at its top; docs/ARCHITECTURE.md has the map. */
#ifndef APP_H
#define APP_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <kernel.h>
#include <libpad.h>
#include "common.h"
#include "ui.h"
#include "look.h"
#include "icon.h"
#include "sound.h"


#define MAX_RESULTS 24

typedef struct {
    char text[160];
    u32 color;
} result_t;

#define DLG_LINES 8
typedef struct {
    char title[200];
    u32 titleColor;
    struct {
        char text[320];
        u32 color;
        int font, gap;
    } line[DLG_LINES];
    int nlines, permille, wide;   /* permille < 0: no bar */
    char note[48];                /* under the bar, on the right ("1 of 3") */
    legend_t legend[3];
    int nlegend;
} dialog_t;

#define LIST_X     86     /* the list on the left */
#define ROW_Y0     158    /* the first row */
#define CARD_X     392
/* the right side's title, on the line of the tabs: at most this wide (panel_title), so the tabs end before it */
#define PANEL_TITLE_W 210
#define CARD_Y     100
#define CARD_CX    (CARD_X + LOOK_CARD_W / 2)

enum { TAB_CARDS, TAB_GAMES, TAB_BOOT, TAB_VMC, TAB_LIBRARY, TAB_FILES, TABS };
#define TAB_KEYS   (PAD_LEFT | PAD_RIGHT | PAD_L1 | PAD_R1)

/* the devices of the Files group: the sd2psx's own microSD and a USB drive */
enum { FDEV_SD, FDEV_USB, FDEV_HDD, FDEV_MX4SIO, FDEVS };
typedef struct {
    int tab;                  /* the group shown */
    int count[TABS];          /* how many cards each group has (the Files group: how many devices) */
    int n, idx[MAX_CARDS + 2];   /* the list's rows: cards (indexes in cards[]), ROW_ALL or ROW_NEW; of a list of
                                    folders, the first card of each folder */
    int fold[MAX_CARDS + 2];  /* of a list of folders: the folder each row is (-1 = a row that isn't one) */
    int lit;                  /* the row of the game of the save on its way (newGame), first and lit: its folder, or
                                 "New card" (-1 = none) */
    int cursor, top;          /* in the list */
    int keep[TABS][2];        /* each group's cursor and top: switching back lands on the same card */
    const card_t *skip;       /* a card left out (the one a save is copied from) */
    int files;                /* the Files group is offered too, with the devices */
    int library;              /* the Library's group: 1 = its games (the main screen), 2 = the Library itself, as a place
                                 saves go to; 0 = none */
    int memcard;              /* the Files group offers the Memory Card (memcard.c), after the devices if they are there:
                                 how many (with no sd2psx, one per slot), and the slots (mcPorts) */
    int mcPorts[2];
    int games[MAX_CARDS], nGames;         /* the game cards, by game and then as they come in cards[] */
    int first[MAX_CARDS + 1], folders;    /* where each folder starts in games[] (first[folders] = nGames) */
    char label[MAX_CARDS][64];            /* each folder's name, cut to fit the list */
    int open, openTop;        /* the folder that is open (-1 = none: the list is of folders) and the list's top then */
    float moved;              /* when a folder was last opened or closed (ui_clock): the list slides for a moment */
    int way;                  /* which way: 1 = into a folder, -1 = out of it */
    int add;                  /* 1 = every list of cards starts with a row that isn't one, "New card" (its idx is -1):
                                 a card file is being installed, as a new card or over one of these */
    int create;               /* 1 = the Cards group starts with "New card": a card is made there (the main screen) */
    int all;                  /* 1 = the folders of the Games group start with one, "All saves": every game card's
                                 saves on one screen (the main screen, with more than one game card) */
    char newGame[48];         /* a save is on its way to a card: its game's folder ("" = no game can be told). The
                                 Games group then offers "New card" for it: among the folders while the game has none
                                 there, inside its folder once it has (and that folder comes first) */
} tabs_t;
/* the rows of a list that are neither a card nor a folder (their idx) */
#define ROW_ALL (-1)              /* "All saves" */
#define ROW_NEW (-2)              /* "New card" */
#define ROW_LIB(k) (-100 - (k))   /* the Library's group: its k-th game (the main screen), or the Library (k = 0, a list
                                     a save is copied or moved to) */
typedef struct {
    mcfs_save_t s;
    card_t *card;          /* the card it is on */
    icon_t *icon;
    int tried;             /* the icon was read (or failed) */
    char line1[104], line2[104];   /* (icon_t's) */
} save_view_t;

/* the programs in the microSD's APPS (a folder with a title.cfg, as OPL's Apps tab shows them): where SD2Cloud can
 * go after IGR or when leaving. Listed once (appsListed) */
#define MAX_APPS 32
typedef struct {
    char title[64], path[200];
} app_t;

/* ------------------------------------------------------------ account.c */
int ensure_google(int allowLogin);
void account_screen(void);
int ask_connect(void);

/* ------------------------------------------------------------ active.c */
int card_in_use(const card_t *c, int active, int channel);
int active_card(int *channel);
void find_active(void);
int active_sure(char seen[65]);
int can_insert(const card_t *c);
int insert_card(const card_t *c);
extern const card_t *movedOff;
int card_free(const card_t *c, const card_t *other);
void card_back(void);
void insert_failed(const card_t *c);
void insert_option(const card_t *c);
extern jmp_buf reloadPoint;
#ifdef DEBUG_BUILD
extern u64 debugGone;
#endif
void device_watch(void);
extern int viewOther;                    /* the cards listed are the other device's (a microSD with both devices') */
void view_switch(void);                  /* the other device's cards listed, or the slot's again */
void view_draw(void);                    /* at the top right: the two devices, L2 and R2 (a microSD with both's cards) */
/* L2 or R2 pressed (VIEW_KEYS): the slot's device is on the left, the other on the right, as the tabs are (nothing past
 * either end). 1 = the other side's cards are listed now */
int view_key(u32 b);
#define VIEW_KEYS (PAD_L2 | PAD_R2)
void device_lost(void) __attribute__((noreturn));
void reload_all(void) __attribute__((noreturn));

/* ------------------------------------------------------------ autosync.c */
void igr(void);

/* ------------------------------------------------------------ backup.c */
int on_progress(long long done, long long total);
int is_selected(const card_t *c, int mode);
int card_folder(const card_t *c, char *folderId, size_t size);
void run_backup(int mode, int rotate);
void summary_screen(int seconds);
void manual_backup(int mode);
void count_cards(int *included, int *changed, long long *bytes);
void remember_unseen(void);
void first_run(void);
int sync_card(card_t *c);

/* ------------------------------------------------------------ browse.c */
struct fbGive_s {
    card_t *c;         /* a save's "Copy" picked a device: START in files_screen writes there what was marked */
    save_view_t *v;    /* the save, when one is marked (NULL = the marked saves, however many) */
    int done;          /* written */
};
extern struct fbGive_s fbGive;
extern card_t *fbCard;
struct fb_s {
    int dev;                   /* FDEV_* */
    char root[16], dir[400];   /* the device's root and the folder shown: a name can be appended to either */
    int parts;                 /* the hard disk formatted APA: its partitions are above the root (root "hdd0:" = the
                                  list of them, which isn't a folder: nothing is written there) */
    dir_entry_t *list;
    int n, cursor, top;
    int loading, error;        /* the folder is being read; it couldn't be read */
    char title[200];           /* the device and the folder, without the start of the path when it's too long */
    char selected[260];        /* the selected name, cut to fit (the others are cut as they're drawn) */
};
extern struct fb_s fb;
extern int fbExit;
extern int fbPick;
extern int (*fbHere)(const char *dir);   /* a folder picked: △ hands the one shown to it (1 = done with the folders) */
extern int fbHereText;
extern char fbPicked[200];
struct psu_s {
    save_view_t v;
    mcfs_psu_t info;
    char path[660];
};
extern struct psu_s psu;
void psu_close(void);
void files_screen(int dev);

/* ------------------------------------------------------------ memcard.c */
extern card_t slotCard;                  /* the Memory Card, in the console's other slot */
int memcard_there(void);
void memcard_screen(void);
void memcard_hint(int port);
void memcard_upload(void);
/* whose backups the Memory Card's are, on Drive and in the state: "slot/memcard" (the slot the device isn't in);
 * with no sd2psx, "slot/memcard1" or 2, by its slot */
const char *memcard_id(void);
/* the slots with a Memory Card that can be offered (with an sd2psx, the other one; with none, both): how many, their
 * ports in ports. memcard_pick: which of them memcard_there takes (slotCard) */
int memcard_ports(int ports[2]);
void memcard_pick(int port);
void memcard_history(void);         /* its backups on Drive, to put one back */
int memcard_restore(card_t *c, const drive_file_t *f);   /* that backup's saves into it, asked first. 1 = done */               /* the whole card, as a backup of its own on Drive */

/* ------------------------------------------------------------ cardops.c */
typedef struct {
    card_t *card;          /* a save, by the card it is on and its folder */
    const char *folder;
} save_ref_t;
/* a new card, with those saves in it (n = 0: empty): its place picked (a group of the device, or a file of a
 * folder), and its size. 1 = it was made (a card of the device: cards[] has it at its end, cardsUnsorted) */
int card_create(const save_ref_t *saves, int n);
void card_resize_screen(card_t *c);
void card_check_screen(card_t *c);
/* safety.c: a copy of that card's file kept in the data folder (Safety/) before it is deleted, made again of another
 * size or has its saves moved off it; the newest ten stay. 0 = kept, or the user goes on without one (asked);
 * -1 = given up */
int safety_copy(const card_t *c);
void safety_screen(void);
/* savesync.c, the backup by save: the saves of that card that changed since they were last sent, each sent by itself
 * as a .psu to the card's Saves folder on Drive (folderId = the card's folder), the newest of each kept. How many were
 * sent; -1 = they couldn't be */
int saves_backup(card_t *c, const char *folderId);
void save_versions_screen(card_t *c, save_view_t *v);   /* "Versions on Drive": one of them put back in its place */
int sync_stop(void);            /* (backup.c) the sync was cancelled, or the card on its way skipped: 1 */   /* Tools > "Safety copies": each one put back as its card, or deleted */
void format_when(int year, int month, int day, int hour, int minute, char *out, size_t size);   /* (format.c) */   /* "Check card": its file system looked through, read only, and what was found */
/* "New VMC": an OPL VMC made in the microSD's VMC folder, of the sizes the OPL makes (written whole, at once: the OPL
 * doesn't take one whose file is in pieces), with those saves in it (n = 0: empty). 1 = made */
int vmc_create(const save_ref_t *saves, int n);      /* the card made again of another size, picked and asked */
int card_rename_screen(card_t *c);       /* its name on the sd2psx, or its folder's: 1 = cards[] changed */
int card_delete(card_t *c);              /* asked, and deleted: 1 = cards[] changed (the main screen lists them again) */
int folder_delete(card_t *c);            /* that card's whole folder, the same way */

/* ------------------------------------------------------------ migrate.c */
void migrate_screen(card_t *src);        /* its saves, each to its game's card (Game ID): listed, picked, copied */

/* ------------------------------------------------------------ dest.c */
struct dst_s {
    tabs_t g;
    const char *title;
    long long need;                   /* the save's size (0 = the card is only being picked, nothing goes into it) */
    long long freeBytes[MAX_CARDS];   /* each card's free space, once read (-2 = not yet, -1 = unknown) */
    long long memFree;                /* the Memory Card's, the same way */
    int memPort;                      /* (of the slot it was read for) */
    icon_t *icon;                     /* the selected game card's icon, once read */
    int iconCard;                     /* which card that icon is of (-1 = none) */
    char newName[56];                 /* on "New card": the card it would make ("" = not known before asking) */
};
extern struct dst_s dst;
extern card_t destNew;
void scene_dest(float t);
void dest_info(void);
int save_game_id(const char *folder, char id[12]);
card_t *choose_dest(const card_t *from, int title, long long need, const char *save, int again);
int new_card(int tab, const char *gameFolder, int max, char folder[48], char base[56]);
const char *dest_game_folder(void);
void dest_new_name(void);
int dest_new_place(const char *game);    /* "New card" of that game picked: where it will be (destNew). 1 = it can be */
int ask_more_channels(int max);
extern int cardsUnsorted;
int card_size_pick(int title, long long room, int current, int device);   /* a card's size, in MB (0 = back): dest.c */
int card_size_pick_in(int title, long long room, int current, int device, int minMb, int maxMb);   /* (within those) */
int dest_new_size(long long room);       /* the new card's size (destNew.size), picked: 1 = it was, 0 = back */
const char *dest_new_note(void);         /* what the question about making it says of it */
card_t *dest_real(card_t *to);
/* a card dest_real makes within a work already on the screen (a split by game): its progress goes there, without a
 * screen of its own (NULL = its own) */
extern void (*destProgress)(long long done, long long total);
card_t *dest_card(const char *folder, int channel);
card_t *dest_card_sized(const char *folder, int channel, long long size);

/* ------------------------------------------------------------ dialog.c */
extern dialog_t dlg, next;
void dlg_new(u32 titleColor, const char *title);
void dlg_line(int font, u32 color, int gap, const char *text);
void dlg_bar(int permille, const char *note);
void dlg_buttons(int b1, int t1, int b2, int t2);
void dlg_button(int button, int text);
void dlg_show(void);
void scene_frame(float t);
void message(u32 titleColor, const char *title, u32 textColor, const char *text);
void message_wait(u32 titleColor, const char *title, u32 textColor, const char *text);
int confirm(const char *title, const char *text, int yesText);
int choose(const char *title, const char *const *items, int n, int start);
int choose_noted(const char *title, const char *const *items, int n, int start, const char *(*note)(int i));

/* ------------------------------------------------------------ format.c */
void format_size(long long bytes, char *out, size_t size);
void format_save_when(unsigned long long w, char *out, size_t size);   /* when a save was saved, for the screen */
const char *format_name(int ps2);
void backup_when(const card_t *c, const drive_file_t *f, char *out, size_t size);
void last_backup(const card_t *c, char *out, size_t size);
u32 status_color(const card_t *c);
const char *status_text(const card_t *c);
void fit(int font, char *s, size_t size, int maxw);

/* ------------------------------------------------------------ history.c */
extern int restoreFile;
extern int restoreNew;
extern u64 lastRestoreDraw;
extern int lastRestorePhase;
int restore_progress(int phase, long long done, long long total);
void history_screen(card_t *c, icon_t *icon);

/* ------------------------------------------------------------ install.c */
extern char fileGameFolder[48];
int install_card(void);
void card_file_screen(const char *file, int install);   /* install: 0 = its saves shown, 1 = installed, 2 = pasted into */
int card_file_held(void);       /* a card file's saves are marked, back in its folder (it is kept open for them) */
int card_file_writable(void);   /* saves can be pasted into the card file that is open (fileCard) */
int card_file_marks(void);      /* saves of card files are marked (the one open, and the ones kept by their paths) */
int card_file_card(const card_t *c);   /* that card is a card file's (fileCard, or one kept for its marked saves) */
int card_file_all_marked(const char *path);   /* are all the marked saves of that card file? */
void card_file_let_go(void);    /* those marks let go of, and the card file closed */

/* ------------------------------------------------------------ keyboard.c */
int keyboard(const char *title, char *text, size_t size);

/* ------------------------------------------------------------ leave.c */
int find_opl(char *out, size_t size);
void resolve_target(const char *target, char *out, size_t size);
void run_target(const char *resolved) __attribute__((noreturn));
void leave(const char *target) __attribute__((noreturn));
extern app_t apps[MAX_APPS];
extern int nApps, appsListed;
void list_apps(void);
void target_for_ini(const char *path, char *out, size_t size);
void target_on_sd(const char *target, char *out, size_t size);
int auto_sync_on(void);
void note_igr_auto(const char *found);
const char *given_name(const char *target);
void target_name(const char *target, char *out, size_t size);

/* ------------------------------------------------------------ main.c */
extern int W, H, helperState;
extern int helperStale;
extern int networkUp, updateAvailable;
extern char rootId[80];
extern result_t results[MAX_RESULTS];
extern int nResults, sent, failed;
extern int cancelLatched;
extern int backupCancelled;
extern int restoring;
extern const card_t *singleCard;
extern icon_t *sd2psxIcon;
extern icon_t *cubeIcon;
extern int activeCard;
extern u64 watchNext;

/* ------------------------------------------------------------ marks.c */
#define MARKS_MAX TPL_SAVES
struct marks_s {
    int on;                /* the screens of saves are marking */
    int copy;              /* what for: 1 = a save's "Copy" (START is "Paste"); 0 = a template (START is "Done") */
    int n, done;           /* how many are marked; START ended it, on a screen of saves */
    card_t *into;          /* the card START was pressed on (the one whose saves were open, or the one under the
                              cursor in the list of cards; NULL = none): a copy pastes the marked saves into it */
    struct {
        card_t *card;
        mcfs_save_t s;     /* the save, as its card lists it */
    } save[MARKS_MAX];
};
extern struct marks_s marks;
int marked(const card_t *c, const char *folder);
int mark_toggle(const save_view_t *v);
int mark_same_folder(const save_view_t *v);   /* one of its folder marked on another card: asked (0 = not marked) */
void marks_reset(void);
int mark_saves(void);
save_view_t *mark_view(int i);
void mark_show(const card_t *to, int i, int text);
void mark_show_done(void);
void marks_report(int copied, int failed);
int copy_start(const save_view_t *v);
int copy_marked(card_t *into);
int copy_leave(const card_t *c);
int marks_of(const card_t *c);           /* how many of the marked saves are of that card */
void marks_move(const card_t *from, card_t *to);   /* the marked saves of one card, taken as another's */
void copy_go_on(const card_t *at);       /* the marking of a copy goes on in the list of cards, until it is over */
int paste_here(const card_t *c);         /* can the marked saves be pasted into that card? */

/* ------------------------------------------------------------ menu.c */
void menu_refresh(void);
void manual(void);

/* ------------------------------------------------------------ save.c */
extern const int psuOptions[];
void op_result(int r, int okText, const card_t *other);
void card_move_saves(card_t *c);
void save_name(const save_view_t *v, char *out, size_t size);
void save_page(save_view_t *v, const char *where, long long bytes, const int *options, int n);
int save_page_choice(void);
int save_screen(card_t *c, int i);

/* ------------------------------------------------------------ saves.c */
struct brw_s {
    card_t *card;          /* whose saves these are (&allGames: every game card's) */
    save_view_t *saves;
    int n, cursor, top;    /* top = the first row on screen */
    long long freeBytes;
    u64 since;             /* when the cursor last moved (the selected icon starts turning from the front) */
    /* for saves that aren't a card's own (a template's): what is said under the name instead of the free space
     * ("" = that), in the middle when there are none (a text; 0 = the card is empty) and at the bottom (nLegend 0 =
     * the usual buttons) */
    char note[64];
    int emptyText;
    legend_t legend[3];
    int nLegend;
};
extern struct brw_s brw;
extern int (*brwIcon)(const save_view_t *v, buffer_t *iconsys, buffer_t *ico);
void browser_scene(void);
void browser_fill(card_t *c, const mcfs_save_t *list, int n, int cursor);
u32 browser_wait(u32 buttons);
extern card_t fileCard;
extern card_file_t cardFile;
extern card_t allGames;
extern card_t *transferDest;
extern char cardFileName[256];
void save_title(save_view_t *v, const icon_t *ic);
void browser_icons(int spin);
void browser_close(void);
icon_t *newest_icon(const card_t *c);
int card_icon_read(const card_t *c, char folder[33], buffer_t *iconsys, buffer_t *ico);   /* (newest_icon's) */
void card_screen(card_t *c);

/* ------------------------------------------------------------ settings.c */
void exit_menu(void);
void settings_screen(void);
void offer_auto_sync(void);
void pick_device(int startup);

/* ------------------------------------------------------------ tabs.c */
int card_number(const card_t *c);
void draw_card_picture(const card_t *c, icon_t *ic, float t);
extern int rowsWide;
void list_rows(int n, int cursor, int top, const char *(*text)(int i, char *buf), u32 (*dot)(int i));
void scroll_to(int cursor, int *top);
extern const int deviceText[FDEVS];
/* the devices the Files group lists (FDEV_*), in order; how many */
int fdev_list(int out[FDEVS]);
int mx4sio_listed(void);   /* an MX4SIO is offered: no PS2 memory card in slot 2 (its driver would take the slot) */
void fdev_refresh(void);   /* slot 2 asked again for mx4sio_listed (on the main thread only: it asks the memory card) */
void panel_title(const char *text);   /* the right side's title: centered over the card, cut to PANEL_TITLE_W */
/* the OPL's VMC folders on other devices (vmcplaces.c): found when the cards are looked for (cards.c's vmcMore) */
void vmc_places_init(void);
const char *vmc_place_name(const char *folder);   /* a group of VMCs' place, as the screens name it: "HD interno" */
const vmc_place_t *vmc_place_pick(void);          /* where a new VMC goes: asked when there is more than one place */
int vmc_places_count(void);                       /* how many folders are read: the microSD's and the ones added */
void vmc_places_screen(void);                     /* Settings > "OPL VMC folders" */
/* the Library on the screens (libview.c): found in the data folder (library_start, when the program starts and when the
 * microSD is read again); libRoot is where a save is copied or moved to it; its k-th game's name, opened as a card's
 * saves, its newest save's icon, drawn as a game card; the Library drawn as the place a list offers (words: with
 * what it is under its card) */
extern card_t libRoot;
void library_start(void);
card_t *library_root(void);
const char *library_row_name(int k);
void library_open(int k);
icon_t *library_icon(int k);
void library_picture(int k, icon_t *ic, float t);
void library_place(int words);
/* the saves the Library is offered for, where they would go: the one of that folder (dest.c), or the marked ones
 * (marks.c); the row's text for them */
void library_dest(const char *folder);
void library_dest_marked(void);
const char *library_dest_name(void);
int tabs_memcard_port(const tabs_t *g);
int memcard_rows(tabs_t *g);   /* (tabs.c) the Memory Cards a list offers: how many, their slots in g->mcPorts */   /* the slot of the Memory Card the cursor is on (-1 = not on one) */
int storage_text(void);   /* with no sd2psx: the name of the drive the program works from (T_DEV_USB...) */
int tab_of(const card_t *c);
const char *game_of(const card_t *c);
int tabs_on_folders(const tabs_t *g);
void tabs_open(tabs_t *g);
void tabs_redo(tabs_t *g);
void tabs_show(tabs_t *g, int tab);
void tabs_init(tabs_t *g, const card_t *skip, int files);
card_t *tabs_card(const tabs_t *g);
void tabs_find(tabs_t *g, int card);
int tabs_folder_nav(tabs_t *g, u32 b);
int tabs_nav(tabs_t *g, u32 b);
void tabs_draw(const tabs_t *g, int dots);
int tabs_device(const tabs_t *g);        /* the Files group's device under the cursor (-1 = the Memory Card) */

/* ------------------------------------------------------------ tools.c */
int templates_startup(void);              /* the warning of game cards that lack their templates: how many (0 = none) */
int templates_wanted(void);              /* templates to put into the game cards after a game? */
void templates_after_game(int alone);
void templates_new_card(card_t *c, int show);   /* a game card made for a save gets its templates too (show = on a
                                                   screen of their own; 0 = within the work on the screen) */
void tools_screen(void);

/* ------------------------------------------------------------ work.c */
void checking_progress(int i, int n, const card_t *c);
void cards_retry(void);
int cards_unread(void);                  /* a card not read yet, to read again while the list of cards waits? */
void card_reread(void);                  /* reads the next one of those again */
extern int circleDown;
void watch_cancel(void);
extern int watchOff;
int confirm_cancel(int title, int text);
void helper_follow(void);
extern int skipOffered, skipLatched;
int confirm_skip(const card_t *c);
extern const card_t *current;
extern int currentN, totalN;
extern u64 iconStart;
extern icon_t *saveIcon;
extern char saveTitle[100];
extern const char *workText;
extern int workFixed;
void load_card_icon(const card_t *c);
void scene_upload(float t);
void upload_screen(long long done, long long total);
void card_work(const card_t *c, int own, const char *text, int fixed);
void card_work_done(void);
#ifdef DEBUG_BUILD
void debug_preview(int k);
void debug_preview_shown(void);
extern int debugDry;
extern int scriptLive;            /* (platform: the script holds the buttons, while the dry sync only looks at them) */
#endif
extern icon_t *cardIcon;                 /* the turning icon of the card on its way */
extern volatile long long shownDone, shownTotal;   /* how far it went */

/* ------------------------------------------------------------ synclist.c */
/* the cards of the sync under way, in its order, and what became of each */
enum { SY_WAITING, SY_SENDING, SY_SENT, SY_FAILED, SY_SKIPPED, SY_RESOLVED, SY_OFF, SY_UNCHANGED };
typedef struct {
    card_t *c;
    int state;   /* SY_*: SKIPPED, RESOLVED and OFF asked from the list (or square) */
    int taken;   /* its turn came: what was asked of it is done */
} sync_card_t;
extern sync_card_t syncCards[MAX_CARDS];
extern int nSync, syncListed, syncView;
void sync_list_start(int mode);
void sync_list_end(void);
void sync_set(int k, int state);
int sync_list_poll(void);
void scene_sync(float t);

void save_into(const card_t *to, const save_view_t *v, int text);
int save_progress(int phase, long long done, long long total);
void save_into_fixed(void);
void save_into_done(void);
void work_text(const char *text);   /* what the screen of the card on its way says it does (NULL = sending) */
void save_sending(const card_t *on, const save_view_t *v, int n, int of);   /* save n of the card's "of", on its way */
void save_sending_done(void);
/* an item of a long work: its name, the card it goes to, its icon, N of M */
void work_item(const char *title, const char *card, icon_t *icon, int n, int of);
void work_item_done(void);

#endif
