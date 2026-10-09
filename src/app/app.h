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
#define CARD_Y     100
#define CARD_CX    (CARD_X + LOOK_CARD_W / 2)

enum { TAB_CARDS, TAB_GAMES, TAB_BOOT, TAB_FILES, TABS };
#define TAB_KEYS   (PAD_LEFT | PAD_RIGHT | PAD_L1 | PAD_R1)

/* the devices of the Files group: the sd2psx's own microSD and a USB drive */
enum { FDEV_SD, FDEV_USB, FDEVS };
typedef struct {
    int tab;                  /* the group shown */
    int count[TABS];          /* how many cards each group has (the Files group: how many devices) */
    int n, idx[MAX_CARDS];    /* the list's cards (indexes in cards[]); of a list of folders, the first card of each */
    int cursor, top;          /* in the list */
    int keep[TABS][2];        /* each group's cursor and top: switching back lands on the same card */
    const card_t *skip;       /* a card left out (the one a save is copied from) */
    int files;                /* the Files group is offered too */
    int games[MAX_CARDS], nGames;         /* the game cards, by game and then as they come in cards[] */
    int first[MAX_CARDS + 1], folders;    /* where each folder starts in games[] (first[folders] = nGames) */
    char label[MAX_CARDS][64];            /* each folder's name, cut to fit the list */
    int open, openTop;        /* the folder that is open (-1 = none: the list is of folders) and the list's top then */
    float moved;              /* when a folder was last opened or closed (ui_clock): the list slides for a moment */
    int way;                  /* which way: 1 = into a folder, -1 = out of it */
    int add;                  /* 1 = every list of cards starts with a row that isn't one, "New card" (its idx is -1):
                                 a card file is being installed, as a new card or over one of these */
    int all;                  /* 1 = the folders of the Games group start with one, "All saves": every game card's
                                 saves on one screen (the main screen, with more than one game card) */
    char newGame[48];         /* a save is on its way to a card: its game's folder ("" = no game can be told). The
                                 Games group then offers "New card" for it: among the folders while the game has none
                                 there, inside its folder once it has */
} tabs_t;
typedef struct {
    mcfs_save_t s;
    card_t *card;          /* the card it is on */
    icon_t *icon;
    int tried;             /* the icon was read (or failed) */
    char line1[72], line2[72];
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
void device_lost(void) __attribute__((noreturn));

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
    card_t *c;
    save_view_t *v;
};
extern struct fbGive_s fbGive;
extern card_t *fbCard;
struct fb_s {
    int dev;                   /* FDEV_* */
    char root[16], dir[400];   /* the device's root and the folder shown: a name can be appended to either */
    dir_entry_t *list;
    int n, cursor, top;
    int loading, error;        /* the folder is being read; it couldn't be read */
    char title[200];           /* the device and the folder, without the start of the path when it's too long */
    char selected[260];        /* the selected name, cut to fit (the others are cut as they're drawn) */
};
extern struct fb_s fb;
extern int fbExit;
extern int fbPick;
extern char fbPicked[200];
struct psu_s {
    save_view_t v;
    mcfs_psu_t info;
    char path[660];
};
extern struct psu_s psu;
void psu_close(void);
void files_screen(int dev);

/* ------------------------------------------------------------ dest.c */
struct dst_s {
    tabs_t g;
    const char *title;
    long long need;                   /* the save's size (0 = the card is only being picked, nothing goes into it) */
    long long freeBytes[MAX_CARDS];   /* each card's free space, once read (-2 = not yet, -1 = unknown) */
    icon_t *icon;                     /* the selected game card's icon, once read */
    int iconCard;                     /* which card that icon is of (-1 = none) */
    char newName[56];                 /* on "New card": the card it would make ("" = not known before asking) */
};
extern struct dst_s dst;
extern int destFiles, destDevice;
extern card_t destNew;
void scene_dest(float t);
void dest_info(void);
int save_game_id(const char *folder, char id[12]);
card_t *choose_dest(const card_t *from, int title, long long need, const char *save);
int new_card(int tab, const char *gameFolder, int max, char folder[48], char base[56]);
const char *dest_game_folder(void);
void dest_new_name(void);
int ask_more_channels(int max);
extern int cardsUnsorted;
card_t *dest_real(card_t *to);

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

/* ------------------------------------------------------------ format.c */
void format_size(long long bytes, char *out, size_t size);
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
void card_file_screen(const char *file, int install);

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

/* ------------------------------------------------------------ menu.c */
void menu_refresh(void);
void manual(void);

/* ------------------------------------------------------------ save.c */
extern const int psuOptions[];
void op_result(int r, int okText, const card_t *other);
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
#define MARKS_MAX TPL_SAVES
struct marks_s {
    int on;                /* the screens of saves are marking */
    int n, done;           /* how many are marked; START ended it */
    struct {
        card_t *card;
        char folder[33];
    } save[MARKS_MAX];
};
extern struct marks_s marks;
int marked(const card_t *c, const char *folder);
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
void card_screen(card_t *c);

/* ------------------------------------------------------------ settings.c */
void exit_menu(void);
void settings_screen(void);
void offer_auto_sync(void);

/* ------------------------------------------------------------ tabs.c */
int card_number(const card_t *c);
void draw_card_picture(const card_t *c, icon_t *ic, float t);
extern int rowsWide;
void list_rows(int n, int cursor, int top, const char *(*text)(int i, char *buf), u32 (*dot)(int i));
void scroll_to(int cursor, int *top);
extern const int deviceText[FDEVS];
int tab_of(const card_t *c);
const char *game_of(const card_t *c);
int tabs_on_folders(const tabs_t *g);
void tabs_open(tabs_t *g);
void tabs_show(tabs_t *g, int tab);
void tabs_init(tabs_t *g, const card_t *skip, int files);
card_t *tabs_card(const tabs_t *g);
void tabs_find(tabs_t *g, int card);
int tabs_folder_nav(tabs_t *g, u32 b);
int tabs_nav(tabs_t *g, u32 b);
void tabs_draw(const tabs_t *g, int dots);

/* ------------------------------------------------------------ tools.c */
void templates_startup(void);
void templates_after_game(void);
void tools_screen(void);

/* ------------------------------------------------------------ work.c */
void checking_progress(int i, int n, const card_t *c);
extern int circleDown;
void watch_cancel(void);
extern int watchOff;
int confirm_cancel(int title, int text);
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
#endif
void save_into(const card_t *to, const save_view_t *v, int text);
int save_progress(int phase, long long done, long long total);
void save_into_fixed(void);
void save_into_done(void);

#endif
