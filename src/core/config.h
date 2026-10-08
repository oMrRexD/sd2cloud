/* SD2Cloud -- the settings: sd2cloud.ini (config.c) */
#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <tamtypes.h>

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
    int beta;                    /* [update] channel = beta: updates come from the "beta" pre-release, rebuilt at every
                                    change, instead of the latest release */
} config_t;
extern config_t cfg;
extern int configExists;
void config_read(void);                     /* no file = the defaults */
void config_write_template(void);           /* writes the sd2cloud.ini with every option at its default */
int config_set(const char *section, const char *key, const char *value);   /* one setting, in place */
int config_keep(const char *id);
int list_has(const char *list, const char *id);   /* does "a, b, c" contain id? */

#endif
