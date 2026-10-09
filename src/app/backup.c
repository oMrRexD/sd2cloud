/* SD2Cloud -- the sync: the cards that changed are sent to Google Drive, one by one, and the old backups past the
 * limit are deleted; what was done is said at the end. */
#include "app.h"

static int backupSkip;   /* the user gave up the card on its way (confirm_skip's answer) */

int on_progress(long long done, long long total)
{
    watch_cancel();
    if (cancelLatched) {
        if (confirm_cancel(restoring ? T_RESTORE_CANCEL_TITLE : T_CANCEL_TITLE,
                           restoring ? T_RESTORE_CANCEL_TEXT : igrMode ? T_CANCEL_TEXT : T_CANCEL_TEXT_MANUAL)) {
            backupCancelled = 1;
            return 1;
        }
    }
    if (skipLatched && current && (backupSkip = confirm_skip(current)) != 0)
        return 1;
    upload_screen(done, total);
#ifdef DEBUG_BUILD
    if (done * 2 >= total && done > 0 && debug_take('E'))
        debug_capture_and_stop();
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
int is_selected(const card_t *c, int mode)
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
int card_folder(const card_t *c, char *folderId, size_t size)
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
void run_backup(int mode, int rotate)
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
    skipLatched = backupSkip = 0;
    skipOffered = mode != 3;   /* (a card synced by itself is given up with circle) */
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
            if (backupSkip) {   /* left out by the user: of this sync, or of every one from now on */
                add_result(COLOR_DIM, "%s: %s", c->base, T(T_UPLOAD_SKIPPED));
                if (backupSkip == 2)
                    card_set_included(c, 0);
                backupSkip = 0;
                continue;
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
    skipOffered = 0;
    state_write();
}

/* the results of a backup; seconds = 0 waits for a button, else shows them that long (IGR) */
void summary_screen(int seconds)
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
void manual_backup(int mode)
{
    run_backup(mode, 1);
    if (backupCancelled && !sent && !failed)
        return;
    summary_screen(0);
}

/* ------------------------------------------------------------ manual */

void count_cards(int *included, int *changed, long long *bytes)
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
void remember_unseen(void)
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
void first_run(void)
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

/* -------- syncing one card (□ in its saves, "Sync now" in its options): asked first; a card that is already synced
 * says so and can be synced again (a new backup on Drive). 1 = it ran */

int sync_card(card_t *c)
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
