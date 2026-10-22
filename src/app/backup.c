/* SD2Cloud -- the sync: the cards that changed are sent to Google Drive, one by one, and the old backups past the
 * limit are deleted; what was done is said at the end. */
#include "app.h"

static int backupSkip;   /* the user gave up the card on its way (confirm_skip's answer; 3 = marked as resolved, from
                            the list of the sync's cards) */

/* Circle (the whole sync) and square (the card on its way), asked as soon as the button is pressed: from here, and
 * from inside the request under way while cards are sent (google_set_stop). 1 = stop what is under way; what was
 * answered stays, so whatever comes next of it stops at once too */
static int stop_asked(void)
{
    if (backupCancelled || backupSkip)
        return 1;
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
    if ((backupSkip = sync_list_poll()) != 0)   /* (asked on the list of the sync's cards) */
        return 1;
    return 0;
}

int on_progress(long long done, long long total)
{
    if (stop_asked())
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
    if (mode == 0)   /* (one marked as resolved as it is, left alone) */
        return (c->status == ST_NEW || c->status == ST_CHANGED) && !c->resolved;
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

/* The access to Google of the sync under way, asked for the first time a card's folder on Drive is needed: a sync with
 * nothing to send (at IGR, a Memory Card whose saves are as they were) doesn't wait for the network at all. 1 = it has
 * it (or no sync is under way), 0 = not yet, -1 = it couldn't be had (said in googleError) */
static int online = 1;

static int go_online(void)
{
    char t[sizeof(googleError)];
    int r;
    if (online)
        return online > 0 ? 0 : -1;
    googleError[0] = 0;
#ifdef DEBUG_BUILD
    r = debugDry ? 0 : ensure_google(!igrMode);   /* (a dry sync: nothing goes, nothing is asked) */
#else
    r = ensure_google(!igrMode);
#endif
    if (r != 0) {
        online = -1;
        if (r < 0)   /* backed out of the sign-in: the same as cancelling */
            backupCancelled = 1;
        snprintf(t, sizeof(t), "%s %s", r > 0 ? T(r) : "", googleError);
        snprintf(googleError, sizeof(googleError), "%s", t);
        return -1;
    }
    /* the main folder is checked once per run (it may have been deleted on Drive), and so are the second layout's */
    rootId[0] = 0;
    drive_session_start();
    online = 1;
    return 0;
}

/* the folder of a card's backups: in the first layout, its group's in the main folder, from the cache or found
 * (created if missing); in the second, its own (drive.c). 0 = ok */
int card_folder(const card_t *c, char *folderId, size_t size)
{
    char key[200];
    const char *cache;
    if (go_online() != 0)
        return -1;
    if (drive_layout() == 2)   /* (each card in a folder of its own, its group's in its device's: drive.c) */
        return drive_folder_of(c, 0, NULL, folderId, size);
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

/* A card left out of the sync, from the list of its cards (or square): of this sync only (SY_SKIPPED); taken as synced
 * as it is now, the automatic sync leaving it alone until it changes again (SY_RESOLVED); or with its sync turned off
 * (SY_OFF). result = it's said among the results */
static void left_out(card_t *c, int how, int result)
{
    if (how == SY_RESOLVED) {
        card_state_t *e = state_card(c->id, 1);
        if (e)
            snprintf(e->resolved, sizeof(e->resolved), "%s", c->fingerprint);
        c->resolved = 1;
    } else if (how == SY_OFF)
        card_set_included(c, 0);
    if (result)
        add_result(COLOR_DIM, "%s: %s", c->base, T(how == SY_RESOLVED ? T_UPLOAD_RESOLVED : how == SY_OFF ? T_UPLOAD_OFF : T_UPLOAD_SKIPPED));
}

#ifdef DEBUG_BUILD
/* script "B": a sync with nothing sent, PCSX2 having no Google account: each card "goes" in 3 s, the buttons the
 * script holds meanwhile (scriptLive), and is taken as sent */
int debugDry;

static int dry_upload(const card_t *c, stream_t *s)
{
    u64 start = now_ms();
    long long done;
    while ((done = (long long)(now_ms() - start) * c->size / 3000) < c->size) {
        if (on_progress(done, c->size))
            return -1;
        sleep_ms(30);
    }
    s->sent = c->size / 4;
    snprintf(s->sha_mcd, sizeof(s->sha_mcd), "dry");
    return 0;
}
#define DRY debugDry
#else
#define DRY 0
#endif

/* rotate = delete the backups beyond the configured limit (not before a restore: the backup picked to restore could
 * be the oldest one) */
/* "28 saves uploaded", "1 save uploaded" */
static const char *saves_sent(int n)
{
    static char t[80];
    if (n == 1)
        return T(T_SAVES_SENT_ONE);
    snprintf(t, sizeof(t), T(T_SAVES_SENT_N), n);
    return t;
}

/* the sync was cancelled, or the card on its way skipped (what goes by itself, as a card's saves, stops too) */
int sync_stop(void) { return backupCancelled || backupSkip; }

/* A Memory Card's turn in the sync (a row of the list, after the cards, when the backup by save is on): its saves
 * that changed since they were last sent, each by itself to its Saves folder on Drive */
static void memcard_turn(int k)
{
    card_t *c = syncCards[k].c, *of = library_path(c->path) ? c : &slotCard;   /* (the Library's own, or the slot's) */
    int r;
    sync_set(k, SY_SENDING);
    googleError[0] = 0;
    if (of == &slotCard) {
        memcard_pick(mcfs_slot_port(c->path));
        if (!memcard_there()) {   /* (taken out since the sync started) */
            sync_set(k, SY_SKIPPED);
            return;
        }
    }
    load_card_icon(of);
    work_text(T(T_SAVES_CHECKING));   /* (what is done first: its saves read, to tell which changed. Its folder on Drive
                                         only once one has: saves_backup) */
    upload_screen(0, 1);
    r = stop_asked() ? -1 : saves_backup(of, NULL);
    if (backupCancelled) {
        sync_set(k, SY_WAITING);
        return;
    }
    if (backupSkip) {   /* (square, or the list: its saves left out of this sync) */
        sync_set(k, SY_SKIPPED);
        left_out(c, SY_SKIPPED, 1);
        backupSkip = 0;
        return;
    }
    if (r > 0) {
        sync_set(k, SY_SENT);
        add_result(COLOR_OK, "%s: %s", c->base, saves_sent(r));
        sent++;
    } else if (r == 0)
        sync_set(k, SY_UNCHANGED);
    else {
        sync_set(k, SY_FAILED);
        add_result(COLOR_ERROR, "%s: %s (%s)", c->base, T(T_UPLOAD_FAILED), googleError);
        failed++;
    }
}

void run_backup(int mode, int rotate)
{
    char name[160], folderId[80], t[200];
    int k, r;
    datetime_t when;

    nResults = sent = failed = 0;
    backupCancelled = cancelLatched = 0;
    googleError[0] = 0;
    online = 0;   /* (asked for once something has to go: go_online) */
    if (!DRY && !google_has_access()) {   /* no account yet: the sign-in first, before the screen of the sync */
        if (go_online() != 0) {
            if (!backupCancelled) {
                add_result(COLOR_ERROR, "%s", googleError);
                failed++;
            }
            online = 1;
            return;
        }
    }

    sync_list_start(mode);
    totalN = nSync;
    currentN = 0;
    skipLatched = backupSkip = 0;
    skipOffered = mode != 3;   /* (a card synced by itself is given up with circle) */
#ifdef DEBUG_BUILD
    scriptLive = debugDry;
#endif
    google_set_stop(stop_asked);
    for (k = 0; k < nSync && !backupCancelled && online >= 0; k++) {   /* (no access to Google: the rest waits too) */
        card_t *c = syncCards[k].c;
        card_state_t *e;
        stream_t s;
        char id[80];
        currentN++;
        syncCards[k].taken = 1;
        if (syncCards[k].state != SY_WAITING) {   /* left out on the list before its turn came */
            left_out(c, syncCards[k].state, 1);
            continue;
        }
        if (mcfs_slot_port(c->path) >= 0 || library_path(c->path)) {   /* a Memory Card, the Library: its saves, each by
                                                                         itself */
            memcard_turn(k);
            continue;
        }
        sync_set(k, SY_SENDING);
        googleError[0] = 0;
        load_card_icon(c);
        upload_screen(0, c->size);
        /* (the card's folder on Drive first: a button pressed meanwhile is answered there too) */
        r = stop_asked() || (!DRY && card_folder(c, folderId, sizeof(folderId)) != 0) ? -1 : 0;
        if (r == 0) {
            local_time(&when);
#ifdef DEBUG_BUILD
            if (debugDry) {   /* (always the same, as the tests compare what the state file has) */
                datetime_t fixed = {2026, 10, 11, 21, 0, 0};
                when = fixed;
            }
#endif
            e = state_card(c->id, 0);
            /* two backups of the same card in the same minute get the seconds in the name */
            snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", when.year, when.month, when.day, when.hour, when.minute);
            if (e && !strncmp(e->when, t, 16))
                snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02dm%02d.zip", c->base, when.year, when.month, when.day,
                         when.hour, when.minute, when.second);
            else
                snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02d.zip", c->base, when.year, when.month, when.day, when.hour,
                         when.minute);
            memset(&s, 0, sizeof(s));
#ifdef DEBUG_BUILD
            if (debugDry)
                r = dry_upload(c, &s);
            else
#endif
                r = google_upload(c, folderId, name, &when, &s, on_progress, id, sizeof(id));
        }
        if (r != 0) {
            if (backupCancelled) {
                sync_set(k, SY_WAITING);
                log_msg("%s: upload cancelled", c->id);
                break;
            }
            if (backupSkip) {   /* left out by the user: of this sync, or of every one from now on; or resolved */
                int how = backupSkip == 3 ? SY_RESOLVED : backupSkip == 2 ? SY_OFF : SY_SKIPPED;
                sync_set(k, how);
                left_out(c, how, 1);
                backupSkip = 0;
                continue;
            }
            sync_set(k, SY_FAILED);
            add_result(COLOR_ERROR, "%s: %s (%s)", c->base, T(T_UPLOAD_FAILED), googleError);
            failed++;
            continue;
        }
        sync_set(k, SY_SENT);
        e = state_card(c->id, 1);
        if (e) {
            snprintf(e->fingerprint, sizeof(e->fingerprint), "%s", c->fingerprint);
            e->version = MCFS_VERSION;
            snprintf(e->sha, sizeof(e->sha), "%s", s.sha_mcd);
            snprintf(e->when, sizeof(e->when), "%04d-%02d-%02d %02d:%02d:%02d", when.year, when.month, when.day, when.hour, when.minute,
                     when.second);
            e->resolved[0] = 0;   /* (it's synced now: a mark of resolved has done its part) */
        }
        c->resolved = 0;
        state_write();
        c->status = ST_UP_TO_DATE;
        sent++;
        format_size(s.sent, t, sizeof(t));
        add_result(COLOR_OK, "%s: %s (%s)", c->base, T(T_UPLOAD_OK), t);
        if (rotate && !DRY)
            delete_old(c, folderId, id);
        if (cfg.save_backup && !backupCancelled) {   /* (and its saves that changed, each by itself) */
            int each = saves_backup(c, folderId);
            if (each > 0)
                add_result(COLOR_OK, "%s: %s", c->base, saves_sent(each));
        }
        backupSkip = 0;   /* (square while the old backups were deleted: this card was sent already) */
    }
    for (; k < nSync; k++)   /* cancelled: a card resolved or turned off on the list before its turn still is */
        if (syncCards[k].state == SY_RESOLVED || syncCards[k].state == SY_OFF)
            left_out(syncCards[k].c, syncCards[k].state, 0);
    google_set_stop(NULL);
#ifdef DEBUG_BUILD
    scriptLive = 0;
#endif
    sync_list_end();
    skipOffered = 0;
    backupSkip = 0;
    online = 1;
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

/* first run: offers to sync every card, and with the backup by save the Memory Cards in the slots (their saves, each
 * by itself: with no sd2psx, all there is); on "later", nothing is sent (remember_unseen keeps the references) */
void first_run(void)
{
    char t[300], size[24];
    int included, changed, minutes, ports[2], n, i;
    long long bytes, left;
    count_cards(&included, &changed, &bytes);
    for (n = cfg.save_backup ? memcard_ports(ports) : 0, i = 0; i < n; i++) {
        memcard_pick(ports[i]);
        if (memcard_there()) {   /* (what its saves take: of the 8 MB a Memory Card has, what isn't free) */
            included++;
            if (mcfs_list_saves(slotCard.path, NULL, 0, &left) >= 0 && left >= 0 && left < MCFS_CARD_8MB)
                bytes += MCFS_CARD_8MB - left;
        }
    }
    if (!included)
        return;
    format_size(bytes, size, sizeof(size));
    minutes = (int)((bytes / (1024 * 1024)) * 2 / 60) + 1;   /* ~2 s per MB on the console (read + compress + upload) */
    dlg_new(COLOR_TITLE, T(T_FIRST_TITLE));
    if (included == 1)
        snprintf(t, sizeof(t), T(T_FIRST_QUESTION_ONE), size, minutes);
    else
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
