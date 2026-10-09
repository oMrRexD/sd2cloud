/*
 * SD2Cloud -- backs up the sd2psx's memory cards to Google Drive, and manages them, from the PS2 itself.
 *
 * This file is where the program starts: it finds out how it was opened (by the user, or by the IGR helper after a
 * game) and hands over to the main screen (src/app/menu.c) or to the automatic sync (src/app/autosync.c). It also
 * holds the state the screens share. The screens and what they do are in src/app; docs/ARCHITECTURE.md has the map.
 */
#include "app.h"

int W, H, helperState = HELPER_NO_FILE;
int helperStale;            /* the card in the device was changed since helperState was read (active_watch) */
int networkUp, updateAvailable;
char rootId[80];
result_t results[MAX_RESULTS];
int nResults, sent, failed;

int cancelLatched;          /* circle was pressed during an upload or download */
int backupCancelled;        /* the user confirmed cancelling the backup */
int restoring;              /* the backup running is the one before a restore (cancelling it doesn't leave) */
const card_t *singleCard;   /* the card of run_backup's mode 3 */
icon_t *sd2psxIcon;         /* the SD2PSX memory card, for the cards shared by many games */
icon_t *cubeIcon;           /* the blue cube of a save without an icon (or with one that can't be read) */
int activeCard = -1;        /* the card the sd2psx is emulating (index in cards[]); -1 = none here, or not known */
u64 watchNext;              /* when the device is next asked whether it is there (device_watch) */

/* ------------------------------------------------------------ main */

int main(int argc, char *argv[])
{
    int r;
    system_init(argc, argv);
    token_read();
#ifdef DEBUG_BUILD
    if (!igrMode && debug_has_script())
        igrMode = debug_take('I');   /* (script: an "I" at the start runs it as IGR does) */
#endif
    /* IGR with no sync to do: straight to what comes after it, before the screen and the sound are even started. The
     * helper does this by itself when it can (igr/igr.c); this is for when it can't: a helper from before it did, or
     * a program on a device only SD2Cloud has the drivers for */
    if (igrMode && !auto_sync_on()) {
        char c[260];
        log_msg("IGR: %s, nothing to sync", cfg.no_auto_sync ? "the automatic sync is off" : "no Google account");
        resolve_target(cfg.igr_return, c, sizeof(c));
        note_igr_auto(c);
        run_target(c);
    }
    i18n_select(cfg.language);
    if (!dev->sd2psx)
        i18n_device(dev->name);
    /* first run: create the sd2cloud.ini with every option explained, in the screen's language */
    if (!configExists)
        config_write_template();
    /* The settings file only gets a line of the app's own when one is needed. Where the program is, for the IGR helper:
     * the settings are always in the same place on the microSD, so the program itself can be in any folder of it (in
     * its usual folder nothing is written). Its version, once a copy on another device (a USB drive) has handed over
     * to this one: started by IGR, that copy runs by itself only if it is the same version */
#ifdef DEBUG_BUILD
    /* The debug build is opened by hand, to look into a problem: on the console it doesn't take the program's place,
     * so the IGR helper and the shortcut go on opening the real one (and the real backups go on being made). A debug
     * build from before this did take it: put back, when the program is in its usual folder. On PCSX2 the debug build
     * is the only program there is */
    if (!strncmp(appPath, "mmce", 4)) {
        static const char usual[] = "APPS/SD2Cloud/SD2CLOUD.ELF";
        char c[80];
        snprintf(c, sizeof(c), "%s%s", sdRoot, usual);
        if (!strcasecmp(cfg.app_path, appPath) && strcasecmp(appPath + 7, usual) != 0 && file_exists(c)) {
            snprintf(cfg.app_path, sizeof(cfg.app_path), "mmce?:/%s", usual);
            config_set("app", "app_path", cfg.app_path);
            log_msg("this debug build was what IGR opened: %s is again", cfg.app_path);
        }
    } else
#endif
    if (!strncmp(appPath, "mmce", 4) || !strncmp(appPath, "host:", 5)
#ifndef DEBUG_BUILD
        || appElsewhere   /* (on a USB drive, an MX4SIO or the hard disk: with that device's drivers on the microSD) */
#endif
        ) {
        if (strcasecmp(cfg.app_path, appPath) != 0) {
            snprintf(cfg.app_path, sizeof(cfg.app_path), "%s", appPath);
            config_set("app", "app_path", appPath);
        }
        if (appElsewhere)
            drivers_store(appPath);
        if ((appTookOver || cfg.app_version[0]) && strcmp(cfg.app_version, APP_VERSION) != 0) {
            snprintf(cfg.app_version, sizeof(cfg.app_version), "%s", APP_VERSION);
            config_set("app", "app_version", APP_VERSION);
        }
    }
    if ((r = ui_init()) != 0) {
        log_msg("ui_init failed (%d)", r);
        go_osd();
    }
    W = ui_width();
    H = ui_height();
    sound_init();
    sound_play(SND_STARTUP);
    ui_lock();
    sd2psxIcon = icon_make_sd2psx();
    cubeIcon = icon_make_cube();
    ui_unlock();
    state_read();
    google_set_poll(watch_cancel);
    if (igrMode)
        igr();
    if (setjmp(reloadPoint)) {   /* the device was taken out and is back (device_watch): maybe with another microSD */
        system_reload();
        token_read();
        google_forget();
        state_read();
        i18n_select(cfg.language);
        ui_lock();
        i18n_device(dev->name);   /* (it may be another device, or the user said it is) */
        ui_unlock();
        if (!configExists)
            config_write_template();
        log_msg("read again: %s, %s", dev->name, google_has_access() ? "with a Google account" : "no Google account");
    }
    idleHook = device_watch;
    manual();
    return 0;
}
