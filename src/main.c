/*
 * SD2Cloud -- backs up the sd2psx memory cards (sd2psXtd firmware) to Google Drive, straight from the PS2.
 *
 * Manual (started from the Apps tab): finds the cards on the microSD, checks which ones changed by the fingerprint of
 * their index (mcfs.c), and shows them: the list of cards on the left and the selected one, big, on the right. X opens
 * a card (its saves, the PS2 browser's way) to back it up now or to restore one of its backups from Drive; triangle
 * opens the options (back up the changed cards or all of them, the IGR helper, updating SD2Cloud). The first time it
 * connects the Google account (code + QR) and asks whether to back up every card.
 * IGR (started by the SD2CLOUD-IGR.ELF helper that OPL runs as "Exit to"): without asking anything, sends the new and
 * changed cards, says it's done (with a sound) and always returns to OPL, even without internet.
 * During any upload, circle asks whether to cancel; cancelling leaves SD2Cloud.
 *
 * The screens are scenes (ui.c draws the current one every frame on a thread of its own): each scene_* function
 * draws one from the state in this file, which the main thread only changes with the screen locked (ui_lock).
 *
 * On Drive: "PS2 Memory Card Backups/<card folder>/<card> YYYY-MM-DD HHhMM.zip", with rotation per card.
 */
#define _GNU_SOURCE   /* strcasestr */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <kernel.h>
#include <libpad.h>
#include "common.h"
#include "ui.h"
#include "look.h"
#include "icon.h"
#include "sound.h"

#define MAX_RESULTS 24

static int W, H, helperState = HELPER_NO_FILE;
static int networkUp, updateAvailable;
static char rootId[80];

typedef struct {
    char text[160];
    u32 color;
} result_t;
static result_t results[MAX_RESULTS];
static int nResults, sent, failed;

static int cancelLatched;          /* circle was pressed during an upload or download */
static int backupCancelled;        /* the user confirmed cancelling the backup */
static int restoring;              /* the backup running is the one before a restore (cancelling it doesn't leave) */
static const card_t *singleCard;   /* the card of run_backup's mode 3 */
static icon_t *sd2psxIcon;         /* the SD2PSX memory card, for the cards shared by many games */

/* ------------------------------------------------------------ small things */

static void format_size(long long bytes, char *out, size_t size)
{
    if (bytes >= 10LL * 1024 * 1024)
        snprintf(out, size, "%lld MB", (bytes + 512 * 1024) / (1024 * 1024));
    else if (bytes < 1024 * 1024)
        snprintf(out, size, "%lld KB", (bytes + 1023) / 1024);
    else
        snprintf(out, size, "%lld.%lld MB", bytes / (1024 * 1024), (bytes % (1024 * 1024)) * 10 / (1024 * 1024));
}

/* a date for the screen: 02/10/2026 21:10 in Portuguese, 2026-10-02 21:10 in English */
static void format_when(int year, int month, int day, int hour, int minute, char *out, size_t size)
{
    if (i18n_is_pt())
        snprintf(out, size, "%02d/%02d/%04d %02d:%02d", day, month, year, hour, minute);
    else
        snprintf(out, size, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
}

/* the date of a backup, from its name ("<card> YYYY-MM-DD HHhMM.zip"); else from Drive's createdTime (UTC) */
static void backup_when(const card_t *c, const drive_file_t *f, char *out, size_t size)
{
    int y, mo, d, h, mi;
    size_t n = strlen(c->base);
    if ((!strncmp(f->name, c->base, n) && sscanf(f->name + n, " %d-%d-%d %dh%d", &y, &mo, &d, &h, &mi) == 5) ||
        sscanf(f->created, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) == 5)
        format_when(y, mo, d, h, mi, out, size);
    else
        snprintf(out, size, "%s", f->name);
}

/* the last backup of a card, for the screen ("" = never) */
static void last_backup(const card_t *c, char *out, size_t size)
{
    card_state_t *e = state_card(c->id, 0);
    int yr, mo, d, h, mi;
    out[0] = 0;
    if (e && e->sha[0] && sscanf(e->when, "%d-%d-%d %d:%d", &yr, &mo, &d, &h, &mi) == 5)
        format_when(yr, mo, d, h, mi, out, size);
}

static u32 status_color(const card_t *c)
{
    return !c->included ? COLOR_DIM : c->status == ST_UP_TO_DATE ? COLOR_OK : c->status == ST_ERROR ? COLOR_ERROR : COLOR_WARN;
}

static const char *status_text(const card_t *c)
{
    const card_state_t *e;
    if (c->included && c->status == ST_CHANGED && (!(e = state_card(c->id, 0)) || !e->sha[0]))
        return T(T_ST_NEW);   /* changed since it was first seen, but never synced */
    return !c->included ? T(T_ST_SKIPPED) : c->status == ST_UP_TO_DATE ? T(T_ST_UP_TO_DATE)
           : c->status == ST_CHANGED ? T(T_ST_CHANGED) : c->status == ST_ERROR ? T(T_ST_ERROR) : T(T_ST_NEW);
}

/* ------------------------------------------------------------ the dialog: a box with a title, text, a bar, buttons */

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
static dialog_t dlg, next;

static void scene_dialog(float t)
{
    int w = dlg.wide ? 520 : 440, x = (W - w) / 2, pad = 28, inner = w - 2 * pad, h = 2 * pad, y, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    if (dlg.title[0])
        h += ui_paragraph_height(FONT_TEXT, inner, dlg.title) + 10;
    for (i = 0; i < dlg.nlines; i++)
        h += ui_paragraph_height(dlg.line[i].font, inner, dlg.line[i].text) + dlg.line[i].gap;
    if (dlg.permille >= 0)
        h += 18 + (dlg.note[0] ? 22 : 0);
    if (h < 130)
        h = 130;
    y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2;
    look_panel(x, y, w, h);
    y += pad;
    if (h == 130 && !dlg.title[0] && dlg.nlines == 1 && dlg.permille < 0)   /* a single short line: in the middle */
        y += (130 - 2 * pad - ui_paragraph_height(dlg.line[0].font, inner, dlg.line[0].text)) / 2;
    if (dlg.title[0]) {
        if (dlg.titleColor == COLOR_TITLE && ui_paragraph_height(FONT_TEXT, inner, dlg.title) <= ui_line_height(FONT_TEXT) + 2) {
            look_title(x + pad, y, dlg.title, 0);
            y += ui_line_height(FONT_TEXT) + 2;
        } else
            y = ui_paragraph(FONT_TEXT, x + pad, y, inner, dlg.titleColor, dlg.title);
        y += 10;
    }
    for (i = 0; i < dlg.nlines; i++)
        y = ui_paragraph(dlg.line[i].font, x + pad, y, inner, dlg.line[i].color, dlg.line[i].text) + dlg.line[i].gap;
    if (dlg.permille >= 0) {
        look_bar(x + pad, y + 8, inner, dlg.permille);
        if (dlg.note[0])
            ui_text_right(FONT_SMALL, x + pad + inner, y + 20, COLOR_DIM, dlg.note);
    }
    look_legend(dlg.legend, dlg.nlegend, 0);
}

static void dlg_new(u32 titleColor, const char *title)
{
    memset(&next, 0, sizeof(next));
    next.permille = -1;
    next.titleColor = titleColor;
    snprintf(next.title, sizeof(next.title), "%s", title ? title : "");
}

static void dlg_line(int font, u32 color, int gap, const char *text)
{
    if (next.nlines == DLG_LINES || !text || !text[0])
        return;
    snprintf(next.line[next.nlines].text, sizeof(next.line[0].text), "%s", text);
    next.line[next.nlines].color = color;
    next.line[next.nlines].font = font;
    next.line[next.nlines++].gap = gap;
}

static void dlg_bar(int permille, const char *note)
{
    next.permille = permille < 0 ? 0 : permille;
    snprintf(next.note, sizeof(next.note), "%s", note ? note : "");
}

/* the buttons, left to right (text ids; 0 = none) */
static void dlg_buttons(int b1, int t1, int b2, int t2)
{
    next.nlegend = 0;
    if (t1)
        next.legend[next.nlegend].button = b1, next.legend[next.nlegend++].text = T(t1);
    if (t2)
        next.legend[next.nlegend].button = b2, next.legend[next.nlegend++].text = T(t2);
}

static void dlg_show(void)
{
    ui_lock();
    dlg = next;
    ui_unlock();
    ui_scene(scene_dialog);
}

/* only the background and the frame: between two screens */
static void scene_frame(float t)
{
    (void)t;
    look_space();
    look_frame();
}

/* a message: title (may be NULL) and a text */
static void message(u32 titleColor, const char *title, u32 textColor, const char *text)
{
    dlg_new(titleColor, title);
    dlg_line(FONT_TEXT, textColor, 0, text);
    dlg_show();
}

/* a message that waits for X or circle */
static void message_wait(u32 titleColor, const char *title, u32 textColor, const char *text)
{
    dlg_new(titleColor, title);
    dlg_line(FONT_TEXT, textColor, 0, text);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* asks before doing something: 1 = X (yesText), 0 = circle */
static int confirm(const char *title, const char *text, int yesText)
{
    dlg_new(COLOR_TITLE, title);
    if (text)
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, text);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, yesText);
    dlg_show();
    if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        return 1;
    }
    sound_play(SND_BACK);
    return 0;
}

/* ------------------------------------------------------------ leaving */

static void on_title_cfg(const char *s, const char *k, const char *v, void *u)
{
    (void)s;
    if (!strcasecmp(k, "boot"))
        snprintf((char *)u, 64, "%s", v);
}

static int looks_like_opl(const char *dir, const char *elf)
{
    return strcasestr(dir, "OPL") || strcasestr(elf, "OPL") || strcasestr(elf, "OPNPS2LD");
}

/* does any .cfg in that OPL folder point the "IGR Path" (exit_path) to SD2Cloud's helper? */
static int points_to_helper(const char *dir)
{
    static char cfgs[16][64];
    char c[260];
    DIR *d;
    struct dirent *e;
    int n = 0, i, found = 0;
    if ((d = opendir(dir)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < 16) {
            size_t len = strlen(e->d_name);
            if (len > 4 && !strcasecmp(e->d_name + len - 4, ".cfg"))
                snprintf(cfgs[n++], sizeof(cfgs[0]), "%s", e->d_name);
        }
        closedir(d);
    }
    for (i = 0; i < n && !found; i++) {
        buffer_t b = {0};
        char *l, *nl;
        snprintf(c, sizeof(c), "%s/%s", dir, cfgs[i]);
        if (file_read(c, &b) == 0 && b.data)
            for (l = (char *)b.data; l && *l && !found; l = nl ? nl + 1 : NULL) {
                if ((nl = strchr(l, '\n')) != NULL)
                    *nl = 0;   /* one line at a time */
                if (!strncmp(l, "exit_path", 9) && strcasestr(l, "SD2CLOUD-IGR"))
                    found = 1;
            }
        buf_free(&b);
    }
    return found;
}

/* "auto": the OPL to return to, without the user saying which. 1) the OPL in APPS whose settings point the "IGR Path"
 * to our helper (that's the one using SD2Cloud, even with other OPLs installed); 2) the first OPL found in APPS (by its
 * title.cfg and name); 3) a loose APPS/OPNPS2LD.ELF. 0 = found, in out */
static int find_opl(char *out, size_t size)
{
    static char dirs[64][64];
    char base[64], dir[160], c[260], elf[64], first[260] = "";
    DIR *d;
    struct dirent *e;
    int n = 0, i;
    snprintf(base, sizeof(base), "%sAPPS", sdRoot);
    if ((d = opendir(base)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < 64)
            if (e->d_name[0] != '.' && strcasecmp(e->d_name, "SD2Cloud"))
                snprintf(dirs[n++], sizeof(dirs[0]), "%s", e->d_name);
        closedir(d);
    }
    for (i = 0; i < n; i++) {
        elf[0] = 0;
        snprintf(dir, sizeof(dir), "%s/%s", base, dirs[i]);
        snprintf(c, sizeof(c), "%s/title.cfg", dir);
        ini_read(c, on_title_cfg, elf);
        if (!elf[0] || !looks_like_opl(dirs[i], elf))
            continue;
        snprintf(c, sizeof(c), "%s/%s", dir, elf);
        if (!file_exists(c))
            continue;
        if (points_to_helper(dir)) {
            log_msg("OPL to return to: %s (its IGR Path points to SD2Cloud)", c);
            snprintf(out, size, "%s", c);
            return 0;
        }
        if (!first[0])
            snprintf(first, sizeof(first), "%s", c);
    }
    if (!first[0]) {
        snprintf(c, sizeof(c), "%sAPPS/OPNPS2LD.ELF", sdRoot);
        if (file_exists(c))
            snprintf(first, sizeof(first), "%s", c);
    }
    if (!first[0])
        return -1;
    log_msg("OPL to return to: %s (the first one found in APPS)", first);
    snprintf(out, size, "%s", first);
    return 0;
}

/* the path in the .ini (the main way: the user says where the OPL is), "osd" = the PS2 menu, "auto" = look for it
 * (find_opl). If a path on the sd2psx doesn't exist (typo, OPL moved), look for it before falling back to the menu; a
 * path on another device (USB, MX4SIO, HDD, memory card) is checked by run_elf, after loading that device's drivers */
static void return_to(const char *target) __attribute__((noreturn));
static void return_to(const char *target)
{
    char c[260], q[260], *p;
    int found = -1;
    snprintf(q, sizeof(q), "%s", target);
    if ((p = strstr(q, "mmce?:")) != NULL)   /* mmce?: becomes the microSD's slot */
        p[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
    if (strcasecmp(q, "osd") != 0 && strcasecmp(q, "auto") != 0 && device_of(q) == DEV_SD && !file_exists(q)) {
        log_msg("the path in the .ini doesn't exist (%s): looking for the OPL", q);
        strcpy(q, "auto");
    }
    if (!strcasecmp(q, "auto"))
        found = find_opl(c, sizeof(c));
    log_msg("returning to: %s", found == 0 ? c : (!strcasecmp(q, "auto") ? "osd" : q));
    if (!strcasecmp(q, "osd"))
        go_osd();
    if (strcasecmp(q, "auto") != 0)
        run_elf(q);
    if (found == 0)
        run_elf(c);
    go_osd();
}

/* leaving with the exit sound: it plays to the end before the next program takes over */
static void leave(const char *target) __attribute__((noreturn));
static void leave(const char *target)
{
    sound_play(SND_EXIT);
    sound_wait(SND_EXIT);
#ifdef TEST
    test_capture_if('F');   /* the last screen, before leaving */
#endif
    return_to(target);
}

/* ------------------------------------------------------------ checking */

static void checking_progress(int i, int n, const card_t *c)
{
    char t[160];
    dlg_new(COLOR_TITLE, igrMode ? T(T_IGR_TITLE) : T(T_SEARCHING));
    snprintf(t, sizeof(t), "%s %s", T(T_CHECKING), c->name[0] ? c->name : c->base);
    dlg_line(FONT_TEXT, COLOR_DIM, 0, t);
    dlg_bar(n ? (i * 1000) / n : 0, NULL);
    dlg_show();
#ifdef TEST
    if (n && i * 2 >= n)
        test_capture_if('P');
#endif
}

/* ------------------------------------------------------------ Google */

static struct {
    char url[128], code[32];
    int minutes;
} login;

static void scene_login(float t)
{
    int x = 64, y = 100, w = W - 128, h = 252, pad = 26, side, qx, tw, lh;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, w, h);
    look_title(x + pad, y + pad, T(T_LOGIN_TITLE), 0);
    y += pad + ui_line_height(FONT_TEXT) + 16;
    tw = w - 2 * pad - 170;
    ui_text_fit(FONT_TEXT, x + pad, y, tw, COLOR_TEXT, T(T_LOGIN_OPEN));
    y += ui_line_height(FONT_TEXT) + 4;
    {   /* "google.com/device": without https:// and www. it fits in big letters, and works the same typed */
        const char *u = !strncmp(login.url, "https://", 8) ? login.url + 8 : login.url;
        if (!strncmp(u, "www.", 4))
            u += 4;
        ui_text_fit(FONT_TITLE, x + pad + 10, y, tw - 10, COLOR_ACCENT, u);
    }
    y += ui_line_height(FONT_TITLE) + 10;
    ui_text_fit(FONT_TEXT, x + pad, y, tw, COLOR_TEXT, T(T_LOGIN_ENTER));
    y += ui_line_height(FONT_TEXT) + 6;
    lh = ui_line_height(FONT_TITLE);
    ui_rect(x + pad, y, ui_measure(FONT_TITLE, login.code) + 32, lh + 12, 0x02060E, 0x50);
    ui_text_glow(FONT_TITLE, x + pad + 16, y + 6, COLOR_TITLE, 0x4A3E08, login.code);
    /* the QR on the right, its caption below it */
    qx = x + w - pad - 148;
    side = ui_qr(login.url, qx, 100 + pad, 4);
    if (side)
        ui_text_center(FONT_SMALL, qx + side / 2.0f, 100 + pad + side + 6, COLOR_DIM, T(T_LOGIN_QR));
    {
        char e[96];
        snprintf(e, sizeof(e), T(T_LOGIN_EXPIRES), login.minutes);
        ui_text(FONT_SMALL, x + pad, 100 + h - pad - ui_line_height(FONT_SMALL), COLOR_DIM, e);
    }
    {
        legend_t l = {BUTTON_CIRCLE, T(T_LATER)};
        look_legend(&l, 1, 0);
    }
}

static void show_login(const char *url, const char *code, int seconds)
{
    ui_lock();
    snprintf(login.url, sizeof(login.url), "%s", url);
    snprintf(login.code, sizeof(login.code), "%s", code);
    login.minutes = (seconds + 59) / 60;
    ui_unlock();
    ui_scene(scene_login);
#ifdef TEST
    /* on PCSX2 the login screen is captured and the test stops there (see test_capture_and_stop in system.c) */
    if (!strncmp(appDir, "host:", 5))
        test_capture_and_stop();
#endif
}

static int cancel_login(void) { return (pad_buttons() & PAD_CIRCLE) != 0; }

/* network + access to Google. 0 = ok, -1 = the user backed out of the sign-in (nothing to say), else the id of the
 * error message */
static int ensure_google(int allowLogin)
{
    int r;
    if (!networkUp) {
        message(0, NULL, COLOR_TEXT, T(T_NET_STARTING));
        if ((r = network_up()) != 0)
            return r;
        networkUp = 1;
    }
    if (google_init() != 0)
        return T_ERR_INTERNET;
    if (google_has_access()) {
        message(0, NULL, COLOR_TEXT, T(T_SIGNING_IN));
        r = google_refresh();
        if (r == 0)
            return 0;
        if (r == -1)
            return T_ERR_INTERNET;
        refreshToken[0] = 0;   /* revoked or expired */
        if (!allowLogin)
            return T_IGR_NO_LOGIN;
        message(0, NULL, COLOR_WARN, T(T_LOGIN_REVOKED));
        sleep_ms(3000);
    }
    if (!allowLogin)
        return T_IGR_NO_LOGIN;
    r = google_login(show_login, cancel_login);
    if (r != 0)
        return r;
    message(0, NULL, COLOR_OK, T(T_LOGIN_OK));
    sound_play(SND_CONFIRM);
    sleep_ms(1500);
    return 0;
}

/* ------------------------------------------------------------ cancelling */

/* watches the controller during uploads and downloads (curl calls it too, while it transfers): a new press of circle =
 * the user wants to cancel. Only the press counts, not the button held down: the circle that answered "keep going"
 * may still be held when the upload resumes. The question itself waits for the next progress update */
static int circleDown;

static void watch_cancel(void)
{
    int down = (pad_buttons() & PAD_CIRCLE) != 0;
    if (down && !circleDown)
        cancelLatched = 1;
    circleDown = down;
#ifdef TEST
    if (test_take('K'))
        cancelLatched = 1;
#endif
}

/* 1 = yes, cancel */
static int confirm_cancel(int title, int text)
{
    int yes;
    sound_play(SND_BACK);
    dlg_new(COLOR_WARN, T(title));
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(text));
    dlg_buttons(BUTTON_CIRCLE, T_CANCEL_NO, BUTTON_CROSS, T_CANCEL_YES);
    dlg_show();
    yes = (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) != 0;
    sound_play(yes ? SND_CONFIRM : SND_BACK);
    cancelLatched = 0;
    circleDown = 1;   /* the circle of "keep going": it counts again only after being released */
    log_msg("cancel? %s", yes ? "yes" : "no, keep going");
    return yes;
}

/* ------------------------------------------------------------ backup */

static const card_t *current;
static int currentN, totalN;   /* "Uploading currentN of totalN" */

/* the icon of the card being sent: on a game's card (Game ID), the 3D icon of its newest save; on a card shared by
 * many games (CardN, named folders, BootCard) or when the save's icon can't be read, the SD2PSX memory card */
static icon_t *cardIcon;
static u64 iconStart;
/* a single save on its way (save_to_cloud): the screen shows its icon (the saves screen's, not freed here) and its
 * name instead of the card's */
static icon_t *saveIcon;
static char saveTitle[100];
static volatile long long shownDone, shownTotal;

static void load_card_icon(const card_t *c)
{
    buffer_t iconsys = {0}, ico = {0};
    char folder[33];
    icon_t *ic = NULL;
    if (c->type == TYPE_GAMEID && mcfs_newest_save_icon(c->path, folder, &iconsys, &ico) == 0 && (ic = icon_load(&iconsys, &ico)))
        log_msg("%s: icon of %s (%d vertices, %d shapes) \"%s\"", c->id, folder, ic->nv, ic->shapes, ic->title);
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    if (cardIcon != sd2psxIcon)
        icon_free(cardIcon);
    cardIcon = ic ? ic : sd2psxIcon;
    current = c;
    iconStart = now_ms();
    ui_unlock();
}

/* the screen while a card is sent, like a console's saving screen: the icon turning on the left, in a soft light; the
 * game (or the card), the card and what's happening on the right, over a thin white bar */
static void scene_upload(float t)
{
    char s[200];
    const card_t *c = current;
    long long done = shownDone, total = shownTotal;
    const char *title;
    int x = 300, y, w = LOOK_LINE_X1 - 8 - 300;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    if (!c)
        return;
    title = saveIcon ? saveTitle
            : c->type == TYPE_GAMEID && cardIcon && cardIcon != sd2psxIcon && cardIcon->title[0] ? cardIcon->title
            : c->name[0] ? c->name : c->base;
    look_halo(168, 222, 118, 0x1A);
    icon_draw(saveIcon ? saveIcon : cardIcon, 168, 218, 210, (now_ms() - iconStart) / 1000.0f);
    y = ui_paragraph(FONT_TITLE, x, 138, w, COLOR_TEXT, title) + 2;
    if (strcmp(title, c->base)) {
        ui_text_fit(FONT_SMALL, x, y, w, COLOR_DIM, c->base);
        y += ui_line_height(FONT_SMALL) + 4;
    }
    y += 18;
    ui_text_fit(FONT_TEXT, x, y, w, COLOR_TEXT, T(T_BACKING_UP));
    y += ui_line_height(FONT_TEXT) + 12;
    look_bar(x, y, w, total ? (int)(done * 1000 / total) : 0);
    if (totalN > 1) {
        snprintf(s, sizeof(s), T(T_UPLOADING), currentN, totalN);
        ui_text_right(FONT_SMALL, x + w, y + 12, COLOR_DIM, s);
    }
    {
        legend_t l = {BUTTON_CIRCLE, T(T_CANCEL)};
        look_legend(&l, 1, 0);
    }
}

static void upload_screen(long long done, long long total)
{
    shownDone = done;
    shownTotal = total;
    ui_scene(scene_upload);
}

#ifdef TEST
/* script "V<n>" at the start: shows the backup screen of card n (in the menu's order) for 1.5 s without sending
 * anything, and captures it: to check the icons on PCSX2 */
static void test_preview(int k)
{
    u64 end;
    if (k < 0 || k >= nCards)
        return;
    currentN = totalN = 1;
    load_card_icon(&cards[k]);
    /* the main thread only sleeps, as if it were waiting on the network: the animation must keep going by itself */
    end = now_ms() + 1500;
    while (now_ms() < end) {
        upload_screen((long long)(now_ms() - iconStart), 3000);
        sleep_ms(100);
    }
    test_capture_and_stop();
}
#endif

static int on_progress(long long done, long long total)
{
    watch_cancel();
    if (cancelLatched) {
        if (confirm_cancel(restoring ? T_RESTORE_CANCEL_TITLE : T_CANCEL_TITLE,
                           restoring ? T_RESTORE_CANCEL_TEXT : igrMode ? T_CANCEL_TEXT : T_CANCEL_TEXT_MANUAL)) {
            backupCancelled = 1;
            return 1;
        }
    }
    upload_screen(done, total);
#ifdef TEST
    if (done * 2 >= total && done > 0 && test_take('E'))
        test_capture_and_stop();
#endif
    return 0;
}

static void add_result(u32 color, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void add_result(u32 color, const char *fmt, ...)
{
    va_list ap;
    if (nResults == MAX_RESULTS) {
        memmove(results, results + 1, sizeof(results[0]) * (MAX_RESULTS - 1));
        nResults--;
    }
    va_start(ap, fmt);
    vsnprintf(results[nResults].text, sizeof(results[0].text), fmt, ap);
    va_end(ap);
    results[nResults++].color = color;
    log_msg("%s", results[nResults - 1].text);
}

/* mode 0 = IGR (new and changed), 1 = manual (everything that is not up to date), 2 = every included card,
 * 3 = only singleCard (picked in the menu; included or not) */
static int is_selected(const card_t *c, int mode)
{
    if (mode == 3)
        return c == singleCard;
    if (!c->included || c->status == ST_ERROR)
        return 0;
    if (mode == 2)
        return 1;
    if (mode == 0)
        return c->status == ST_NEW || c->status == ST_CHANGED;
    return c->status != ST_UP_TO_DATE;
}

static void delete_old(const card_t *c, const char *folder, const char *newId)
{
    static drive_file_t list[200];
    int keep = config_keep(c->id), n, i, extra;
    if (keep <= 0)
        return;
    n = google_list(folder, c->id, list, 200);
    if (n < 0) {
        log_msg("%s: couldn't list to delete the old ones (next time, then)", c->id);
        return;
    }
    for (i = 0, extra = n - keep; i < n && extra > 0; i++) {
        if (!strcmp(list[i].id, newId))
            continue;
        if (google_delete(list[i].id) == 0)
            log_msg("deleted: %s", list[i].name);
        extra--;
    }
}

/* the main Drive folder and the card's folder in it, from the cache or found (created if missing). 0 = ok */
static int card_folder(const card_t *c, char *folderId, size_t size)
{
    char key[200];
    const char *cache;
    if (!rootId[0]) {
        cache = state_folder(cfg.drive_folder);
        snprintf(rootId, sizeof(rootId), "%s", cache ? cache : "");
        if (google_folder(cfg.drive_folder, "root", rootId, sizeof(rootId)) != 0) {
            rootId[0] = 0;
            return -1;
        }
        state_set_folder(cfg.drive_folder, rootId);
    }
    snprintf(key, sizeof(key), "%s/%s", cfg.drive_folder, c->folder);
    cache = state_folder(key);
    snprintf(folderId, size, "%s", cache ? cache : "");
    if (google_folder(c->folder, rootId, folderId, size) != 0)
        return -1;
    state_set_folder(key, folderId);
    return 0;
}

/* rotate = delete the backups beyond the configured limit (not before a restore: the backup picked to restore could
 * be the oldest one) */
static void run_backup(int mode, int rotate)
{
    char name[160], folderId[80], t[200];
    int i, r;
    datetime_t when;

    nResults = sent = failed = 0;
    backupCancelled = cancelLatched = 0;
    googleError[0] = 0;
    r = ensure_google(!igrMode);
    if (r < 0) {   /* backed out of the sign-in: the same as cancelling */
        backupCancelled = 1;
        return;
    }
    if (r != 0) {
        add_result(COLOR_ERROR, "%s %s", T(r), googleError);
        failed++;
        return;
    }
    /* the main folder is checked once per run (it may have been deleted on Drive) */
    rootId[0] = 0;

    for (i = 0, totalN = 0; i < nCards; i++)
        totalN += is_selected(&cards[i], mode);
    currentN = 0;
    for (i = 0; i < nCards && !backupCancelled; i++) {
        card_t *c = &cards[i];
        card_state_t *e;
        stream_t s;
        char id[80];
        if (!is_selected(c, mode))
            continue;
        currentN++;
        googleError[0] = 0;
        load_card_icon(c);
        upload_screen(0, c->size);
        if (card_folder(c, folderId, sizeof(folderId)) != 0) {
            add_result(COLOR_ERROR, "%s: %s (%s)", c->base, T(T_UPLOAD_FAILED), googleError);
            failed++;
            continue;
        }
        local_time(&when);
        e = state_card(c->id, 0);
        /* two backups of the same card in the same minute get the seconds in the name */
        snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", when.year, when.month, when.day, when.hour, when.minute);
        if (e && !strncmp(e->when, t, 16))
            snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02dm%02d.zip", c->base, when.year, when.month, when.day, when.hour,
                     when.minute, when.second);
        else
            snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02d.zip", c->base, when.year, when.month, when.day, when.hour, when.minute);
        memset(&s, 0, sizeof(s));
        r = google_upload(c, folderId, name, &when, &s, on_progress, id, sizeof(id));
        if (r != 0) {
            if (backupCancelled) {
                log_msg("%s: upload cancelled", c->id);
                break;
            }
            add_result(COLOR_ERROR, "%s: %s (%s)", c->base, T(T_UPLOAD_FAILED), googleError);
            failed++;
            continue;
        }
        e = state_card(c->id, 1);
        if (e) {
            snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
            e->version = MCFS_VERSION;
            snprintf(e->sha, sizeof(e->sha), "%s", s.sha_mcd);
            snprintf(e->when, sizeof(e->when), "%04d-%02d-%02d %02d:%02d:%02d", when.year, when.month, when.day, when.hour, when.minute,
                     when.second);
        }
        state_write();
        c->status = ST_UP_TO_DATE;
        sent++;
        format_size(s.sent, t, sizeof(t));
        add_result(COLOR_OK, "%s: %s (%s)", c->base, T(T_UPLOAD_OK), t);
        if (rotate)
            delete_old(c, folderId, id);
    }
    state_write();
}

/* the results of a backup; seconds = 0 waits for a button, else shows them that long (IGR) */
static void summary_screen(int seconds)
{
    char t[120];
    int i;
    dlg_new(!failed ? COLOR_OK : sent ? COLOR_WARN : COLOR_ERROR, T(!failed ? T_SUMMARY_OK : sent ? T_SUMMARY_PARTIAL : T_SUMMARY_FAILED));
    for (i = nResults > DLG_LINES ? nResults - DLG_LINES : 0; i < nResults; i++)
        dlg_line(FONT_SMALL, results[i].color, 2, results[i].text);
    if (seconds) {
        snprintf(t, sizeof(t), T(T_RETURNING), seconds);
        dlg_line(FONT_SMALL, COLOR_DIM, 0, t);
    } else
        dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    next.wide = 1;
    dlg_show();
    if (!seconds)
        sound_play(failed ? SND_BACK : SND_EXIT);
    wait_button(PAD_CROSS | PAD_CIRCLE, seconds);
    if (!seconds)
        sound_play(SND_BACK);
}

/* a backup from the manual menu: cancelling it goes back to where it was started (only IGR leaves SD2Cloud); what
 * was sent before the cancel is still shown */
static void manual_backup(int mode)
{
    run_backup(mode, 1);
    if (backupCancelled && !sent && !failed)
        return;
    summary_screen(0);
}

/* ------------------------------------------------------------ manual */

static void count_cards(int *included, int *changed, long long *bytes)
{
    int i;
    *included = *changed = 0;
    *bytes = 0;
    for (i = 0; i < nCards; i++)
        if (cards[i].included) {
            (*included)++;
            *bytes += cards[i].size;
            if (is_selected(&cards[i], 1))
                (*changed)++;
        }
}

/* every card seen for the first time keeps its fingerprint as the reference, even without a backup: IGR then sends
 * only the cards that change from now on (and the cards of new games), not every card that was never synced */
static void remember_unseen(void)
{
    int i, n = 0;
    for (i = 0; i < nCards; i++)
        if (cards[i].status == ST_NEW && cards[i].fingerprint[0]) {   /* a card left out of the sync was never read */
            card_state_t *e = state_card(cards[i].id, 1);
            if (e && !e->fingerprint[0]) {
                snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", cards[i].fingerprint);
                e->version = MCFS_VERSION;
                cards[i].status = ST_NO_BACKUP;
                n++;
            }
        }
    if (n) {
        i = state_write();
        log_msg("%d card(s) seen for the first time: fingerprint kept as the reference (%d)", n, i);
    }
}

/* first run: offers to sync every card; on "later", nothing is sent (remember_unseen keeps the references) */
static void first_run(void)
{
    char t[300], size[24];
    int included, changed, minutes;
    long long bytes;
    count_cards(&included, &changed, &bytes);
    if (!included)
        return;
    format_size(bytes, size, sizeof(size));
    minutes = (int)((bytes / (1024 * 1024)) * 2 / 60) + 1;   /* ~2 s per MB on the console (read + compress + upload) */
    dlg_new(COLOR_TITLE, T(T_FIRST_TITLE));
    snprintf(t, sizeof(t), T(T_FIRST_QUESTION), included, size, minutes);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, t);
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_FIRST_AFTER));
    dlg_buttons(BUTTON_CIRCLE, T_LATER, BUTTON_CROSS, T_YES);
    dlg_show();
    if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        manual_backup(2);
        return;
    }
    sound_play(SND_BACK);
    log_msg("[first run] later");
}

/* -------- the big glass card on the right (the main screen and the backups list) */

#define LIST_X     86     /* the list on the left */
#define LIST_W     208    /* its width: the tabs and the counter are centered on it */
#define LIST_Y     104
#define ROW_Y0     158    /* the first row */
#define ROW_H      30
#define ROWS       5
#define CARD_X     392
#define CARD_Y     100
#define CARD_CX    (CARD_X + LOOK_CARD_W / 2)

static void card_label(const card_t *c, char *number, char *label)
{
    number[0] = label[0] = 0;
    if (c->type == TYPE_NORMAL && !strncmp(c->folder, "Card", 4))
        snprintf(number, 8, "%s", c->folder + 4);
    else
        snprintf(label, 48, "%s", c->folder);
}

/* a card's big picture: the glass card with its number, or with its name and, on a game card, its save's icon */
static void draw_card_picture(const card_t *c, icon_t *ic, float t)
{
    char number[8], label[48];
    card_label(c, number, label);
    look_card(CARD_X, CARD_Y, number, ic ? NULL : label);
    if (ic) {
        icon_draw(ic, CARD_CX, CARD_Y + 82, 104, t);
        if (ui_measure(FONT_SMALL, label) <= LOOK_CARD_W - 60)
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + 146, 0x8CD4DA, label);
        else
            ui_text_fit(FONT_SMALL, CARD_X + 30, CARD_Y + 146, LOOK_CARD_W - 60, 0x8CD4DA, label);
    }
}

/* the rows of a list: scrolls with the cursor, arrows when there's more */
static void list_rows(int n, int cursor, int top, const char *(*text)(int i, char *buf), u32 (*dot)(int i))
{
    char buf[64];
    int i, y;
    for (i = top, y = ROW_Y0; i < n && i < top + ROWS; i++, y += ROW_H) {
        const char *s = text(i, buf);
        if (dot) {   /* the card's status: a small colored light */
            float cy = y + ui_line_height(FONT_TEXT) / 2.0f + 1;
            ui_image(IMG_GLOW, LIST_X + 21, cy - 7, 14, 14, dot(i), 0x80);
            ui_rect(LIST_X + 27, cy - 1, 2, 2, dot(i), 0x80);
        }
        /* without the status light (the backups' dates) the text starts further left: a whole date fits */
        if (i == cursor)
            look_item(LIST_X + (dot ? 42 : 24), y, s, 1, 0);
        else
            ui_text_fit(FONT_TEXT, LIST_X + (dot ? 42 : 24), y, dot ? 150 : 172, COLOR_ITEM, s);
    }
    if (top > 0)
        look_arrow(LIST_X + 186, ROW_Y0 - 14, 0, 0x6E9AE0);
    if (top + ROWS < n)
        look_arrow(LIST_X + 186, ROW_Y0 + ROWS * ROW_H, 1, 0x6E9AE0);
    if (n > 0) {   /* "3/12" under the list: where the cursor is in it */
        char t[24];
        snprintf(t, sizeof(t), "%d/%d", cursor + 1, n);
        ui_text_center(FONT_SMALL, LIST_X + LIST_W / 2.0f, LIST_Y + 222, 0x8E98AA, t);
    }
}

static void scroll_to(int cursor, int *top)
{
    if (cursor < *top)
        *top = cursor;
    if (cursor >= *top + ROWS)
        *top = cursor - ROWS + 1;
    if (*top < 0)
        *top = 0;
}

/* -------- the cards in groups, one shown at a time, chosen with left/right (or L1/R1) over the list: the numbered
 * cards (CardN, with the folders the user named), the game cards (Game ID) and the boot cards. A group without cards
 * isn't offered; with a single group there are no tabs at all. Used by the main screen and by "Copy to" / "Move to" */

enum { TAB_CARDS, TAB_GAMES, TAB_BOOT, TABS };
static const int tabText[TABS] = {T_TAB_CARDS, T_TAB_GAMES, T_TAB_BOOT};
#define TAB_Y      78     /* the tabs: over the list, on the line of the title on the right */
#define TAB_GAP    22
#define TAB_KEYS   (PAD_LEFT | PAD_RIGHT | PAD_L1 | PAD_R1)

typedef struct {
    int tab;                  /* the group shown */
    int count[TABS];          /* how many cards each group has */
    int n, idx[MAX_CARDS];    /* the shown group's cards (indexes in cards[]) */
    int cursor, top;          /* in the shown group */
    int keep[TABS][2];        /* each group's cursor and top: switching back lands on the same card */
    const card_t *skip;       /* a card left out (the one a save is copied from) */
} tabs_t;

static int tab_of(const card_t *c)
{
    return c->type == TYPE_GAMEID ? TAB_GAMES : c->type == TYPE_BOOT ? TAB_BOOT : TAB_CARDS;
}

/* shows a group, on the card it was left on (with ui_lock held) */
static void tabs_show(tabs_t *g, int tab)
{
    int i;
    g->keep[g->tab][0] = g->cursor;
    g->keep[g->tab][1] = g->top;
    memset(g->count, 0, sizeof(g->count));
    for (i = g->n = 0; i < nCards; i++) {
        if (&cards[i] == g->skip)
            continue;
        g->count[tab_of(&cards[i])]++;
        if (tab_of(&cards[i]) == tab)
            g->idx[g->n++] = i;
    }
    g->tab = tab;
    g->cursor = g->keep[tab][0] < g->n ? g->keep[tab][0] : 0;
    g->top = g->keep[tab][1];
    scroll_to(g->cursor, &g->top);
}

/* from scratch, on the first group that has cards; skip = a card to leave out (with ui_lock held) */
static void tabs_init(tabs_t *g, const card_t *skip)
{
    int k;
    memset(g, 0, sizeof(*g));
    g->skip = skip;
    tabs_show(g, TAB_CARDS);
    for (k = 0; k < TABS && !g->count[k]; k++)
        ;
    tabs_show(g, k < TABS ? k : TAB_CARDS);
}

/* the next group with cards to that side (dir -1 / +1); -1 = none */
static int tabs_next(const tabs_t *g, int dir)
{
    int k;
    for (k = g->tab + dir; k >= 0 && k < TABS; k += dir)
        if (g->count[k])
            return k;
    return -1;
}

static card_t *tabs_card(const tabs_t *g) { return g->n ? &cards[g->idx[g->cursor]] : NULL; }

/* up/down move the cursor, left/right change the group (with the sound). 1 = the press was one of those */
static int tabs_nav(tabs_t *g, u32 b)
{
    if (b & (PAD_UP | PAD_DOWN)) {
        if (g->n) {
            ui_lock();
            g->cursor = (b & PAD_UP) ? (g->cursor + g->n - 1) % g->n : (g->cursor + 1) % g->n;
            scroll_to(g->cursor, &g->top);
            ui_unlock();
            sound_play(SND_MOVE);
        }
        return 1;
    }
    if (b & TAB_KEYS) {
        int k = tabs_next(g, (b & (PAD_LEFT | PAD_L1)) ? -1 : 1);
        if (k >= 0) {
            ui_lock();
            tabs_show(g, k);
            ui_unlock();
            sound_play(SND_MOVE);
        }
        return 1;
    }
    return 0;
}

static const tabs_t *rowsOf;   /* the groups list_rows is drawing (its callbacks only get the row) */

static const char *tabs_text(int i, char *buf)
{
    (void)buf;
    return cards[rowsOf->idx[i]].base;
}

static u32 tabs_dot(int i) { return status_color(&cards[rowsOf->idx[i]]); }

/* the groups over the list (the one shown bright, in a soft light; the others dim; a small arrow on each side that
 * has another group) and the shown group's cards. dots = each card's status light */
static void tabs_draw(const tabs_t *g, int dots)
{
    int k, shown = 0, w = 0, x, lh = ui_line_height(FONT_SMALL);
    float cy = TAB_Y + lh / 2.0f + 1;
    for (k = 0; k < TABS; k++)
        if (g->count[k])
            w += (shown++ ? TAB_GAP : 0) + ui_measure(FONT_SMALL, T(tabText[k]));
    if (shown > 1) {
        x = LIST_X + LIST_W / 2 - w / 2;
        if (tabs_next(g, -1) >= 0)
            ui_triangle(x - 18, cy, x - 11, cy - 6, x - 11, cy + 6, 0x6E9AE0, 0x70);
        for (k = 0; k < TABS; k++) {
            int tw;
            if (!g->count[k])
                continue;
            tw = ui_measure(FONT_SMALL, T(tabText[k]));
            if (k == g->tab) {
                ui_light(x + tw / 2.0f, cy, tw / 2.0f + 26, lh * 0.8f, COLOR_GLOW, 0x30);
                ui_text(FONT_SMALL, x, TAB_Y, COLOR_ITEM_ON, T(tabText[k]));
            } else
                ui_text(FONT_SMALL, x, TAB_Y, 0x6F7C94, T(tabText[k]));
            x += tw + TAB_GAP;
        }
        if (tabs_next(g, 1) >= 0)
            ui_triangle(x - TAB_GAP + 18, cy, x - TAB_GAP + 11, cy + 6, x - TAB_GAP + 11, cy - 6, 0x6E9AE0, 0x70);
    }
    rowsOf = g;
    list_rows(g->n, g->cursor, g->top, tabs_text, dots ? tabs_dot : NULL);
}

/* -------- restore */

static const card_t *restoreCard;
static u64 lastRestoreDraw;
static int lastRestorePhase;

/* one screen and one bar for the whole restore, so the bar never goes back: downloading 30%, checking the download
 * 10%, writing 40%, reading it back 20%. The two steps the user cares about are the only ones named */
static int restore_progress(int phase, long long done, long long total)
{
    static const int start[] = {0, 300, 400, 800}, span[] = {300, 100, 400, 200};
    char t[200];
    if (phase < RESTORE_WRITE) {   /* once it starts writing over the card there's no cancelling */
        watch_cancel();
        if (cancelLatched) {
            if (confirm_cancel(T_RESTORE_CANCEL_TITLE, T_RESTORE_CANCEL_TEXT))
                return 1;
            lastRestoreDraw = 0;
        }
    }
    if (phase == lastRestorePhase && now_ms() - lastRestoreDraw < 100 && done < total)
        return 0;
    lastRestorePhase = phase;
    lastRestoreDraw = now_ms();
    if (done > total)
        done = total;
    snprintf(t, sizeof(t), T(T_RESTORING), restoreCard->base);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_DIM, 0, T(phase < RESTORE_WRITE ? T_RESTORE_STEP_DOWNLOAD : T_RESTORE_STEP_WRITE));
    dlg_bar(start[phase] + (total ? (int)(span[phase] * done / total) : 0), NULL);
    if (phase < RESTORE_WRITE)
        dlg_buttons(BUTTON_CIRCLE, T_CANCEL, 0, 0);
    dlg_show();
#ifdef TEST
    if (total && done * 2 >= total)
        test_capture_if(phase == RESTORE_WRITE ? 'W' : 'E');
#endif
    return 0;
}

/* is this card the one the sd2psx is emulating right now? 1 = yes, 0 = no, -1 = couldn't tell.
 * A CardN comes with its number. Card number 0 is the BootCard, a game card or a named folder, which the MMCE commands
 * don't tell apart: then it compares the root folder the PS2 sees in that slot with the one inside the .mcd (the same
 * saves with the same modification times = the same card) */
static int card_in_use(const card_t *c, int active, int channel)
{
    char seen[65], file[65];
    if (active == -2)
        return 0;   /* not on an MMCE device (testing on PCSX2) */
    if (active >= 1)
        return c->type == TYPE_NORMAL && atoi(c->folder + 4) == active && c->channel == channel;
    if (active == 0 && (c->type == TYPE_NORMAL || c->channel != channel))
        return 0;
    if (mc_root_signature(sdRoot[4] - '0', seen) != 0 || mcfs_root_signature(c->path, file) != 0)
        return active == 0 ? 1 : -1;   /* couldn't compare: on a special card, assume the worst */
    log_msg("restore: root of the card in the slot %.16s, of %s %.16s", seen, c->id, file);
    return strcmp(seen, file) == 0;
}

/* confirm, back up the current card if it changed, and restore. 1 = the card was restored */
static int restore_flow(card_t *c, const drive_file_t *f)
{
    char when[40], t[300];
    int active, channel = 0, inUse, unsure, r;
    backup_when(c, f, when, sizeof(when));
    for (;;) {
        active = mmce_active_card(&channel);
        inUse = card_in_use(c, active, channel);
        unsure = inUse < 0;
        if (unsure)
            inUse = 0;
        snprintf(t, sizeof(t), T(T_RESTORE_TITLE), c->base);
        dlg_new(COLOR_TITLE, t);
        snprintf(t, sizeof(t), T(T_RESTORE_FROM), when);
        dlg_line(FONT_TEXT, COLOR_ACCENT, 8, t);
        dlg_line(FONT_TEXT, COLOR_TEXT, 6, T(T_RESTORE_TEXT));
        if (c->status != ST_UP_TO_DATE)
            dlg_line(FONT_SMALL, COLOR_DIM, 6, T(T_RESTORE_SAVE_FIRST));
        if (inUse) {
            snprintf(t, sizeof(t), T(T_RESTORE_IN_USE), c->base);
            dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
        } else if (unsure)
            dlg_line(FONT_SMALL, COLOR_WARN, 0, T(T_RESTORE_UNKNOWN));
        dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_RESTORE);
        next.wide = 1;
        dlg_show();
        if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return 0;
        }
        sound_play(SND_CONFIRM);
        if (!inUse)
            break;
        log_msg("restore: %s is in use on the sd2psx, asking again", c->id);   /* X checks again */
    }
    /* first the current card, if it changed: nothing gets lost */
    if (c->status != ST_UP_TO_DATE) {
        restoring = 1;
        singleCard = c;
        run_backup(3, 0);
        restoring = 0;
        if (backupCancelled) {
            backupCancelled = 0;
            return 0;
        }
        if (failed) {
            summary_screen(0);
            return 0;
        }
    }
    restoreCard = c;
    lastRestoreDraw = 0;
    lastRestorePhase = -1;
    cancelLatched = 0;
    r = restore_card(c, f, restore_progress);
    if (r == -2)
        return 0;   /* cancelled before writing: the card didn't change */
    dlg_new(r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_RESTORE_OK : T_RESTORE_FAILED));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 4, c->base);
    if (r == 0)
        snprintf(t, sizeof(t), T(T_RESTORE_FROM), when);
    else
        snprintf(t, sizeof(t), "%s", googleError);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    sound_play(r == 0 ? SND_EXIT : SND_BACK);
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
    return r == 0;
}

/* -------- a card's backups on Drive: the list on the left, the card on the right */

static struct {
    card_t *card;
    drive_file_t *list;   /* oldest first (Drive's order); shown newest first */
    int n, cursor, top;
    icon_t *icon;
} hist;

static const char *hist_text(int i, char *buf)
{
    backup_when(hist.card, &hist.list[hist.n - 1 - i], buf, 40);
    return buf;
}

static void scene_history(float t)
{
    char s[200];
    const drive_file_t *f;
    card_state_t *e = state_card(hist.card->id, 0);
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    list_rows(hist.n, hist.cursor, hist.top, hist_text, NULL);
    snprintf(s, sizeof(s), T(T_HIST_TITLE), hist.card->base);
    ui_text_center(FONT_SMALL, CARD_CX, 84, 0x7E8AA0, s);
    draw_card_picture(hist.card, hist.icon, ui_clock());
    if (!hist.n) {
        ui_paragraph(FONT_TEXT, LIST_X + 30, ROW_Y0, 150, COLOR_DIM, T(T_HIST_EMPTY));
        {
            legend_t l = {BUTTON_CIRCLE, T(T_BACK)};
            look_legend(&l, 1, 0);
        }
        return;
    }
    f = &hist.list[hist.n - 1 - hist.cursor];
    format_size(f->size, s, sizeof(s));
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_TEXT, s);
    /* the same content as the card has now (and it hasn't changed since): restoring it changes nothing */
    if (e && f->sha_mcd[0] && !strcasecmp(f->sha_mcd, e->sha) && hist.card->status == ST_UP_TO_DATE)
        ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_OK, T(T_HIST_SAME));
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_RESTORE)}};
        look_legend(l, 2, 0);
    }
}

static void history_screen(card_t *c, icon_t *icon)
{
    static drive_file_t list[200];
    char folderId[80], t[300];
    int n, r;

    googleError[0] = 0;
    r = ensure_google(1);
    if (r < 0)
        return;
    if (r == 0) {
        message(0, NULL, COLOR_TEXT, T(T_HIST_LOADING));
        r = card_folder(c, folderId, sizeof(folderId)) != 0 ? T_ERR_INTERNET : 0;
    }
    n = r == 0 ? google_list(folderId, c->id, list, 200) : -1;
    if (r != 0 || n < 0) {
        snprintf(t, sizeof(t), "%s %s", T(r ? r : T_ERR_INTERNET), googleError);
        message_wait(0, NULL, COLOR_ERROR, t);
        return;
    }
    log_msg("%s: %d backup(s) on Drive", c->id, n);
    ui_lock();
    hist.card = c;
    hist.list = list;
    hist.n = n;
    hist.cursor = hist.top = 0;
    hist.icon = icon;
    ui_unlock();
    ui_scene(scene_history);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (!n || (b & PAD_CIRCLE)) {
            sound_play(SND_BACK);
            return;
        }
        if (b & (PAD_UP | PAD_DOWN)) {
            ui_lock();
            hist.cursor = (b & PAD_UP) ? (hist.cursor > 0 ? hist.cursor - 1 : n - 1) : (hist.cursor < n - 1 ? hist.cursor + 1 : 0);
            scroll_to(hist.cursor, &hist.top);
            ui_unlock();
            sound_play(SND_MOVE);
        } else if (b & PAD_CROSS) {
            sound_play(SND_CONFIRM);
            restore_flow(c, &list[n - 1 - hist.cursor]);
            return;   /* the list may have changed (the current card went to Drive first) */
        }
    }
}

/* -------- one card: its saves, the PS2 browser's way */

#define GRID_COLS 5
#define GRID_ROWS 4
typedef struct {
    mcfs_save_t s;
    icon_t *icon;
    int tried;             /* the icon was read (or failed) */
    char line1[72], line2[72];
} save_view_t;

static struct {
    card_t *card;
    save_view_t *saves;
    int n, cursor, top;    /* top = the first row on screen */
    long long freeBytes;
    u64 since;             /* when the cursor last moved (the selected icon starts turning from the front) */
} brw;

static void grid_cell(int i, float height, float *cx, float *cy)
{
    int k = i - brw.top * GRID_COLS;
    icon_cell_point(k % GRID_COLS, k / GRID_COLS, height, cx, cy);
}

/* the icons of the rows on screen; spin = the selected one turns */
static void browser_icons(int spin)
{
    int i, first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    for (i = first; i < brw.n && i < last; i++) {
        float cx, cy;
        save_view_t *v = &brw.saves[i];
        if (v->icon) {
            int k = i - first;
            icon_draw_cell(v->icon, k % GRID_COLS, k / GRID_COLS, spin && i == brw.cursor ? (now_ms() - brw.since) / 1000.0f : -1);
        } else if (v->tried) {   /* a save without an icon: a plain grey block */
            grid_cell(i, 2.5f, &cx, &cy);
            ui_rect(cx - 16, cy - 16, 32, 32, 0x505056, 0x60);
        }
    }
}

static void scene_browser(float t)
{
    char s[120];
    int first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    look_space();   /* the browser's layout, over the same space as the other screens */
    ui_alpha(look_fade(t));
    /* the card at the top left, its free space below */
    ui_image(IMG_MINICARD, 62, 38, 24, 28, 0xFFFFFF, 0x80);
    ui_text_shadow(FONT_BROWSER, 98, 32, 0xF4F4F4, brw.card->base);
    if (brw.freeBytes >= 0) {
        snprintf(s, sizeof(s), T(T_FREE_KB), (int)(brw.freeBytes / 1024));
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, s);
    }
    if (!brw.n)
        ui_text_center(FONT_BROWSER, W / 2.0f, 200, 0xF0F0F0, T(T_CARD_EMPTY));
    /* the selected save: a white light behind it, it turns; its name in yellow at the top right */
    if (brw.n) {
        float cx, cy;
        save_view_t *v = &brw.saves[brw.cursor];
        grid_cell(brw.cursor, 0.6f, &cx, &cy);   /* like the browser: a light at the selected icon's base */
        look_halo(cx, cy, 46, 0x34);
        if (v->line1[0] || v->line2[0]) {
            if (v->line1[0])
                ui_text_right(FONT_BROWSER, 596, 32, 0xE6E640, v->line1);
            if (v->line2[0])
                ui_text_right(FONT_BROWSER, 596, 32 + ui_line_height(FONT_BROWSER), 0xE6E640, v->line2);
        } else if (v->tried)
            ui_text_right(FONT_BROWSER, 596, 32, 0xE6E640, v->s.folder);
    }
    browser_icons(1);
    if (brw.top > 0)
        look_arrow(W / 2.0f, 92, 0, 0x3A5AE0);
    if (last < brw.n)
        look_arrow(W / 2.0f, 340, 1, 0x3A5AE0);
    {
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_SQUARE, T(T_SYNC)}};
        look_legend(l, 3, 0);
    }
}

/* reads the icon of the first save on screen that wasn't read yet; 0 = there was none */
static int load_next_icon(void)
{
    int i, first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    /* the selected one first */
    i = brw.saves[brw.cursor].tried || !brw.n ? -1 : brw.cursor;
    for (i = i >= 0 ? i : first; i >= 0 && i < brw.n && i < last && brw.saves[i].tried; i++)
        ;
    if (i < 0 || i >= brw.n || i >= last)
        return 0;
    {
        buffer_t iconsys = {0}, ico = {0};
        icon_t *ic = NULL;
        if (mcfs_save_icon(brw.card->path, &brw.saves[i].s, &iconsys, &ico) == 0)
            ic = icon_load(&iconsys, &ico);
        buf_free(&iconsys);
        buf_free(&ico);
        ui_lock();
        brw.saves[i].icon = ic;
        brw.saves[i].tried = 1;
        if (ic) {
            snprintf(brw.saves[i].line1, sizeof(brw.saves[i].line1), "%s", ic->line1);
            snprintf(brw.saves[i].line2, sizeof(brw.saves[i].line2), "%s", ic->line2);
        }
        ui_unlock();
    }
    return 1;
}

/* only the icons of the rows on screen (and one row around) stay in memory */
static void drop_far_icons(void)
{
    int i, lo = (brw.top - 1) * GRID_COLS, hi = (brw.top + GRID_ROWS + 1) * GRID_COLS;
    ui_lock();
    for (i = 0; i < brw.n; i++)
        if ((i < lo || i >= hi) && brw.saves[i].icon) {
            icon_free(brw.saves[i].icon);
            brw.saves[i].icon = NULL;
            brw.saves[i].tried = 0;
        }
    ui_unlock();
}

static void browser_close(void)
{
    int i;
    ui_scene(scene_frame);   /* off the screen before the icons go */
    ui_lock();
    for (i = 0; i < brw.n; i++)
        icon_free(brw.saves[i].icon);
    brw.n = 0;
    ui_unlock();
}

/* the newest save's icon, for the big card (history) */
static icon_t *newest_icon(const card_t *c)
{
    buffer_t iconsys = {0}, ico = {0};
    char folder[33];
    icon_t *ic = NULL;
    if (c->type == TYPE_GAMEID && mcfs_newest_save_icon(c->path, folder, &iconsys, &ico) == 0)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    return ic;
}

/* -------- a list to pick from, in a box (the card's menu, the card to copy a save to) */

#define PICK_MAX 128
#define PICK_ROWS 6
static struct {
    char title[100];
    const char *items[PICK_MAX];
    int n, cursor, top;
} pick;

static void scene_pick(float t)
{
    int rows = pick.n < PICK_ROWS ? pick.n : PICK_ROWS, w = 380, h = 56 + rows * 32 + 24, x = (W - w) / 2,
        y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, w, h);
    look_title(W / 2.0f, y + 20, pick.title, 1);
    for (i = pick.top; i < pick.n && i < pick.top + rows; i++)
        look_item(W / 2.0f, y + 64 + (i - pick.top) * 32, pick.items[i], i == pick.cursor, 1);
    if (pick.top > 0)
        look_arrow(x + w - 26, y + 60, 0, 0x6E9AE0);
    if (pick.top + rows < pick.n)
        look_arrow(x + w - 26, y + h - 26, 1, 0x6E9AE0);
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
}

/* the index picked, or -1 (circle). start = the item the cursor starts on (the current value) */
static int choose(const char *title, const char *const *items, int n, int start)
{
    int i;
    if (n > PICK_MAX)
        n = PICK_MAX;
    ui_lock();
    snprintf(pick.title, sizeof(pick.title), "%s", title);
    for (i = 0; i < n; i++)
        pick.items[i] = items[i];
    pick.n = n;
    pick.cursor = start >= 0 && start < n ? start : 0;
    pick.top = pick.cursor >= PICK_ROWS ? pick.cursor - PICK_ROWS + 1 : 0;
    ui_unlock();
    ui_scene(scene_pick);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return -1;
        }
        if (b & PAD_CROSS) {
            sound_play(SND_CONFIRM);
            return pick.cursor;
        }
        ui_lock();
        pick.cursor = (b & PAD_UP) ? (pick.cursor + n - 1) % n : (pick.cursor + 1) % n;
        if (pick.cursor < pick.top)
            pick.top = pick.cursor;
        if (pick.cursor >= pick.top + PICK_ROWS)
            pick.top = pick.cursor - PICK_ROWS + 1;
        ui_unlock();
        sound_play(SND_MOVE);
    }
}

/* -------- a save's page, like the PS2 browser's: the icon big on the left; the card, the name, the date and the
 * size on the right, and what can be done with it */

static const int saveOptions[] = {T_COPY, T_MOVE, T_DELETE, T_TO_CLOUD};
#define SAVE_OPTIONS 4
static struct {
    save_view_t *v;
    char date[40], size[40];
    int cursor;
    u64 since;
} sp;

static void text_center_shadow(int font, float cx, float y, u32 color, const char *s)
{
    ui_text_shadow(font, (int)(cx - ui_measure(font, s) / 2), y, color, s);
}

static void scene_save(float t)
{
    save_view_t *v = sp.v;
    const float cx = 452;
    float y = 70;
    int i, lh = ui_line_height(FONT_BROWSER);
    look_space();
    browser_icons(0);   /* the card's saves stay behind, faded, like in the browser */
    ui_rect(0, 0, W, H, 0x02040C, 0x9C);
    ui_alpha(look_fade(t));
    ui_light(190, 318, 110, 34, 0xFFFFFF, 0x26);
    if (v->icon)
        icon_draw(v->icon, 190, 232, 214, (now_ms() - sp.since) / 1000.0f);
    text_center_shadow(FONT_TEXT, cx, y, 0xE8E8EC, brw.card->base);
    y += 26;
    if (v->line1[0])
        text_center_shadow(FONT_BROWSER, cx, y, 0xE6E640, v->line1), y += lh;
    if (v->line2[0])
        text_center_shadow(FONT_BROWSER, cx, y, 0xE6E640, v->line2), y += lh;
    if (!v->line1[0] && !v->line2[0])
        text_center_shadow(FONT_BROWSER, cx, y, 0xE6E640, v->s.folder), y += lh;
    y += 6;
    text_center_shadow(FONT_TEXT, cx, y, 0xE4E4E8, sp.date);
    text_center_shadow(FONT_TEXT, cx, y + 23, 0xE4E4E8, sp.size);
    for (i = 0, y = 232; i < SAVE_OPTIONS; i++, y += 31) {
        const char *s = T(saveOptions[i]);
        float x = (int)(cx - ui_measure(FONT_BROWSER, s) / 2);
        if (i == sp.cursor)
            look_glow_text(FONT_BROWSER, x, y, 0x3A88C8, 0x7AD8FF, s);
        else
            ui_text_shadow(FONT_BROWSER, x, y, 0x8C929C, s);
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
}

/* the sd2psx keeps the card it's using in its own memory and writes it back by itself: never change that card's
 * .mcd. 1 = it's in use (or that couldn't be told), and the user was told */
static int card_busy(const card_t *c)
{
    char t[300];
    int channel = 0, active = mmce_active_card(&channel), r = card_in_use(c, active, channel);
    if (r == 0)
        return 0;
    snprintf(t, sizeof(t), T(r > 0 ? T_CARD_IN_USE : T_RESTORE_UNKNOWN), c->base);
    message_wait(0, NULL, COLOR_WARN, t);
    return 1;
}

static void op_result(int r, int okText, const card_t *other)
{
    char t[300];
    if (r == MCFS_OK)
        snprintf(t, sizeof(t), T(okText), other ? other->base : "");
    else
        snprintf(t, sizeof(t), T(r == MCFS_ERR_EXISTS ? T_ERR_EXISTS : r == MCFS_ERR_FULL ? T_ERR_FULL
                                 : r == MCFS_ERR_CHECK ? T_ERR_MC_CHECK : T_ERR_MC_WRITE), other ? other->base : "");
    message_wait(0, NULL, r == MCFS_OK ? COLOR_OK : COLOR_ERROR, t);
}

/* the save's name as the questions show it: its icon.sys title, or the folder when it has none */
static void save_name(const save_view_t *v, char *out, size_t size)
{
    if (v->line1[0] || v->line2[0])
        snprintf(out, size, "%s%s%s", v->line1, v->line1[0] && v->line2[0] ? " " : "", v->line2);
    else
        snprintf(out, size, "%s", v->s.folder);
}

/* -------- where to copy or move a save to: the cards in the same groups as the main screen (without the card the save
 * is on), the destination big on the right with its free space. A card without room for the save can't be chosen */

static struct {
    tabs_t g;
    const char *title;
    long long need;                   /* the save's size */
    long long freeBytes[MAX_CARDS];   /* each card's free space, once read (-2 = not yet, -1 = unknown) */
    icon_t *icon;                     /* the selected game card's icon, once read */
    int iconCard;                     /* which card that icon is of (-1 = none) */
} dst;

static void scene_dest(float t)
{
    const card_t *c;
    long long f;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    tabs_draw(&dst.g, 0);
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
    look_title(CARD_CX, 82, dst.title, 1);
    if (!(c = tabs_card(&dst.g)))
        return;
    draw_card_picture(c, dst.iconCard == dst.g.idx[dst.g.cursor] ? dst.icon : NULL, ui_clock());
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_ITEM_ON, c->base);
    if ((f = dst.freeBytes[dst.g.idx[dst.g.cursor]]) >= 0) {
        char l[64];
        if (f < dst.need)
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_WARN, T(T_NO_ROOM));
        else {
            snprintf(l, sizeof(l), T(T_FREE_KB), (int)(f / 1024));
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_DIM, l);
        }
    }
}

/* the selected card's free space and, for a game card, its icon: read when the cursor rests a moment */
static void dest_info(void)
{
    card_t *c = tabs_card(&dst.g);
    icon_t *ic;
    int k;
    if (!c)
        return;
    k = dst.g.idx[dst.g.cursor];
    if (dst.freeBytes[k] == -2) {
        long long f = -1;
        if (mcfs_list_saves(c->path, NULL, 0, &f) < 0)
            f = -1;
        ui_lock();
        dst.freeBytes[k] = f;
        ui_unlock();
    }
    if (dst.iconCard == k)
        return;
    ic = newest_icon(c);
    ui_lock();
    icon_free(dst.icon);
    dst.icon = ic;
    dst.iconCard = k;
    ui_unlock();
}

/* the card picked (NULL = circle). need = the save's size */
static card_t *choose_dest(const card_t *from, int move, long long need)
{
    card_t *c = NULL;
    int i;
    ui_lock();
    tabs_init(&dst.g, from);
    dst.title = T(move ? T_MOVE_TO : T_COPY_TO);
    dst.need = need;
    for (i = 0; i < MAX_CARDS; i++)
        dst.freeBytes[i] = -2;
    dst.icon = NULL;
    dst.iconCard = -1;
    ui_unlock();
    ui_scene(scene_dest);
    for (;;) {
        u32 keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE, b = wait_nav_ms(keys, 250);
        if (!b) {
            dest_info();
            b = wait_nav(keys);
        }
        if (tabs_nav(&dst.g, b))
            continue;
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            break;
        }
        if ((b & PAD_CROSS) && dst.g.n) {
            long long f = dst.freeBytes[dst.g.idx[dst.g.cursor]];
            if (f >= 0 && f < dst.need) {   /* no room: the warning is already on screen */
                sound_play(SND_BACK);
                continue;
            }
            sound_play(SND_CONFIRM);
            c = tabs_card(&dst.g);
            break;
        }
    }
    ui_lock();
    icon_free(dst.icon);
    dst.icon = NULL;
    dst.iconCard = -1;
    ui_unlock();
    return c;
}

/* copy or move the save to another card. 1 = the card it was on changed (moved) */
static int save_transfer(card_t *c, save_view_t *v, int move)
{
    card_t *to;
    long long bytes = 0;
    int files = 0, r;
    if (nCards < 2) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_OTHER_CARDS));
        return 0;
    }
    mcfs_save_info(c->path, v->s.folder, &bytes, &files);
    if (!(to = choose_dest(c, move, bytes)))
        return 0;
    {
        char name[100], t[200];
        save_name(v, name, sizeof(name));
        snprintf(t, sizeof(t), T(move ? T_CONFIRM_MOVE : T_CONFIRM_COPY), name, to->base);
        if (!confirm(t, NULL, move ? T_MOVE : T_COPY))
            return 0;
    }
    if (card_busy(to) || (move && card_busy(c)))
        return 0;
    message(0, NULL, COLOR_TEXT, T(move ? T_WORKING_MOVE : T_WORKING_COPY));
    r = mcfs_copy_save(c->path, v->s.folder, to->path);
    if (r == MCFS_OK && move)
        r = mcfs_delete_save(c->path, v->s.folder);
    log_msg("%s %s from %s to %s: %d", move ? "move" : "copy", v->s.folder, c->id, to->id, r);
    cards_recheck(to);
    if (move)
        cards_recheck(c);
    op_result(r, move ? T_DONE_MOVE : T_DONE_COPY, to);
    return r == MCFS_OK && move;
}

/* 1 = deleted */
static int save_delete(card_t *c, save_view_t *v)
{
    char name[100], t[300];
    int r;
    if (card_busy(c))
        return 0;
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_DELETE_ASK), name);
    dlg_new(COLOR_WARN, t);
    snprintf(t, sizeof(t), T(T_DELETE_TEXT), c->base);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_DELETE);
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    message(0, NULL, COLOR_TEXT, T(T_WORKING_DELETE));
    r = mcfs_delete_save(c->path, v->s.folder);
    log_msg("delete %s from %s: %d", v->s.folder, c->id, r);
    cards_recheck(c);
    op_result(r, T_DONE_DELETE, NULL);
    return r == MCFS_OK;
}

/* just this save to Drive, as a .psu: "<main folder>/<card folder>/Saves/<save> YYYY-MM-DD HHhMM.psu" */
static void save_to_cloud(card_t *c, save_view_t *v)
{
    buffer_t psu = {0};
    char folderId[80], savesId[80] = "", name[160], desc[200], id[80], t[400];
    const char *cache;
    char key[200];
    datetime_t when;
    int r;
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_CONFIRM_CLOUD), name);
    if (!confirm(t, T(T_CONFIRM_CLOUD_TEXT), T_TO_CLOUD))
        return;
    googleError[0] = 0;
    if ((r = ensure_google(1)) != 0) {
        if (r > 0) {
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(0, NULL, COLOR_ERROR, t);
        }
        return;
    }
    /* the same screen as a card's backup, with the save's icon and name */
    ui_lock();
    current = c;
    saveIcon = v->icon;
    snprintf(saveTitle, sizeof(saveTitle), "%s", name);
    iconStart = now_ms();
    currentN = totalN = 1;
    ui_unlock();
    upload_screen(0, 1);
    backupCancelled = cancelLatched = 0;
    rootId[0] = 0;
    r = -1;
    if (mcfs_export_psu(c->path, v->s.folder, &psu) == MCFS_OK && card_folder(c, folderId, sizeof(folderId)) == 0) {
        snprintf(key, sizeof(key), "%s/%s/Saves", cfg.drive_folder, c->folder);
        if ((cache = state_folder(key)) != NULL)
            snprintf(savesId, sizeof(savesId), "%s", cache);
        if (google_folder("Saves", folderId, savesId, sizeof(savesId)) == 0) {
            state_set_folder(key, savesId);
            local_time(&when);
            snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02d.psu", v->s.folder, when.year, when.month, when.day, when.hour,
                     when.minute);
            snprintf(desc, sizeof(desc), "%s%s%s (%s) - " APP_NAME " " APP_VERSION, v->line1, v->line2[0] ? " " : "", v->line2, c->base);
            r = google_upload_buffer(savesId, name, desc, psu.data, psu.len, on_progress, id, sizeof(id));
            state_write();
        }
    }
    log_msg("save %s of %s to Drive (%u bytes): %d %s", v->s.folder, c->id, (unsigned)psu.len, r, googleError);
    buf_free(&psu);
    ui_lock();
    saveIcon = NULL;
    ui_unlock();
    if (backupCancelled) {   /* circle, and "yes, cancel": back to the save's page, nothing to say */
        backupCancelled = 0;
        return;
    }
    if (r == 0)
        message_wait(0, NULL, COLOR_OK, T(T_CLOUD_DONE));
    else {
        snprintf(t, sizeof(t), "%s %s", T(T_CLOUD_FAILED), googleError);
        message_wait(0, NULL, COLOR_ERROR, t);
    }
}

/* 1 = the card changed (the list has to be read again) */
static int save_screen(card_t *c, int i)
{
    save_view_t *v = &brw.saves[i];
    unsigned long long w = v->s.when;
    long long bytes = 0;
    int files = 0;
    datetime_t d;
    mcfs_save_info(c->path, v->s.folder, &bytes, &files);
    card_time_local((int)(w >> 40), (w >> 32) & 0xFF, (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF, &d);
    ui_lock();
    sp.v = v;
    if (i18n_is_pt())
        snprintf(sp.date, sizeof(sp.date), "%02d/%02d/%04d  %02d:%02d:%02d", d.day, d.month, d.year, d.hour, d.minute, d.second);
    else
        snprintf(sp.date, sizeof(sp.date), "%04d-%02d-%02d  %02d:%02d:%02d", d.year, d.month, d.day, d.hour, d.minute, d.second);
    snprintf(sp.size, sizeof(sp.size), T(T_SAVE_KB), (int)((bytes + 1023) / 1024));
    sp.cursor = 0;
    sp.since = now_ms();
    ui_unlock();
    for (;;) {
        u32 b;
        ui_scene(scene_save);
        b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return 0;
        }
        if (b & (PAD_UP | PAD_DOWN)) {
            ui_lock();
            sp.cursor = (b & PAD_UP) ? (sp.cursor + SAVE_OPTIONS - 1) % SAVE_OPTIONS : (sp.cursor + 1) % SAVE_OPTIONS;
            ui_unlock();
            sound_play(SND_MOVE);
            continue;
        }
        sound_play(SND_CONFIRM);
        switch (saveOptions[sp.cursor]) {
        case T_COPY:
            save_transfer(c, v, 0);
            break;
        case T_MOVE:
            if (save_transfer(c, v, 1))
                return 1;
            break;
        case T_DELETE:
            if (save_delete(c, v))
                return 1;
            break;
        default:
            save_to_cloud(c, v);
            break;
        }
    }
}

/* -------- syncing one card (□ in its saves, "Sync now" in its options): asked first; a card that is already synced
 * says so and can be synced again (a new backup on Drive). 1 = it ran */

static int sync_card(card_t *c)
{
    char t[200];
    if (c->status == ST_UP_TO_DATE) {
        snprintf(t, sizeof(t), T(T_ALREADY_SYNCED), c->base);
        if (!confirm(t, T(T_SYNC_AGAIN_ONE), T_SYNC))
            return 0;
    } else {
        snprintf(t, sizeof(t), T(T_CONFIRM_BACKUP_ONE), c->base);
        if (!confirm(t, T(T_CONFIRM_BACKUP_TEXT), T_SYNC))
            return 0;
    }
    singleCard = c;
    manual_backup(3);
    return 1;
}

/* -------- one card's saves, the PS2 browser's way */

static void browser_load(card_t *c, int cursor)
{
    static save_view_t *saves;
    static mcfs_save_t list[MCFS_MAX_SAVES];
    long long freeBytes = -1;
    int n, i;
    if (!saves && !(saves = calloc(MCFS_MAX_SAVES, sizeof(save_view_t))))
        return;
    n = mcfs_list_saves(c->path, list, MCFS_MAX_SAVES, &freeBytes);
    if (n < 0)
        n = 0;
    ui_lock();
    for (i = 0; i < brw.n; i++)   /* a list read again (after a move or a delete): the old icons go */
        icon_free(brw.saves ? brw.saves[i].icon : NULL);
    memset(saves, 0, sizeof(save_view_t) * MCFS_MAX_SAVES);
    for (i = 0; i < n; i++)
        saves[i].s = list[i];
    brw.card = c;
    brw.saves = saves;
    brw.n = n;
    brw.cursor = cursor < n ? cursor : n > 0 ? n - 1 : 0;
    brw.top = 0;
    while (brw.cursor / GRID_COLS >= brw.top + GRID_ROWS)
        brw.top++;
    brw.freeBytes = freeBytes;
    brw.since = now_ms();
    ui_unlock();
    log_msg("%s: %d saves, %lld bytes free", c->id, n, freeBytes);
}

static void card_screen(card_t *c)
{
    brw.n = 0;
    browser_load(c, 0);
    ui_scene(scene_browser);
    for (;;) {
        /* reads the icons one by one while nobody presses anything */
        int pending = 1, n = brw.n;
        u32 b;
        while (pending && !(b = wait_nav_ms(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE | PAD_SQUARE, 0)))
            pending = load_next_icon();
        if (!pending)
            b = wait_nav(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE | PAD_SQUARE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            browser_close();
            return;
        }
        if (b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
            int k = brw.cursor;
            if (b & PAD_LEFT)
                k = k > 0 ? k - 1 : k;
            else if (b & PAD_RIGHT)
                k = k < n - 1 ? k + 1 : k;
            else if (b & PAD_UP)
                k = k >= GRID_COLS ? k - GRID_COLS : k;
            else if (b & PAD_DOWN)
                k = k + GRID_COLS < n ? k + GRID_COLS : (k / GRID_COLS < (n - 1) / GRID_COLS ? n - 1 : k);
            if (k != brw.cursor) {
                ui_lock();
                brw.cursor = k;
                brw.since = now_ms();
                if (k / GRID_COLS < brw.top)
                    brw.top = k / GRID_COLS;
                if (k / GRID_COLS >= brw.top + GRID_ROWS)
                    brw.top = k / GRID_COLS - GRID_ROWS + 1;
                ui_unlock();
                drop_far_icons();
                sound_play(SND_MOVE);
            }
        } else if ((b & PAD_CROSS) && n) {
            sound_play(SND_CONFIRM);
            if (save_screen(c, brw.cursor))
                browser_load(c, brw.cursor);
            ui_scene(scene_browser);
        } else if (b & PAD_SQUARE) {
            sound_play(SND_CONFIRM);
            sync_card(c);
            ui_scene(scene_browser);
        }
    }
}

/* -------- △ on a card: what can be done with it as a whole */

static void card_options(card_t *c)
{
    static const char *items[2];
    int k = 0;
    for (;;) {   /* circle in what comes next comes back here; circle here goes back to the main screen */
        items[0] = T(T_SYNC_NOW);
        items[1] = T(T_RESTORE_BACKUP);
        if ((k = choose(c->base, items, 2, k)) < 0)
            return;
        if (k == 0) {
            if (sync_card(c))
                return;   /* synced: back to the main screen, where its new status shows */
        } else {
            icon_t *ic = newest_icon(c);
            history_screen(c, ic);
            ui_scene(scene_frame);
            ui_lock();
            icon_free(ic);
            ui_unlock();
        }
    }
}

/* -------- the main screen: the cards on the left, in their groups (tabs), the selected one big on the right */

static struct {
    tabs_t g;
    icon_t *icon;             /* the 3D icon of the selected game card, once read */
    int iconCard;             /* which card that icon is of (index in cards[], -1 = none) */
} menu = {.iconCard = -1};

static void scene_menu(float t)
{
    char s[64];
    const card_t *c;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    tabs_draw(&menu.g, 1);
    {
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_MENU_EXIT)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_TRIANGLE, T(T_OPTIONS)},
                         {BUTTON_START, T(T_SETTINGS)}};
        look_legend(l, 4, 1);
    }
    if (!(c = tabs_card(&menu.g))) {
        ui_paragraph(FONT_TEXT, LIST_X + 30, ROW_Y0, 150, COLOR_WARN, T(T_NO_CARDS));
        return;
    }
    ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, c->name[0] ? c->name : c->base);
    draw_card_picture(c, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, status_color(c), status_text(c));
    last_backup(c, s, sizeof(s));
    if (s[0]) {
        char l[96];
        snprintf(l, sizeof(l), T(T_CARD_LAST), s);
        ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_DIM, l);
    }
}

/* the selected game card's icon, for its big card (read after the cursor rests on it a moment) */
static void menu_icon(void)
{
    icon_t *ic = NULL;
    card_t *c = tabs_card(&menu.g);
    if (!c || menu.iconCard == menu.g.idx[menu.g.cursor])
        return;
    if (c->type == TYPE_GAMEID)
        ic = newest_icon(c);
    ui_lock();
    icon_free(menu.icon);
    menu.icon = ic;
    menu.iconCard = menu.g.idx[menu.g.cursor];
    ui_unlock();
}

static void offer_auto_sync(void);
static void helper_install_now(int doneTitle);

/* -------- settings (START): syncing every card, the IGR helper, where to go after IGR, the language, how many backups
 * to keep, the update check, the Google account, about. Each change goes to sd2cloud.ini at once */

enum { SET_SYNC_ALL, SET_HELPER, SET_IGR_RETURN, SET_LANGUAGE, SET_KEEP, SET_UPDATES, SET_ACCOUNT, SET_ABOUT,
       SET_MAX };
#define SET_X      80     /* the labels; the values end at SET_RIGHT */
#define SET_RIGHT  560
#define SET_Y0     122
#define SET_ROW    24
static struct {
    int n, cursor, item[SET_MAX];
    char label[SET_MAX][96], value[SET_MAX][64];
} st;

/* the programs in the microSD's APPS (a folder with a title.cfg, as OPL's Apps tab shows them): where SD2Cloud can
 * go after IGR or when leaving. Listed once (appsListed) */
#define MAX_APPS 32
typedef struct {
    char title[64], path[200];
} app_t;
static app_t apps[MAX_APPS];
static int nApps, appsListed;

static void on_app_cfg(const char *s, const char *k, const char *v, void *u)
{
    app_t *a = u;
    (void)s;
    if (!strcasecmp(k, "title"))
        snprintf(a->title, sizeof(a->title), "%s", v);
    else if (!strcasecmp(k, "boot"))
        snprintf(a->path, sizeof(a->path), "%s", v);
}

static void list_apps(void)
{
    static char dirs[MAX_APPS][64];
    char base[64], c[260];
    DIR *d;
    struct dirent *e;
    int n = 0, i;
    nApps = 0;
    snprintf(base, sizeof(base), "%sAPPS", sdRoot);
    if ((d = opendir(base)) != NULL) {   /* read the whole list and close it before opening anything else (sd2psx) */
        while ((e = readdir(d)) != NULL && n < MAX_APPS)
            if (e->d_name[0] != '.' && strcasecmp(e->d_name, "SD2Cloud"))
                snprintf(dirs[n++], sizeof(dirs[0]), "%s", e->d_name);
        closedir(d);
    }
    for (i = 0; i < n; i++) {
        app_t *a = &apps[nApps];
        memset(a, 0, sizeof(*a));
        snprintf(c, sizeof(c), "%s/%s/title.cfg", base, dirs[i]);
        if (ini_read(c, on_app_cfg, a) != 0 || !a->path[0] || strchr(a->path, '/'))
            continue;
        snprintf(c, sizeof(c), "%s/%s/%s", base, dirs[i], a->path);
        if (!file_exists(c))
            continue;
        snprintf(a->path, sizeof(a->path), "%s", c);
        if (!a->title[0])
            snprintf(a->title, sizeof(a->title), "%s", dirs[i]);
        nApps++;
    }
    appsListed = 1;
    log_msg("%d programs in APPS", nApps);
}

/* a path as the .ini keeps it ("mmce?:" = the microSD's slot, which can change) and as it is on this console */
static void target_for_ini(const char *path, char *out, size_t size)
{
    snprintf(out, size, "%s", path);
    if (!strncmp(out, "mmce", 4) && out[4] && out[5] == ':')
        out[4] = '?';
}

static void target_on_sd(const char *target, char *out, size_t size)
{
    char *p;
    snprintf(out, size, "%s", target);
    if ((p = strstr(out, "mmce?:")) != NULL)
        p[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
}

/* the name the user gave in sd2cloud.ini ("name", under [manual] or [igr]) to the program that section's "return"
 * points to, when target is that program; NULL = none */
static const char *given_name(const char *target)
{
    char q[260], w[260];
    target_on_sd(target, q, sizeof(q));
    target_on_sd(cfg.manual_return, w, sizeof(w));
    if (cfg.manual_name[0] && !strcasecmp(q, w))
        return cfg.manual_name;
    target_on_sd(cfg.igr_return, w, sizeof(w));
    if (cfg.igr_name[0] && !strcasecmp(q, w))
        return cfg.igr_name;
    return NULL;
}

/* what the settings show for a target: the name the user gave it, the program's title, "Automatic" or the PS2 menu */
static void target_name(const char *target, char *out, size_t size)
{
    char q[260], *p, *s;
    int i;
    if (!strcasecmp(target, "auto") || !strcasecmp(target, "osd")) {
        snprintf(out, size, "%s", T(!strcasecmp(target, "auto") ? T_AUTO : T_RET_OSD));
        return;
    }
    if (given_name(target)) {
        snprintf(out, size, "%s", given_name(target));
        return;
    }
    target_on_sd(target, q, sizeof(q));
    for (i = 0; i < nApps; i++)
        if (!strcasecmp(apps[i].path, q)) {
            snprintf(out, size, "%s", apps[i].title);
            return;
        }
    /* a path written by hand: its folder in APPS, or the file; on another device, which one */
    if ((p = strcasestr(q, "APPS/")) != NULL && (s = strchr(p + 5, '/')) != NULL) {
        *s = 0;
        snprintf(out, size, "%s", p + 5);
    } else
        snprintf(out, size, "%s", (p = strrchr(q, '/')) != NULL ? p + 1 : (p = strrchr(q, ':')) != NULL ? p + 1 : q);
    i = device_of(target);
    if (i != DEV_SD) {
        size_t n = strlen(out);
        snprintf(out + n, size - n, " (%s)", i == DEV_MC ? T(T_DEV_MC) : i == DEV_USB ? "USB" : i == DEV_MX4SIO ? "MX4SIO" : T(T_DEV_HDD));
    }
}

/* shortens a text with "..." until it fits maxw (with ui_lock held: it measures with the fonts) */
static void fit(int font, char *s, size_t size, int maxw)
{
    size_t n = strlen(s);
    if (ui_measure(font, s) <= maxw)
        return;
    if (n > size - 4)
        n = size - 4;
    while (n > 0) {
        do
            n--;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80);   /* not in the middle of a UTF-8 letter */
        strcpy(s + n, "...");
        if (ui_measure(font, s) <= maxw)
            return;
    }
}

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
    set_item(&i, SET_HELPER, T(T_HELPER_TITLE), T(helperState == HELPER_SAME ? T_HELPER_INSTALLED
                                                : helperState == HELPER_DIFFERENT ? T_HELPER_OUTDATED
                                                : helperState == HELPER_NOT_INSTALLED ? T_HELPER_NOT_INSTALLED : T_HELPER_NA));
    target_name(cfg.igr_return, v, sizeof(v));
    set_item(&i, SET_IGR_RETURN, T(T_SET_IGR_RETURN), v);
    set_item(&i, SET_LANGUAGE, T(T_SET_LANGUAGE), !strcmp(cfg.language, "pt") ? "Português" : !strcmp(cfg.language, "en") ? "English"
                                                                                    : T(T_AUTO));
    snprintf(v, sizeof(v), "%d", cfg.keep);
    set_item(&i, SET_KEEP, T(T_SET_KEEP), cfg.keep ? v : T(T_KEEP_ALL));
    if (updateAvailable)
        snprintf(v, sizeof(v), T(T_UPDATE_AVAILABLE), update_tag());
    set_item(&i, SET_UPDATES, T(T_SET_UPDATES), updateAvailable ? v : "");
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
        float y = SET_Y0 + i * SET_ROW;
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
    if (r == -2)
        return T(T_HELPER_MISSING);
    if (r == -3) {
        snprintf(t, sizeof(t), T(T_HELPER_FULL), helperNeedKb, helperFreeKb);
        return t;
    }
    return T(T_HELPER_ERROR);
}

/* SD2Cloud was started from what looks like a USB drive (OPL's Apps list calls it mass0:, mass1:...): the helper in
 * its folder there, as OPL's "IGR Path" wants it (OPL's IGR calls the USB drive mass:). "" = it wasn't */
static const char *usb_helper_path(void)
{
    static char t[260];
    const char *p = strchr(appPath, ':'), *slash;
    t[0] = 0;
    if (!appElsewhere || !p || (strncasecmp(appPath, "mass", 4) != 0 && strncasecmp(appPath, "usb", 3) != 0))
        return t;
    slash = strrchr(++p, '/');
    snprintf(t, sizeof(t), "mass:%.*sSD2CLOUD-IGR.ELF", slash ? (int)(slash - p + 1) : 0, p);
    return t;
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
    int r;
    message(0, NULL, COLOR_TEXT, T(T_AUTO_INSTALLING));
    r = helper_install();
    helperState = helper_status();
    if (r != 0) {
        message_wait(COLOR_ERROR, T(T_HELPER_TITLE), COLOR_TEXT, helper_result(r));
        return;
    }
    dlg_new(COLOR_OK, T(doneTitle));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_AUTO_OPL));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 10, "mc?:" HELPER_TARGET);
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_AUTO_NOTE));
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

/* the IGR helper in the settings. Not installed: says what it does, where it goes and how much of the memory card it
 * takes, and installs it once the user agrees. Installed: installs it again (or updates it), or removes it */
static void helper_screen(void)
{
    if (!helper_present()) {
        dlg_new(COLOR_TITLE, T(T_HELPER_TITLE));
        if (appElsewhere) {
            /* the memory card's helper only starts SD2Cloud from the microSD; from a USB drive OPL runs the helper in
             * SD2Cloud's own folder, with nothing to install */
            dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_HELPER_ELSEWHERE));
            if (usb_helper_path()[0]) {
                dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_HELPER_USB));
                dlg_line(FONT_TEXT, COLOR_ACCENT, 0, usb_helper_path());
            }
        } else
            dlg_line(FONT_TEXT, COLOR_ERROR, 0, T(T_HELPER_MISSING));
        next.wide = 1;
        dlg_buttons(BUTTON_CIRCLE, T_BACK, 0, 0);
        dlg_show();
        wait_button(PAD_CIRCLE | PAD_CROSS, 0);
        sound_play(SND_BACK);
        return;
    }
    if (helperState == HELPER_SAME || helperState == HELPER_DIFFERENT) {
        static const char *items[2];
        int k;
        items[0] = T(helperState == HELPER_DIFFERENT ? T_UPDATE : T_HELPER_REINSTALL);
        items[1] = T(T_HELPER_UNINSTALL);
        if ((k = choose(T(T_HELPER_TITLE), items, 2, 0)) < 0)
            return;
        if (k == 1) {
            helper_remove();
            return;
        }
    }
    dlg_new(COLOR_TITLE, T(T_HELPER_TITLE));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_HELPER_ABOUT));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_HELPER_WHERE));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 4, "mc0:" HELPER_TARGET);
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, helper_space_text());
    dlg_line(FONT_SMALL, COLOR_DIM, 4, T(T_HELPER_AUTOBOOT));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_HELPER_USB_HINT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_HELPER_INSTALL);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return;
    }
    sound_play(SND_CONFIRM);
    helper_install_now(T_HELPER_OK);
}

/* updates SD2Cloud: downloads, verifies, replaces and reopens the new version (which offers to update the memory
 * card's helper) */
static void update_app(void)
{
    char c[260];
    snprintf(c, sizeof(c), T(T_MENU_UPDATE), update_tag());
    if (!confirm(c, T(T_CONFIRM_UPDATE_TEXT), T_UPDATE))
        return;
    message(0, NULL, COLOR_TEXT, T(T_UPDATE_RUNNING));
    googleError[0] = 0;
    if (update_install() != 0) {
        char t[400];
        snprintf(t, sizeof(t), "%s %s", T(T_UPDATE_ERROR), googleError);
        message_wait(0, NULL, COLOR_ERROR, t);
        return;
    }
    message(0, NULL, COLOR_OK, T(T_UPDATE_DONE));
    sound_play(SND_CONFIRM);
    sleep_ms(1500);
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
 * offers the new version; otherwise says this one is the latest, or that GitHub couldn't be reached */
static void check_updates_now(void)
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
        r = update_check();
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

/* where to go after IGR: automatic, a program in APPS or the PS2 menu */
static void pick_return(void)
{
    static const char *items[MAX_APPS + 3];
    static char values[MAX_APPS + 3][200];
    char *now = cfg.igr_return, q[260], w[260];
    int n = 0, i, k, start = 0;
    items[n] = T(T_AUTO);
    snprintf(values[n++], sizeof(values[0]), "auto");
    for (i = 0; i < nApps; i++) {
        items[n] = given_name(apps[i].path) ? given_name(apps[i].path) : apps[i].title;
        target_for_ini(apps[i].path, values[n++], sizeof(values[0]));
    }
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
    if (!strcasecmp(values[k], now))
        return;
    if (cfg.igr_name[0]) {   /* the name was the other program's */
        cfg.igr_name[0] = 0;
        config_set("igr", "name", "");
    }
    snprintf(now, sizeof(cfg.igr_return), "%s", values[k]);
    config_set("igr", "return", values[k]);
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

/* connected: disconnects (after asking); not connected: signs in */
static void account_screen(void)
{
    int r;
    if (!google_has_access()) {
        googleError[0] = 0;
        if ((r = ensure_google(1)) > 0) {
            char t[400];
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(COLOR_ERROR, T(T_LOGIN_ERROR), COLOR_TEXT, t);
        } else if (r == 0)
            offer_auto_sync();
        return;
    }
    if (!confirm(T(T_LOGOUT_ASK), T(T_LOGOUT_TEXT), T_LOGOUT_YES))
        return;
    if (!networkUp) {
        message(0, NULL, COLOR_TEXT, T(T_NET_STARTING));
        networkUp = network_up() == 0;
    }
    google_logout(networkUp && google_init() == 0);
    message_wait(0, NULL, COLOR_OK, T(T_LOGOUT_DONE));
}

static void about_screen(void)
{
    char t[120];
    snprintf(t, sizeof(t), "SD2Cloud v%s", APP_VERSION);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_ABOUT_TEXT));
    dlg_line(FONT_SMALL, COLOR_DIM, 4, T(T_ABOUT_CREDITS));
    dlg_line(FONT_SMALL, COLOR_DIM, 10, T(T_ABOUT_LICENSES));
    dlg_line(FONT_TEXT, COLOR_TEXT, 2, T(T_ABOUT_AUTHOR));
    snprintf(t, sizeof(t), "github.com/%s", cfg.repo);
    dlg_line(FONT_SMALL, COLOR_ACCENT, 0, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, 0, 0);
    next.wide = 1;
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* circle on the main screen: where to go (the programs in APPS or the PS2 menu). The cursor starts on the last choice
 * (sd2cloud.ini [manual] return; "auto" = the OPL SD2Cloud would pick), and the choice is kept for next time */
static void exit_menu(void)
{
    static const char *items[MAX_APPS + 2];
    static char values[MAX_APPS + 2][200];
    char q[260], w[260];
    int n = 0, i, k, start = 0;
    if (!appsListed)
        list_apps();
    for (i = 0; i < nApps; i++) {
        items[n] = given_name(apps[i].path) ? given_name(apps[i].path) : apps[i].title;
        target_for_ini(apps[i].path, values[n++], sizeof(values[0]));
    }
    items[n] = T(T_RET_OSD);
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
    if (strcasecmp(values[k], cfg.manual_return) != 0) {
        if (cfg.manual_name[0])   /* the name was the other program's */
            config_set("manual", "name", "");
        config_set("manual", "return", values[k]);
    }
    leave(values[k]);
}

static void settings_screen(void)
{
    if (!appsListed)
        list_apps();
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
        case SET_HELPER:
            helper_screen();
            helperState = helper_status();
            break;
        case SET_IGR_RETURN:
            pick_return();
            break;
        case SET_LANGUAGE:
            pick_language();
            break;
        case SET_KEEP:
            pick_keep();
            break;
        case SET_UPDATES:
            check_updates_now();
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
static void offer_auto_sync(void)
{
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
    helper_install_now(T_AUTO_DONE);
}

/* no Google account yet: connect now (network + the code on the TV) or later (straight to the menu, without waiting for
 * the network; the account can be connected in the settings). 1 = now */
static int ask_connect(void)
{
    dlg_new(COLOR_TITLE, T(T_SET_ACCOUNT));
    dlg_line(FONT_TEXT, COLOR_TEXT, 10, T(T_CONNECT_ASK));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_CONNECT_HINT));
    dlg_buttons(BUTTON_CIRCLE, T_LATER, BUTTON_CROSS, T_CONNECT);
    dlg_show();
    if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        return 1;
    }
    sound_play(SND_BACK);
    log_msg("no Google account: later");
    return 0;
}

static void manual(void)
{
    int n, r, i;
    message(0, NULL, COLOR_TEXT, T(T_SEARCHING));
    n = cards_scan();
    if (n < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_NO_SD));
        leave(cfg.manual_return);
    }
    cards_check(checking_progress);
#ifdef TEST
    if (test_take('V'))
        test_preview(test_digit());
#endif
    helperState = helper_status();
#ifdef TEST
    {   /* checks the in-use detection on PCSX2: the root signature of the card in slot 1 against each .mcd's */
        char seen[65], file[65];
        if (mc_root_signature(0, seen) == 0)
            for (i = 0; i < nCards; i++)
                if (mcfs_root_signature(cards[i].path, file) == 0)
                    log_msg("root signature: slot 1 %.16s, %s %.16s%s", seen, cards[i].id, file, strcmp(seen, file) ? "" : "  <- same card");
    }
#endif
    if (!google_has_access() && ask_connect()) {
        googleError[0] = 0;
        r = ensure_google(1);
        if (r == 0)
            offer_auto_sync();
        else if (r > 0) {
            char t[400];
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            dlg_new(COLOR_ERROR, T(T_LOGIN_ERROR));
            dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
            dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
            dlg_show();
            wait_button(PAD_CROSS | PAD_CIRCLE, 0);
            sound_play(SND_BACK);
        }
    }
#ifdef TEST
    if (test_take('N'))
        testNoLinkOnce = 1;
    if (test_take('n'))
        testNoDhcpOnce = 1;
    if (test_take('u'))
        testNoZero = 1;
#endif
#ifdef TEST
    if (test_take('A'))   /* the questions after the first sign-in, without signing in */
        offer_auto_sync();
#endif
    if (google_has_access() && state_empty())
        first_run();
    remember_unseen();
    /* the cursor starts on the first card that needs a backup, in its group */
    for (i = 0; i < nCards && !is_selected(&cards[i], 1); i++)
        ;
    ui_lock();
    tabs_init(&menu.g, NULL);
    if (i < nCards) {
        int k;
        tabs_show(&menu.g, tab_of(&cards[i]));
        for (k = 0; k < menu.g.n; k++)
            if (menu.g.idx[k] == i)
                menu.g.cursor = k;
        scroll_to(menu.g.cursor, &menu.g.top);
    }
    ui_unlock();
    for (;;) {
        u32 b, keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_START;
        ui_scene(scene_menu);
        /* the selected card's icon is read when the cursor rests a moment */
        b = wait_nav_ms(keys, 250);
        if (!b) {
            menu_icon();
            b = wait_nav(keys);
        }
        if (tabs_nav(&menu.g, b))
            continue;
        if ((b & PAD_CROSS) && menu.g.n) {
            sound_play(SND_CONFIRM);
            card_screen(tabs_card(&menu.g));
        } else if ((b & PAD_TRIANGLE) && menu.g.n) {
            sound_play(SND_CONFIRM);
            card_options(tabs_card(&menu.g));
        } else if (b & PAD_START) {
            sound_play(SND_CONFIRM);
            settings_screen();
        } else if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            exit_menu();
        }
    }
}

/* ------------------------------------------------------------ IGR */

static void igr(void)
{
    int i, n = 0;
    message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_DIM, T(T_SEARCHING));
    /* the sd2psx writes the game's card to the microSD when it goes idle (or when switching to the BootCard): wait a
     * little */
    sleep_ms(cfg.igr_settle * 1000);
    if (cards_scan() < 0) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_ERROR, T(T_NO_SD));
        sleep_ms(3000);
        leave(cfg.igr_return);
    }
    cards_check(checking_progress);
    for (i = 0; i < nCards; i++)
        n += is_selected(&cards[i], 0);
    /* at IGR what matters is getting back to OPL quickly: nothing changed = say so and return; everything went well =
     * "backup sent" with the exit sound and return. A failure stays on screen a little longer (summary_seconds), else
     * it would go unnoticed. Cancelling returns right away. */
    if (!n) {
        message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_TEXT, T(T_IGR_NOTHING));
        leave(cfg.igr_return);
    }
    run_backup(0, 1);
    if (backupCancelled)
        leave(cfg.igr_return);
    if (failed) {
        if (cfg.igr_summary > 0)
            summary_screen(cfg.igr_summary);
        leave(cfg.igr_return);
    }
    dlg_new(COLOR_OK, T(T_IGR_DONE));
    for (i = nResults > DLG_LINES ? nResults - DLG_LINES : 0; i < nResults; i++)
        dlg_line(FONT_SMALL, COLOR_TEXT, 2, results[i].text);
    dlg_show();
    leave(cfg.igr_return);
}

/* ------------------------------------------------------------ main */

int main(int argc, char *argv[])
{
    int r;
    system_init(argc, argv);
    i18n_select(cfg.language);
    /* first run: create the sd2cloud.ini with every option explained, in the screen's language */
    if (!configExists)
        config_write_template();
    /* The settings file only gets a line of the app's own when one is needed. Where the program is, for the IGR helper:
     * the settings are always in the same place on the microSD, so the program itself can be in any folder of it (in
     * its usual folder nothing is written). Its version, once a copy on another device (a USB drive) has handed over
     * to this one: started by IGR, that copy runs by itself only if it is the same version */
    if (!strncmp(appPath, "mmce", 4) || !strncmp(appPath, "host:", 5)) {
        if (strcasecmp(cfg.app_path, appPath) != 0) {
            snprintf(cfg.app_path, sizeof(cfg.app_path), "%s", appPath);
            config_set("app", "app_path", appPath);
        }
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
    ui_unlock();
    state_read();
    token_read();
    google_set_poll(watch_cancel);
#ifdef TEST
    if (!igrMode)
        igrMode = test_has_script() ? test_take('I') : 1;   /* on the console, with nobody at the controller: run as IGR */
#endif
    if (igrMode)
        igr();
    manual();
    return 0;
}
