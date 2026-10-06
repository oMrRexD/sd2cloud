/* SD2Cloud -- declarations shared by the app's source files */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <tamtypes.h>

#define APP_NAME    "SD2Cloud"
#define APP_VERSION "1.3.2"

/* ------------------------------------------------------------ system.c */
extern char appDir[200];    /* where the program lives: mmce0:/APPS/SD2Cloud/ (host:APPS/SD2Cloud/ on PCSX2) */
extern char appPath[260];   /* the program itself, as it was started: mmce?:/APPS/SD2Cloud/SD2CLOUD.ELF (mmce?: =
                               whichever slot the sd2psx is in). Kept in the settings for the IGR helper */
/* the program was started from another device (USB, MX4SIO...) and there is no SD2Cloud on the microSD to take
 * over: appDir is then where it would be on the microSD (APPS/SD2Cloud), which is where an update installs it */
extern int appElsewhere;
extern int appTookOver;     /* started by a copy of the program on another device, which handed over to this one */
extern char sdRoot[16];     /* root of the sd2psx microSD: mmce0:/ or mmce1:/ (host: on PCSX2) */
extern char dataDir[32];    /* the user's data: <sdRoot>SD2Cloud/ (sd2cloud.ini, state.ini, token.dat) */
extern int igrMode;         /* started by the IGR helper (-igr) */

void system_init(int argc, char *argv[]);
u64 now_ms(void);
void sleep_ms(int ms);
u32 pad_buttons(void);                      /* buttons held right now (PAD_*) */
u32 wait_button(u32 mask, int seconds);     /* 0 = timed out; seconds = 0 waits forever */
u32 wait_nav(u32 mask);                     /* for lists: like wait_button, but the arrows repeat while held */
u32 wait_nav_ms(u32 mask, int ms);          /* the same, giving up after ms (0 = no press) */
typedef struct {
    int year, month, day, hour, minute, second;
} datetime_t;
void local_time(datetime_t *t);
/* a memory card's time (the PS2 writes it in the console clock's Japan time) in the console's time zone */
void card_time_local(int year, int month, int day, int hour, int minute, int second, datetime_t *t);
void log_raw(const char *d, size_t n);
void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int network_up(void);                       /* 0 = ok, else the id of the message to show */
void run_elf(const char *path) __attribute__((noreturn));   /* falls back to the OSD if it can't */
/* where a program to open is: the sd2psx (or PCSX2's host:), a memory card, USB, MX4SIO, the HDD exFAT or APA */
enum { DEV_SD, DEV_MC, DEV_USB, DEV_MX4SIO, DEV_ATA, DEV_HDD };
int device_of(const char *path);
/* the card the sd2psx is emulating right now: its number (0 = BootCard, a game card or a named folder) and channel.
 * -1 = couldn't ask; -2 = not on an MMCE device (testing on PCSX2) */
int mmce_active_card(int *channel);
/* Tell the sd2psx to emulate another card, as its own buttons do: it writes what it holds of the current one to the
 * microSD, closes it and opens the other, a moment later. Each one only asks (0 = the sd2psx took the request):
 * whether the card changed has to be seen in the slot afterwards. A numbered card (its first channel), or with boot
 * the BootCard, which the sd2psx only goes to with Autoboot on, in the channel it keeps for it; another channel of
 * the card it is on; the card of a game, by its ID, which it only goes to with Game ID on */
int mmce_set_card(int boot, int number);
int mmce_set_channel(int channel);
int mmce_set_gameid(const char *id);
/* a USB drive (mass0:), for the file browser: loads its drivers the first time, then waits up to ms for the drive.
 * 0 = it's there */
int usb_open(int ms);
void go_osd(void) __attribute__((noreturn));
#ifdef DEBUG_BUILD
int debug_has_script(void);
int debug_take(char mark);                   /* the next letter of the script is this one: consume it */
int debug_digit(void);                       /* the next letter is a digit: consume it (-1 = it isn't) */
void debug_capture_if(char mark);            /* the next letter of the script is this one: capture and stop */
void debug_capture_and_stop(void) __attribute__((noreturn));
extern int debugNoLinkOnce, debugNoDhcpOnce, debugNoZero;
void debug_log_memory(const char *when);
#endif

/* ------------------------------------------------------------ buffer and files (files.c) */
typedef struct {
    unsigned char *data;
    size_t len, cap;
} buffer_t;
int buf_append(buffer_t *b, const void *d, size_t n);   /* 0 = ok; keeps a 0 after the end */
void buf_free(buffer_t *b);
int file_read(const char *path, buffer_t *b);
int file_write(const char *path, const unsigned char *d, size_t n);
int file_replace(const char *path, const unsigned char *d, size_t n);   /* .new -> verify -> rename */
int file_write_checked(const char *path, const unsigned char *d, size_t n);   /* written and read back: 1 = it's there, as it should be */
int file_exists(const char *path);
/* what a folder has (a path ending in /): the folders first, then the files, each by name. Returns how many (the
 * ones past max are left out), or -1 when it can't be read */
typedef struct {
    char name[256];
    int dir;
    long long size;
} dir_entry_t;
int dir_list(const char *path, dir_entry_t *list, int max);
void ensure_data_dir(void);                 /* creates <sd>SD2Cloud/ if needed */
void ensure_dir(const char *dir);           /* creates a folder (a path ending in /) and the ones above it, if needed */
/* reads an .ini: cb(section, key, value) for each "key = value" (section "" before the first one) */
int ini_read(const char *path, void (*cb)(const char *section, const char *key, const char *value, void *u), void *u);
void trim(char *s);
/* a text typed on a PC that isn't valid UTF-8 (a file saved as ANSI by an older editor) is taken as Latin-1 and
 * converted: its accents show on screen, and Google refuses a request that isn't UTF-8 */
void utf8_fix(char *s, size_t size);
void sha256_hex(const unsigned char *d, size_t n, char *out);

/* ------------------------------------------------------------ json.c */
typedef struct {
    const char *p, *end;
} json_t;
int js_string(json_t *j, char *out, size_t size);
int js_skip(json_t *j, int depth);
int js_find(const char *txt, size_t n, const char *key, json_t *v);
int js_get_string(const char *txt, size_t n, const char *key, char *out, size_t size);
int js_get_number(const char *txt, size_t n, const char *key, long long *out);
int js_get_object(const char *txt, size_t n, const char *key, const char **start, size_t *len);   /* nested object */
int js_each(const char *txt, size_t n, const char *key, void (*cb)(const char *obj, size_t n, void *u), void *u);

/* ------------------------------------------------------------ i18n.c */
#include "messages.h"
void i18n_select(const char *language);    /* "auto", "en" or "pt" */
const char *T(int id);
int i18n_is_pt(void);                       /* is the screen in Portuguese? */

/* ------------------------------------------------------------ config.c (sd2cloud.ini) */
#define TYPE_NORMAL 1
#define TYPE_GAMEID 2
#define TYPE_BOOT   4
#define TYPE_NAMED  8
#define MAX_RULES   64
typedef struct {
    char language[8];
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
    int no_auto_sync;            /* [igr] auto_sync = no: IGR goes straight to igr_return, without syncing */
    char igr_auto[200];          /* [app] igr_auto: what "auto" in igr_return led to, for the IGR helper */
    int igr_settle, igr_summary; /* seconds */
    char app_path[260];          /* [app] app_path: where the program is, for the IGR helper (written by the app) */
    char app_version[16];        /* [app] app_version: the version of that one (written by the app) */
    char manual_return[200];
    char manual_name[64];        /* [manual] name: the same, for the program in manual_return */
    char repo[80];               /* [update] repo (owner/name on GitHub) */
} config_t;
extern config_t cfg;
extern int configExists;
void config_read(void);                     /* no file = the defaults */
void config_write_template(void);           /* writes the sd2cloud.ini with every option at its default */
int config_set(const char *section, const char *key, const char *value);   /* one setting, in place */
int config_keep(const char *id);
int list_has(const char *list, const char *id);   /* does "a, b, c" contain id? */

/* ------------------------------------------------------------ state.c (state.ini, token.dat) */
#define MAX_CARDS 512
/* version of the fingerprint (mcfs.c). 2 = without the "history" of the B?DATA-SYSTEM folders; 3 = without those
 * folders at all */
#define MCFS_VERSION 3
typedef struct {
    char id[96];
    char fingerprint[65];
    int version;           /* MCFS_VERSION the fingerprint was computed with (0 = the first one, before 2026-10-02) */
    char sha[65];
    char when[20];
} card_state_t;
card_state_t *state_card(const char *id, int create);
const char *state_folder(const char *name);              /* Drive folder id (cache) */
void state_set_folder(const char *name, const char *id);
void state_read(void);
int state_write(void);
int state_empty(void);                                   /* nothing was ever backed up */
extern char refreshToken[512];
void token_read(void);
int token_write(void);

/* ------------------------------------------------------------ cards.c */
typedef struct {
    char folder[48];       /* Card1, SLUS-21065, BOOT, MyCard */
    char base[56];         /* Card1-1, SLUS-21065-1, BootCard-1 */
    char id[96];           /* folder/base: the key in the config and in the state */
    char path[200];        /* the .mcd */
    char name[48];         /* channel name from the CardX.ini, if any */
    char game[64];         /* a game card's game, as the sd2psx names it ("" = not a game card, or not in the list) */
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
/* fingerprint of each included card's index; progress(i, n) before each card */
void cards_check(void (*progress)(int i, int n, const card_t *c));
void cards_recheck(card_t *c);              /* one card again (after a save was copied in or out) */

/* ------------------------------------------------------------ mcfs.c */
/* fingerprint of the card's file system index (root and folder entries). 0 = ok */
int mcfs_fingerprint(const char *path, char hex[65], int *saves);
/* signature of the root folder only (name + modification time of each entry, in any order): the same one
 * helper.c computes from the memory card the PS2 sees, to tell whether a .mcd is the card the sd2psx is emulating */
#define ROOT_REC 40
int mcfs_root_signature(const char *path, char hex[65]);
void mcfs_sign_records(unsigned char (*rec)[ROOT_REC], int n, char hex[65]);
/* the newest save of the card that has an icon (by modification time, not the B?DATA-SYSTEM folders): its folder and
 * the contents of its icon.sys and of the 3D icon that icon.sys names. 0 = ok */
int mcfs_newest_save_icon(const char *path, char folder[33], buffer_t *iconsys, buffer_t *ico);
/* the saves of a card (its folders, not the B?DATA-SYSTEM ones), the newest first as in the PS2 browser, and its
 * free space (-1 = unknown). Returns how many, or -1 */
#define MCFS_MAX_SAVES 256
typedef struct {
    char folder[33];
    unsigned long long when;       /* modification time, comparable */
    unsigned int cluster, count;   /* where the folder is and how many entries it has */
} mcfs_save_t;
int mcfs_list_saves(const char *path, mcfs_save_t *list, int max, long long *freeBytes);
/* the icon.sys and the 3D icon of a save from that list. 0 = ok */
int mcfs_save_icon(const char *path, const mcfs_save_t *save, buffer_t *iconsys, buffer_t *ico);
/* changing a card (a .mcd that the sd2psx is NOT using right now): copy a save to another card (read back and
 * compared), delete a save, export a save as a .psu, import one from a .psu file (also read back and compared).
 * 0 = ok, else MCFS_ERR_* (BAD = the file isn't a .psu this can use) */
enum { MCFS_OK = 0, MCFS_ERR_IO = -1, MCFS_ERR_EXISTS = -2, MCFS_ERR_FULL = -3, MCFS_ERR_NOT_FOUND = -4, MCFS_ERR_CHECK = -5,
       MCFS_ERR_BAD = -6 };
int mcfs_save_info(const char *path, const char *folder, long long *bytes, int *files);
int mcfs_copy_save(const char *from, const char *folder, const char *to);
int mcfs_delete_save(const char *path, const char *folder);
int mcfs_export_psu(const char *path, const char *folder, buffer_t *out);
/* what a .psu file holds: the save's folder, when it was last saved, its files' sizes added up, and its icon (the
 * buffers stay empty when it has none) */
typedef struct {
    char folder[33];
    unsigned long long when;
    long long bytes;
    int files;
} mcfs_psu_t;
int mcfs_psu_info(const char *psu, mcfs_psu_t *info, buffer_t *iconsys, buffer_t *ico);
int mcfs_import_psu(const char *psu, const char *to);

/* ------------------------------------------------------------ stream.c */
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

/* ------------------------------------------------------------ google.c */
int google_init(void);
int google_has_access(void);                /* there is a refresh token */
void google_logout(int online);             /* revokes the access (online) and forgets it */
int google_refresh(void);                   /* 0 ok, -2 = must sign in again, -1 error */
/* sign-in with a code: calls show(url, code) and waits; cancel() != 0 gives up. 0 = ok, -1 = given up, else the id
 * of the message that says why it failed */
int google_login(void (*show)(const char *url, const char *code, int seconds_left), int (*cancel)(void));
int google_folder(const char *name, const char *parent, char *id, size_t idsize);   /* finds or creates */
typedef struct {
    char id[80];
    char name[128];
    char created[32];
    long long size;        /* of the .zip */
    char sha_mcd[65];      /* SHA-256 of the .mcd inside (appProperties; may be empty) */
} drive_file_t;
int google_list(const char *folder, const char *card, drive_file_t *list, int max);   /* oldest first */
int google_delete(const char *id);
/* uploads a card, streamed, to the folder; progress(read_mcd, total_mcd). id = the file created */
int google_upload(const card_t *c, const char *folder, const char *name, const datetime_t *t, stream_t *s,
                  int (*progress)(long long done, long long total), char *id, size_t idsize);
/* a small file from memory (a single save). 0 = ok */
int google_upload_buffer(const char *folder, const char *name, const char *description, const unsigned char *d, size_t n,
                         int (*progress)(long long done, long long total), char *id, size_t idsize);
/* downloads a Drive file, handing each piece to sink (!= 0 stops). 0 = ok, -2 = stopped by sink, -1 = error */
int google_download(const char *id, int (*sink)(const unsigned char *d, size_t n, void *u), void *u);
/* called while curl transfers (to watch the controller during an upload or download) */
void google_set_poll(void (*poll)(void));
extern char googleError[256];               /* why the last request failed, for the screen */
/* GET from GitHub (the API or a release asset; follow = follow redirects). returns the HTTP status, 0 = network */
long github_get(const char *url, buffer_t *b, int follow);

/* ------------------------------------------------------------ update.c (GitHub Releases) */
int update_check(void);                     /* 1 = there is a newer version (with SD2CLOUD.ELF and GitHub's SHA-256), 0 = not, -1 = no answer */
const char *update_tag(void);
int update_install(void);                   /* 0 = installed and verified (the app then reopens the new version) */

/* ------------------------------------------------------------ restore.c */
enum { RESTORE_DOWNLOAD, RESTORE_CHECK, RESTORE_WRITE, RESTORE_VERIFY };
/* replaces the card on the microSD with a backup from Drive, checked before and after writing.
 * progress(phase, done, total) != 0 cancels (only before RESTORE_WRITE).
 * 0 = restored; -2 = cancelled (the card didn't change); -1 = error (googleError says why) */
int restore_card(card_t *c, const drive_file_t *f, int (*progress)(int phase, long long done, long long total));

/* ------------------------------------------------------------ helper.c */
enum { HELPER_NO_FILE, HELPER_NOT_INSTALLED, HELPER_DIFFERENT, HELPER_SAME };
int helper_status(void);                    /* the helper in mc0:/BOOT compared with the one in the app's folder */
int mc_root_signature(int port, char hex[65]);   /* mcfs_root_signature of the card in that slot, through mcman */
int mc_card_state(int port);                     /* 0 = the card in that slot is the one it was when last asked */
#define HELPER_TARGET "/BOOT/SD2CLOUD-IGR.ELF"
int helper_present(void);                   /* the helper's ELF is in the app's folder, and it can be installed */
int helper_install(void);                   /* copies it to mc0: 0 = ok, -2 = the ELF is missing, -1 = couldn't write,
                                               -3 = not enough room (helperNeedKb, helperFreeKb) */
/* what installing takes on the memory card in use and what the card has free, for the question before installing:
 * fills helperNeedKb and helperFreeKb (-1 = unknown). 0 = known (-1 = the helper's ELF is missing) */
int helper_space(void);
int helper_uninstall(void);                 /* removes it from mc0: 0 = removed (or it wasn't there) */
extern int helperNeedKb, helperFreeKb;

#endif
