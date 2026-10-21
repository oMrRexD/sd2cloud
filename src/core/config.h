/* SD2Cloud -- the settings: sd2cloud.ini (config.c) */
#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <tamtypes.h>

#define TYPE_NORMAL 1
#define TYPE_GAMEID 2
#define TYPE_BOOT   4
#define TYPE_NAMED  8
#define TYPE_VMC    16   /* an OPL VMC of the microSD (cards.c) */
#define TYPE_LIBRARY 32  /* the Library's saves (library.c): never one of the cards, nor synced as one */
#define MAX_RULES   64
#define SORT_NEWEST 0
#define SORT_OLDEST 1
#define SORT_NAME   2
/* [vmc]: the OPL's VMC folders on other devices than the microSD (cards.c), one for each: its path from the device's
 * root ("/VMC"); on the hard disk's APA side, its partition first ("+OPL/VMC"); other, any folder as the PS2 names
 * it ("mass0:/VMC") */
enum { VMC_HDD, VMC_APA, VMC_MX4SIO, VMC_USB, VMC_OTHER, VMC_KEYS };
extern const char *const vmcKeys[VMC_KEYS];
typedef struct {
    char language[8];
    char device[12];             /* [general] device = sd2psx or pro2: which device the microSD is in, when it has the
                                    cards of both and the device doesn't tell ("" = not asked yet) */
    char drive_folder[64];
    int no_ask_connect;          /* [general] ask_connect = no: don't offer to connect a Google account at startup */
    int list_mode;               /* 0 = auto, 1 = only the cards in include */
    int types;                   /* TYPE_* */
    char include[1024], exclude[1024];
    int keep;                    /* backups per card (0 = all) */
    int ps2;                     /* the backups go as .ps2 (the card with its ECC bytes, as PCSX2 reads it), not .mcd */
    int n_rules;
    struct {
        char id[96];
        int keep;
    } rules[MAX_RULES];
    char igr_return[200];
    char igr_name[64];           /* [igr] name: what to call the program in igr_return on screen ("" = by its path) */
    int save_backup;             /* [cards] saves = yes: each save that changed is sent by itself too, with its versions
                                    (and the Memory Card's, by the automatic sync). Not said: yes with no device in the
                                    slots (the Memory Card's saves are what there is to sync), no with one */
    int no_auto_sync;            /* [igr] auto_sync = no: IGR goes straight to igr_return, without syncing */
    char igr_auto[200];          /* [app] igr_auto: what "auto" in igr_return led to, for the IGR helper */
    int igr_settle, igr_summary; /* seconds */
    char app_path[260];          /* [app] app_path: where the program is, for the IGR helper (written by the app) */
    char app_drivers[48];        /* [app] app_drivers: the device and the build whose drivers SD2Cloud/drivers/ holds */
    char app_build[40];          /* [app] app_build: the version and the commit of the program that last ran here */
    char app_version[16];        /* [app] app_version: the version of that one (written by the app) */
    char manual_return[200];
    char manual_name[64];        /* [manual] name: the same, for the program in manual_return */
    int no_tpl_after_game;       /* [templates] after_game = no: the automatic sync leaves the game cards as they are,
                                    without putting their templates into them */
    char repo[80];               /* [update] repo (owner/name on GitHub) */
    int sort;                    /* [saves] sort: how a card's saves are listed (SORT_*): date, the newest first; date_asc,
                                    the oldest first; or name */
    int show_repeated;           /* [saves] repeated = show: on the screen of every game card's saves, the same save
                                    on more than one card is there each time, not once */
    int beta;                    /* [update] channel = beta: updates come from the "beta" pre-release, rebuilt at every
                                    change, instead of the latest release */
    char vmc[VMC_KEYS][120];     /* [vmc] hdd, apa, mx4sio, usb, other ("" = none) */
    int library_drive;           /* [library] drive = yes: a sync sends the Library's new saves to Drive too, each by
                                    itself (the backup by save's way) */
    int drive_layout;            /* [drive] layout: how the backups are kept on Drive (drivemap.h): 1 = the first layout,
                                    2 = the second; 0 = not said (app/drive.c tells which, and writes it) */
    int drive_asked;             /* [drive] asked = yes: the move to the second layout was offered once already */
    int drive_moving;            /* [drive] moving = yes: that move was agreed to, and isn't over (it goes on at startup) */
} config_t;
#define DRIVE_FOLDER_OLD "PS2 Memory Card Backups"   /* the main folder's name in the first layout, by default */
#define DRIVE_FOLDER_NEW "SD2Cloud"                  /* in the second */
extern config_t cfg;
extern int configExists;
void config_read(void);                     /* no file = the defaults */
void config_write_template(void);           /* writes the sd2cloud.ini with every option at its default */
int config_set(const char *section, const char *key, const char *value);   /* one setting, in place */
int config_keep(const char *id);
int list_has(const char *list, const char *id);   /* does "a, b, c" contain id? */
/* such a list with one more card (0 = it is there, -1 = no room for it), and without the rules that are that very
 * card: its whole id, or its file's name (a rule for its whole folder stays; given a folder, that rule goes) */
int list_add(char *list, size_t size, const char *id);
void list_remove(char *list, size_t size, const char *id);

#endif
