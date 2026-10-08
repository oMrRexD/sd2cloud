/* SD2Cloud -- a card's backups on Google Drive, listed, and one of them restored over the card. */
#include "app.h"

/* -------- restore */

/* a card file is being installed, not a backup from Drive restored: 1 = it is read into memory first, 2 = it is
 * there already, and writing it is all there is to wait for, 3 = it goes straight from the file (or it isn't known yet
 * which: it is 1 once the file is seen being read) */
int restoreFile;
int restoreNew;   /* as a card that wasn't there: it can be given up on to the end (it is deleted then) */
u64 lastRestoreDraw;
int lastRestorePhase;

/* A piece of a card file couldn't be read, with the card half written. X tries again (a drive that came loose can
 * be put back), circle gives the file up. 1 = given up */
static int source_lost(void)
{
    char t[200];
    int again;
    watchOff++;
    sound_play(SND_BACK);
    dlg_new(COLOR_ERROR, T(T_INSTALL_LOST));
    if (!restoreNew && current) {   /* (a new card just isn't made) */
        snprintf(t, sizeof(t), T(T_INSTALL_LOST_TEXT), current->base);
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    }
    dlg_buttons(BUTTON_CIRCLE, T_CANCEL, BUTTON_CROSS, T_TRY_AGAIN);
    dlg_show();
    again = (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) != 0;
    sound_play(again ? SND_CONFIRM : SND_BACK);
    log_msg("the card file can't be read: %s", again ? "trying again" : "given up");
    cancelLatched = 0;
    circleDown = 1;
    watchOff--;
    if (again)
        ui_scene(scene_upload);
    return !again;
}

/* one screen and one bar for the whole restore, so the bar never goes back: downloading 30%, checking the download
 * 10%, writing 40%, reading it back 20%. A card file: read into memory 25%, written 50%, read back 25%; when it is in
 * memory already, 65% and 35%; when it goes straight from the file, 80% and 20%. The two steps the user cares about
 * are the only ones named */
int restore_progress(int phase, long long done, long long total)
{
    static const int start[4][4] = {{0, 300, 400, 800}, {0, 250, 250, 750}, {0, 0, 0, 650}, {0, 0, 0, 800}};
    static const int span[4][4] = {{300, 100, 400, 200}, {250, 0, 500, 250}, {0, 0, 650, 350}, {0, 0, 800, 200}};
    int canStop;
    if (phase == RESTORE_LOST)
        return source_lost();
    if (restoreFile == 3 && phase == RESTORE_DOWNLOAD)
        restoreFile = 1;
    canStop = phase < RESTORE_WRITE || restoreNew;   /* once it starts writing over a card there's no cancelling */
#ifdef DEBUG_BUILD
    if (phase == RESTORE_WRITE && total && done * 2 >= total && debug_take('k'))
        cancelLatched = 1;   /* (script: circle halfway through the writing) */
#endif
    if (canStop) {
        watch_cancel();
        if (cancelLatched) {
            if (confirm_cancel(restoreFile ? T_INSTALL_CANCEL_TITLE : T_RESTORE_CANCEL_TITLE,
                               restoreNew ? T_INSTALL_CANCEL_NEW : restoreFile ? T_INSTALL_CANCEL_TEXT : T_RESTORE_CANCEL_TEXT))
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
    ui_lock();
    workText = T(phase >= RESTORE_WRITE ? T_RESTORE_STEP_WRITE : restoreFile ? T_LOADING : T_RESTORE_STEP_DOWNLOAD);
    workFixed = !canStop;
    ui_unlock();
    upload_screen(start[restoreFile][phase] + (total ? span[restoreFile][phase] * done / total : 0), 1000);
#ifdef DEBUG_BUILD
    if (total && done * 2 >= total)
        debug_capture_if(phase == RESTORE_WRITE ? 'W' : 'E');
#endif
    return 0;
}

/* confirm, back up the current card if it changed, and restore. 1 = the card was restored */
static int restore_flow(card_t *c, const drive_file_t *f)
{
    char when[40], t[300];
    int active, channel = 0, inUse, unsure, r;
    backup_when(c, f, when, sizeof(when));
    for (;;) {
        active = active_card(&channel);
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
    lastRestoreDraw = 0;
    lastRestorePhase = -1;
    card_work(c, 1, T(T_RESTORE_STEP_DOWNLOAD), 0);
    r = restore_card(c, f, restore_progress);
    card_work_done();
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

void history_screen(card_t *c, icon_t *icon)
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
