/* SD2Cloud -- the settings (START): the automatic sync and what it keeps on the memory card, where to go after
 * IGR, the language, how many backups are kept and in which format, the updates, and About. */
#include "app.h"

static void helper_install_now(int doneTitle);

/* -------- settings (START): syncing every card, the automatic sync on or off, the IGR helper, where to go after IGR,
 * the language, how many backups to keep, the update check, the Google account, about. Each change goes to
 * sd2cloud.ini at once */


enum { SET_SYNC_ALL, SET_AUTO_SYNC, SET_HELPER, SET_IGR_RETURN, SET_DEVICE, SET_LANGUAGE, SET_KEEP, SET_FORMAT, SET_UPDATES,
       SET_CHANNEL, SET_ACCOUNT, SET_ABOUT, SET_MAX };
#define SET_X      80     /* the labels; the values end at SET_RIGHT */
#define SET_RIGHT  560
#define SET_Y0     120
#define SET_ROW    22
static struct {
    int n, cursor, item[SET_MAX];
    char label[SET_MAX][96], value[SET_MAX][64];
} st;

static void set_item(int *i, int id, const char *label, const char *value)
{
    st.item[*i] = id;
    snprintf(st.label[*i], sizeof(st.label[0]), "%s", label);
    snprintf(st.value[*i], sizeof(st.value[0]), "%s", value);
    fit(FONT_TEXT, st.value[*i], sizeof(st.value[0]), 200);
    (*i)++;
}

static void build_settings(void)
{
    char v[96];
    int included, changed, i = 0;
    long long bytes;
    count_cards(&included, &changed, &bytes);
    ui_lock();
    if (changed > 1)
        snprintf(v, sizeof(v), T(T_PENDING_N), changed);
    set_item(&i, SET_SYNC_ALL, T(T_SET_SYNC_ALL), changed > 1 ? v : T(changed ? T_PENDING_ONE : T_NONE_PENDING));
    set_item(&i, SET_AUTO_SYNC, T(T_IGR_TITLE), T(auto_sync_on() ? T_SYNC_ON : T_SYNC_OFF));
    set_item(&i, SET_HELPER, T(T_HELPER_TITLE), T(helperState == HELPER_SAME ? T_HELPER_INSTALLED
                                                : helperState == HELPER_DIFFERENT ? T_HELPER_OUTDATED
                                                : helperState == HELPER_NOT_INSTALLED ? T_HELPER_NOT_INSTALLED : T_HELPER_NA));
    target_name(cfg.igr_return, v, sizeof(v));
    set_item(&i, SET_IGR_RETURN, T(T_SET_IGR_RETURN), v);
    if (deviceBoth)   /* (a microSD with the cards of both devices: which one it is in is the user's to say) */
        set_item(&i, SET_DEVICE, T(T_SET_DEVICE), dev->name);
    set_item(&i, SET_LANGUAGE, T(T_SET_LANGUAGE), !strcmp(cfg.language, "pt") ? "Português" : !strcmp(cfg.language, "en") ? "English"
                                                                                    : T(T_AUTO));
    snprintf(v, sizeof(v), "%d", cfg.keep);
    set_item(&i, SET_KEEP, T(T_SET_KEEP), cfg.keep ? v : T(T_KEEP_ALL));
    set_item(&i, SET_FORMAT, T(T_SET_FORMAT), format_name(cfg.ps2));
    if (updateAvailable)
        snprintf(v, sizeof(v), T(T_UPDATE_AVAILABLE), update_tag());
    set_item(&i, SET_UPDATES, T(T_SET_UPDATES), updateAvailable ? v : "");
    set_item(&i, SET_CHANNEL, T(T_SET_CHANNEL), cfg.beta ? "Beta" : T(T_CHANNEL_STABLE));
    set_item(&i, SET_ACCOUNT, T(T_SET_ACCOUNT), T(google_has_access() ? T_ACCOUNT_ON : T_ACCOUNT_OFF));
    set_item(&i, SET_ABOUT, T(T_SET_ABOUT), "v" APP_VERSION);
    st.n = i;
    if (st.cursor >= i)
        st.cursor = 0;
    ui_unlock();
}

static void scene_settings(float t)
{
    int i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_title(W / 2.0f, 88, T(T_SETTINGS), 1);
    for (i = 0; i < st.n; i++) {
        float y = SET_Y0 + i * (st.n > 11 ? SET_ROW - 2 : SET_ROW);   /* (one more row than usual: closer together) */
        if (i == st.cursor) {
            look_glow_text(FONT_TEXT, SET_X, y, COLOR_ITEM, COLOR_ITEM_ON, st.label[i]);
            ui_text_right(FONT_TEXT, SET_RIGHT, y, 0xD8E4F4, st.value[i]);
        } else {
            ui_text(FONT_TEXT, SET_X, y, COLOR_ITEM, st.label[i]);
            ui_text_right(FONT_TEXT, SET_RIGHT, y, COLOR_DIM, st.value[i]);
        }
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
}

/* every card that isn't synced; when all of them are, says so and offers to sync them all again */
static void sync_all(void)
{
    char t[200];
    int included, changed, i;
    long long bytes;
    count_cards(&included, &changed, &bytes);
    if (!included) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS));
        return;
    }
    if (!changed) {
        snprintf(t, sizeof(t), T(T_SYNC_AGAIN_ALL), included);
        if (confirm(T(T_ALL_SYNCED), t, T_SYNC))
            manual_backup(2);
        return;
    }
    if (changed == 1) {
        for (i = 0; i < nCards && !is_selected(&cards[i], 1); i++)
            ;
        snprintf(t, sizeof(t), T(T_CONFIRM_BACKUP_ONE), cards[i].base);
    } else
        snprintf(t, sizeof(t), T(T_CONFIRM_BACKUP_CHANGED), changed);
    if (confirm(t, T(T_CONFIRM_BACKUP_TEXT), T_SYNC))
        manual_backup(1);
}

/* what helper_install's answer means for the user */
static const char *helper_result(int r)
{
    static char t[200];
    if (r == -3) {
        snprintf(t, sizeof(t), T(T_HELPER_FULL), helperNeedKb, helperFreeKb);
        return t;
    }
    return T(T_HELPER_ERROR);
}

/* how much of the memory card in use the helper takes, and how much the card has free: said before installing
 * ("" when the helper's file is missing) */
static const char *helper_space_text(void)
{
    static char t[160];
    t[0] = 0;
    if (helper_space() == 0) {
        if (helperFreeKb >= 0)
            snprintf(t, sizeof(t), T(T_HELPER_SPACE), helperNeedKb, helperFreeKb);
        else
            snprintf(t, sizeof(t), T(T_HELPER_SPACE_NEED), helperNeedKb);
    }
    return t;
}

/* installs the helper (the user has just agreed to it) and, once it is there, shows the path to set in OPL.
 * doneTitle = the title of that last screen */
static void helper_install_now(int doneTitle)
{
    char path[80];
    int r;
    message(0, NULL, COLOR_TEXT, T(T_AUTO_INSTALLING));
    r = helper_install();
    helperState = helper_status();
    if (r != 0) {
        message_wait(COLOR_ERROR, T(T_HELPER_TITLE), COLOR_TEXT, helper_result(r));
        return;
    }
    snprintf(path, sizeof(path), "mc0:%s", helper_path());   /* (OPL didn't find it as "mc?:" when it was typed so) */
    dlg_new(COLOR_OK, T(doneTitle));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_AUTO_OPL));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 10, path);
    dlg_line(FONT_SMALL, COLOR_DIM, 4, T(T_AUTO_NOTE));
    if (helperLegacy)   /* OPL may still have the path of the helper in BOOT: that one was kept, and works */
        dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_HELPER_OLD_PATH));
    dlg_buttons(BUTTON_CROSS, T_FINISH, 0, 0);
    next.wide = 1;
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_CONFIRM);
}

/* removes the helper from the memory card in use, after asking */
static void helper_remove(void)
{
    int r;
    if (!confirm(T(T_HELPER_UNINSTALL_ASK), T(T_HELPER_UNINSTALL_TEXT), T_HELPER_UNINSTALL))
        return;
    r = helper_uninstall();
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_HELPER_UNINSTALLED : T_HELPER_UNINSTALL_ERROR));
}

/* SD2Cloud lives on a memory card, with the helper next to it: OPL runs that one, and all there is to do is tell OPL
 * where it is */
static void helper_on_card(int title)
{
    char path[120];
    snprintf(path, sizeof(path), "mc%d:%s/SD2CLOUD-IGR.ELF", appCardPort, appCardDir);
    dlg_new(COLOR_TITLE, T(title));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_HELPER_ABOUT));
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_HELPER_ON_CARD));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_AUTO_OPL));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 0, path);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, 0, 0);
    next.wide = 1;
    dlg_show();
    wait_button(PAD_CIRCLE | PAD_CROSS, 0);
    sound_play(SND_BACK);
}

/* The SAS package in the settings: SD2Cloud's folder on the memory card, with the IGR helper and the shortcut. Not
 * installed: says what it is, where it goes and how much of the memory card it takes, and installs it once the user
 * agrees. Installed: installs it again (or updates it), or removes it */
/* 1 = the helper was installed or removed (its state has to be read again) */
static int helper_screen(void)
{
    char place[80];
    if (appOnCard && helperState == HELPER_SAME) {
        helper_on_card(T_HELPER_TITLE);
        return 0;
    }
    if (!helper_present()) {
        dlg_new(COLOR_TITLE, T(T_HELPER_TITLE));
        dlg_line(FONT_TEXT, COLOR_ERROR, 0, T(T_HELPER_MISSING));
        next.wide = 1;
        dlg_buttons(BUTTON_CIRCLE, T_BACK, 0, 0);
        dlg_show();
        wait_button(PAD_CIRCLE | PAD_CROSS, 0);
        sound_play(SND_BACK);
        return 0;
    }
    if (helperState == HELPER_SAME || helperState == HELPER_DIFFERENT) {
        static const char *items[2];
        int k;
        items[0] = T(helperState == HELPER_DIFFERENT ? T_UPDATE : T_HELPER_REINSTALL);
        items[1] = T(T_HELPER_UNINSTALL);
        if ((k = choose(T(T_HELPER_TITLE), items, 2, 0)) < 0)
            return 0;
        if (k == 1) {
            helper_remove();
            return 1;
        }
    }
    snprintf(place, sizeof(place), "mc0:%s", helper_place());
    dlg_new(COLOR_TITLE, T(T_HELPER_TITLE));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_HELPER_ABOUT));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_HELPER_WHERE));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 4, place);
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, helper_space_text());
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_HELPER_AUTOBOOT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_HELPER_INSTALL);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    helper_install_now(T_HELPER_OK);
    return 1;
}

/* The program is another build than the last one that ran here (it was updated, or a newer file was put in its
 * place): the SAS package on the memory card in use, when the card has one of another version, is written again, so
 * the helper IGR runs is this version's and nobody has to see "Outdated" and do it by hand. Not asked: it is the
 * program's own package, there because the user installed it. Not the debug build's doing, which doesn't take the
 * program's place */
void helper_follow(void)
{
#ifndef DEBUG_BUILD
    char build[40];
    snprintf(build, sizeof(build), "%s %s", APP_VERSION, APP_COMMIT);
    if (!strcmp(cfg.app_build, build) || (helperState != HELPER_DIFFERENT && helperState != HELPER_SAME))
        return;   /* (with no package on this card, the one that has it is still to be seen) */
    if (helperState == HELPER_DIFFERENT) {
        int r;
        message(0, NULL, COLOR_TEXT, T(T_HELPER_UPDATING));
        r = helper_install();
        log_msg("SAS package: another build of the program (%s, it was %s), written again: %d", build, cfg.app_build, r);
        helperState = helper_status();
    }
    snprintf(cfg.app_build, sizeof(cfg.app_build), "%s", build);
    config_set("app", "app_build", build);
#endif
}

/* updates SD2Cloud: downloads, verifies, replaces and reopens the new version (which writes the memory card's SAS
 * package again: helper_follow) */
static void update_progress(long long done, long long total)
{
    ui_lock();
    dlg.permille = total ? (int)(done * 1000 / total) : 0;
    ui_unlock();
}

static void update_app(void)
{
    char c[260];
    int r;
    snprintf(c, sizeof(c), T(T_MENU_UPDATE), update_tag());
    if (!confirm(c, T(T_CONFIRM_UPDATE_TEXT), T_UPDATE))
        return;
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_UPDATE_RUNNING));
    if (appOnCard)   /* writing to a memory card takes a while: a bar for it */
        dlg_bar(0, NULL);
    dlg_show();
    googleError[0] = 0;
    if ((r = update_install(update_progress)) != 0) {
        char t[400];
        if (r == -3)
            snprintf(t, sizeof(t), T(T_HELPER_FULL), helperNeedKb, helperFreeKb);
        else
            snprintf(t, sizeof(t), "%s %s", T(T_UPDATE_ERROR), googleError);
        message_wait(0, NULL, COLOR_ERROR, t);
        return;
    }
    message(0, NULL, COLOR_OK, T(T_UPDATE_DONE));
    sound_play(SND_CONFIRM);
    sleep_ms(1500);
    if (appElsewhere)   /* written where it is, and started from there */
        run_elf_update(appPath, &updateApp, &updateIgr);
    if (appOnCard)
        snprintf(c, sizeof(c), "mc%d:%s/SD2CLOUD.ELF", appCardPort, appCardDir);
    else
        snprintf(c, sizeof(c), "%sSD2CLOUD.ELF", appDir);
    run_elf(c);
}

/* a path written by hand in sd2cloud.ini (another device, a program outside APPS): one more item at the end of a list
 * of targets, so choosing from the list doesn't lose it. Returns the new count */
static int add_custom(const char *target, const char **items, char (*values)[200], int n)
{
    static char name[2][96];
    static int which;
    char q[260], w[260];
    int i;
    if (!strcasecmp(target, "auto") || !strcasecmp(target, "osd"))
        return n;
    target_on_sd(target, q, sizeof(q));
    for (i = 0; i < n; i++) {
        target_on_sd(values[i], w, sizeof(w));
        if (!strcasecmp(w, q))
            return n;
    }
    which ^= 1;
    target_name(target, name[which], sizeof(name[0]));
    items[n] = name[which];
    snprintf(values[n], 200, "%s", target);
    return n + 1;
}

/* "Check for updates": asks GitHub (the only moment SD2Cloud looks for a new version: it never does by itself) and
 * offers the new version; otherwise says this one is the latest, or that GitHub couldn't be reached. back = 1: the
 * beta channel was just left, and the stable version is offered in place of the beta installed */
static void check_updates_now(int back)
{
    char t[200];
    int r;
    if (!updateAvailable) {
        if (!networkUp) {
            message(0, NULL, COLOR_TEXT, T(T_NET_STARTING));
            if ((r = network_up()) != 0) {
                snprintf(t, sizeof(t), "%s %s", T(T_UPDATE_CHECK_FAILED), T(r));
                message_wait(0, NULL, COLOR_ERROR, t);
                return;
            }
            networkUp = 1;
        }
        message(0, NULL, COLOR_TEXT, T(T_UPDATE_CHECKING));
        r = update_check(back);
        if (r < 0) {
            message_wait(0, NULL, COLOR_ERROR, T(T_UPDATE_CHECK_FAILED));
            return;
        }
        if (r == 0) {
            snprintf(t, sizeof(t), T(T_UPDATE_NONE), APP_VERSION);
            message_wait(0, NULL, COLOR_OK, t);
            return;
        }
        updateAvailable = 1;
        look_news(update_tag());
    }
    update_app();
}

/* Which device the microSD is in, when it has the cards of an sd2psx and those of a MemCard PRO2 (it was in both) and
 * the device didn't say which it is. startup = asked when the program opens, before the cards are looked for: what
 * was picked is used at once, and where it can be changed is said (circle leaves it for the next time). From the
 * settings, everything that came from the microSD is read again as that device's */
void pick_device(int startup)
{
    static const char *const items[2] = {"sd2psx", "MemCard PRO2"};
    int now = !dev->sd2psx, k = choose(T(startup ? T_DEVICE_ASK : T_SET_DEVICE), items, 2, now);
    if (k < 0 || (!startup && k == now))
        return;
    snprintf(cfg.device, sizeof(cfg.device), "%s", k ? "pro2" : "sd2psx");
    config_set("general", "device", cfg.device);
    log_msg("device: the user says it is %s", items[k]);
    if (!startup)
        reload_all();
    system_reload();
    ui_lock();
    i18n_device(dev->name);
    ui_unlock();
    message_wait(0, NULL, COLOR_TEXT, T(T_DEVICE_NOTE));
}

/* where the updates come from: the releases (stable) or the build made from every change (beta, after saying what
 * that means). The channel chosen is looked at right away; leaving the beta, that is the way back to the stable
 * version */
static void pick_channel(void)
{
    static const char *items[2];
    int k;
    items[0] = T(T_CHANNEL_STABLE);
    items[1] = "Beta";
    if ((k = choose(T(T_SET_CHANNEL), items, 2, cfg.beta)) < 0 || k == cfg.beta)
        return;
    if (k && !confirm(T(T_CHANNEL_BETA_ASK), T(T_CHANNEL_BETA_TEXT), T_YES))
        return;
    cfg.beta = k;
    config_set("update", "channel", k ? "beta" : "stable");
    updateAvailable = 0;   /* what was found before was the other channel's */
    look_news("");
    check_updates_now(!k);
}

/* where to go after IGR: automatic, a program in APPS, any ELF picked in the folders of the microSD or of a USB drive,
 * or the PS2 menu */
static void pick_return(void)
{
    static const char *items[MAX_APPS + 4], *devices[FDEVS];
    static char values[MAX_APPS + 4][200];
    char *now = cfg.igr_return, q[260], w[260];
    int n = 0, i, k, start = 0;
    items[n] = T(T_AUTO);
    snprintf(values[n++], sizeof(values[0]), "auto");
    for (i = 0; i < nApps; i++) {
        items[n] = given_name(apps[i].path) ? given_name(apps[i].path) : apps[i].title;
        target_for_ini(apps[i].path, values[n++], sizeof(values[0]));
    }
    items[n] = T(T_PICK_ELF);
    snprintf(values[n++], sizeof(values[0]), "files");
    items[n] = T(T_RET_OSD);
    snprintf(values[n++], sizeof(values[0]), "osd");
    n = add_custom(now, items, values, n);
    target_on_sd(now, q, sizeof(q));
    for (i = 0; i < n; i++) {
        target_on_sd(values[i], w, sizeof(w));
        if (!strcasecmp(w, q))
            start = i;
    }
    if ((k = choose(T(T_SET_IGR_RETURN), items, n, start)) < 0)
        return;
    if (!strcmp(values[k], "files")) {   /* the folders: X on an ELF chooses it */
        for (i = 0; i < FDEVS; i++)
            devices[i] = T(deviceText[i]);
        if ((i = choose(T(T_PICK_ELF), devices, FDEVS, 0)) < 0)
            return;
        fbPicked[0] = 0;
        fbPick = 1;
        files_screen(i);
        fbPick = 0;
        if (!fbPicked[0])
            return;
        snprintf(values[k], sizeof(values[0]), "%s", fbPicked);
    }
    if (!strcasecmp(values[k], now))
        return;
    if (cfg.igr_name[0]) {   /* the name was the other program's */
        cfg.igr_name[0] = 0;
        config_set("igr", "name", "");
    }
    snprintf(now, sizeof(cfg.igr_return), "%s", values[k]);
    config_set("igr", "return", values[k]);
    note_igr_auto(NULL);
}

/* the automatic sync at IGR, on or off: off, IGR goes straight to what comes after it. Without a Google account there
 * is nothing to turn on */
static void pick_auto_sync(void)
{
    static const char *items[2];
    int k;
    if (!google_has_access()) {
        message_wait(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_SYNC_NO_ACCOUNT));
        return;
    }
    items[0] = T(T_SYNC_ON);
    items[1] = T(T_SYNC_OFF);
    if ((k = choose(T(T_IGR_TITLE), items, 2, cfg.no_auto_sync)) < 0 || k == cfg.no_auto_sync)
        return;
    cfg.no_auto_sync = k;
    config_set("igr", "auto_sync", k ? "no" : "yes");
    note_igr_auto(NULL);
}

static void pick_language(void)
{
    static const char *const values[3] = {"auto", "pt", "en"};
    static const char *items[3];
    int k, start = !strcmp(cfg.language, "pt") ? 1 : !strcmp(cfg.language, "en") ? 2 : 0;
    items[0] = T(T_AUTO);
    items[1] = "Português";
    items[2] = "English";
    if ((k = choose(T(T_SET_LANGUAGE), items, 3, start)) < 0)
        return;
    snprintf(cfg.language, sizeof(cfg.language), "%s", values[k]);
    config_set("general", "language", values[k]);
    ui_lock();
    i18n_select(cfg.language);   /* every screen changes language at once */
    ui_unlock();
}

static void pick_keep(void)
{
    static const int values[6] = {3, 5, 10, 20, 50, 0};
    static char text[6][24];
    static const char *items[6];
    char v[16];
    int i, k, start = 2;
    for (i = 0; i < 6; i++) {
        if (values[i])
            snprintf(text[i], sizeof(text[i]), "%d", values[i]);
        else
            snprintf(text[i], sizeof(text[i]), "%s", T(T_KEEP_ALL));
        items[i] = text[i];
        if (values[i] == cfg.keep)
            start = i;
    }
    if ((k = choose(T(T_SET_KEEP), items, 6, start)) < 0)
        return;
    cfg.keep = values[k];
    snprintf(v, sizeof(v), "%d", values[k]);
    config_set("retention", "default", v);
}

static void pick_format(void)
{
    static const char *items[2];
    int k;
    items[0] = format_name(0);
    items[1] = format_name(1);
    if ((k = choose(T(T_SET_FORMAT), items, 2, cfg.ps2)) < 0 || k == cfg.ps2)
        return;
    cfg.ps2 = k;
    config_set("cards", "format", k ? "ps2" : "mcd");
}

static void about_screen(void)
{
    char t[120];
    /* with the commit it was built from, when the build was told: it is what tells one beta from another */
    snprintf(t, sizeof(t), "SD2Cloud v%s%s%s%s", APP_VERSION, APP_COMMIT[0] ? " (" : "", APP_COMMIT, APP_COMMIT[0] ? ")" : "");
#ifdef DEBUG_BUILD
    snprintf(t + strlen(t), sizeof(t) - strlen(t), " debug");
#endif
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_ABOUT_TEXT));
    dlg_line(FONT_SMALL, COLOR_DIM, 4, T(T_ABOUT_CREDITS));
    dlg_line(FONT_SMALL, COLOR_DIM, 10, T(T_ABOUT_LICENSES));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_ABOUT_AUTHOR));
    snprintf(t, sizeof(t), "github.com/%s", cfg.repo);
    dlg_line(FONT_SMALL, COLOR_ACCENT, 6, t);
    dlg_line(FONT_SMALL, COLOR_DIM, 0, appPath);   /* where the program is, as the IGR helper is told */
    dlg_buttons(BUTTON_CIRCLE, T_BACK, 0, 0);
    next.wide = 1;
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* circle on the main screen: where to go (the programs in APPS or the PS2 menu). The cursor starts on the last choice
 * (sd2cloud.ini [manual] return; "auto" = the OPL SD2Cloud would pick), and the choice is kept for next time */
void exit_menu(void)
{
    static const char *items[MAX_APPS + 3], *devices[FDEVS];
    static char values[MAX_APPS + 3][200];
    char q[260], w[260];
    int n = 0, i, k, start = 0;
    if (!appsListed)
        list_apps();
    for (i = 0; i < nApps; i++) {
        items[n] = given_name(apps[i].path) ? given_name(apps[i].path) : apps[i].title;
        target_for_ini(apps[i].path, values[n++], sizeof(values[0]));
    }
    items[n] = T(T_EXIT_FILES);   /* any ELF, picked in the folders of the microSD or of a USB drive */
    snprintf(values[n++], sizeof(values[0]), "files");
    items[n] = T(T_EXIT_BROWSER);
    snprintf(values[n++], sizeof(values[0]), "osd");
    n = add_custom(cfg.manual_return, items, values, n);
    if (strcasecmp(cfg.manual_return, "auto") != 0 || find_opl(q, sizeof(q)) != 0)
        target_on_sd(cfg.manual_return, q, sizeof(q));
    for (i = 0; i < n; i++) {
        target_on_sd(values[i], w, sizeof(w));
        if (!strcasecmp(w, q))
            start = i;
    }
    if ((k = choose(T(T_EXIT_TO), items, n, start)) < 0)
        return;
    if (!strcmp(values[k], "files")) {
        for (i = 0; i < FDEVS; i++)
            devices[i] = T(deviceText[i]);
        if ((i = choose(T(T_EXIT_FILES), devices, FDEVS, 0)) >= 0) {
            fbExit = 1;
            files_screen(i);   /* only comes back when nothing was run */
            fbExit = 0;
        }
        return;
    }
    if (strcasecmp(values[k], cfg.manual_return) != 0) {
        if (cfg.manual_name[0])   /* the name was the other program's */
            config_set("manual", "name", "");
        config_set("manual", "return", values[k]);
    }
    leave(values[k]);
}

void settings_screen(void)
{
    if (!appsListed)
        list_apps();
    if (helperStale) {   /* the card in the device was changed since: the package is the one of the card in use */
        helperStale = 0;
        helperState = helper_status();
    }
    build_settings();
    ui_scene(scene_settings);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE | PAD_START);
        if (b & (PAD_CIRCLE | PAD_START)) {
            sound_play(SND_BACK);
            return;
        }
        if (b & (PAD_UP | PAD_DOWN)) {
            ui_lock();
            st.cursor = (b & PAD_UP) ? (st.cursor + st.n - 1) % st.n : (st.cursor + 1) % st.n;
            ui_unlock();
            sound_play(SND_MOVE);
            continue;
        }
        sound_play(SND_CONFIRM);
        switch (st.item[st.cursor]) {
        case SET_SYNC_ALL:
            sync_all();
            break;
        case SET_AUTO_SYNC:
            pick_auto_sync();
            break;
        case SET_HELPER:
            if (helper_screen())   /* only then: reading the helper back from the memory card takes seconds */
                helperState = helper_status();
            break;
        case SET_IGR_RETURN:
            pick_return();
            break;
        case SET_DEVICE:
            pick_device(0);
            break;
        case SET_LANGUAGE:
            pick_language();
            break;
        case SET_KEEP:
            pick_keep();
            break;
        case SET_FORMAT:
            pick_format();
            break;
        case SET_UPDATES:
            check_updates_now(0);
            break;
        case SET_CHANNEL:
            pick_channel();
            break;
        case SET_ACCOUNT:
            account_screen();
            break;
        default:
            about_screen();
            break;
        }
        build_settings();
        ui_scene(scene_settings);
    }
}

/* right after signing in: offers the automatic sync (the IGR helper on the memory card in use). Installed, it shows
 * the path to set in OPL; declined, where to turn it on later. Not offered when the helper is already there */
void offer_auto_sync(void)
{
    if (appOnCard && helperState == HELPER_SAME) {   /* nothing to install: only OPL has to be told where it is */
        helper_on_card(T_IGR_TITLE);
        return;
    }
    if (helperState == HELPER_SAME || helperState == HELPER_NO_FILE)
        return;
    dlg_new(COLOR_TITLE, T(T_IGR_TITLE));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_AUTO_ASK));
    dlg_line(FONT_SMALL, COLOR_DIM, 4, T(T_AUTO_WHERE));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, helper_space_text());
    dlg_buttons(BUTTON_CIRCLE, T_LATER, BUTTON_CROSS, T_AUTO_ON);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        message_wait(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_AUTO_LATER));
        return;
    }
    sound_play(SND_CONFIRM);
    if (cfg.no_auto_sync) {   /* it had been turned off in the settings: this answer turns it on again */
        cfg.no_auto_sync = 0;
        config_set("igr", "auto_sync", "yes");
    }
    helper_install_now(T_AUTO_DONE);
}
