/* SD2Cloud -- the backup by save: each save of a card that changed since it was last sent, sent by itself as a .psu
 * to the card's Saves folder on Drive (where "Send to Drive" sends one), the newest of each kept as its backups are;
 * and a save's versions there, from its page, one of them put back in its place. */
#include "app.h"

#ifdef DEBUG_BUILD
#define DRY debugDry
#else
#define DRY 0
#endif

static mcfs_save_t sv[MCFS_MAX_SAVES];
static unsigned long long svSig[MCFS_MAX_SAVES];
static char svStamp[MCFS_MAX_SAVES][17];   /* a Memory Card's: what each folder lists (mcfs_save_stamp), in hex; "" = none */

/* whose saves they are, for the state and on Drive: the Memory Card is one, whichever slot it is in */
static const char *owner_of(const card_t *c) { return mcfs_slot_port(c->path) >= 0 ? memcard_id() : c->id; }

/* what a save's versions are tagged with on Drive (google_list lists them by it) */
static void save_tag(const card_t *c, const char *folder, char *out, size_t size)
{
    snprintf(out, size, "save:%.80s/%s", owner_of(c), folder);
}

/* does save i go: it was read (or listed what it did), and its signature isn't the one it had when it was last sent?
 * (one that couldn't be read stays as it was) */
static int goes(const card_t *c, int i)
{
    char hex[17];
    const char *was;
    snprintf(hex, sizeof(hex), "%016llx", svSig[i]);
    return svSig[i] && ((was = state_save(owner_of(c), sv[i].folder)) == NULL || strcmp(was, hex) != 0);
}

/* what became of save i once looked at, on the list of its saves (SELECT) */
static void looked_at(const card_t *c, int i)
{
    sync_saves_set(i, !svSig[i] ? SY_FAILED : goes(c, i) ? SY_WAITING : SY_UNCHANGED);
}

/* each save's signature: the engine's, which reads the card's file; a Memory Card's, from its .psu (mcman reads it:
 * the engine doesn't read its files one by one). A Memory Card's save whose folder lists what it listed when it was
 * last sent isn't read: its signature is the one it had then (reading every save takes half a minute on a full card,
 * listing them, a second). Only while the console's clock goes on: one that stands still dates every save the same */
static int signatures(const card_t *c, int n)
{
    int i, listed = 0, stamps;
    if (library_path(c->path)) {   /* (the Library: kept since each save went into it, nothing read) */
        for (i = 0; i < n; i++)
            svSig[i] = library_signature(sv[i].folder), svStamp[i][0] = 0;
        return 0;
    }
    if (mcfs_slot_port(c->path) < 0) {
        for (i = 0; i < n; i++)
            svStamp[i][0] = 0;
        return mcfs_save_signatures(c->path, sv, n, svSig);
    }
    stamps = clock_goes();
    for (i = 0; i < n; i++) {   /* (the slot's card is read through mcman, a save at a time: its bar, and a cancel heard) */
        buffer_t psu = {0};
        unsigned long long h = 1469598103934665603ULL, stamp;   /* (FNV-1a, 64 bits) */
        const char *was;
        size_t k;
        svSig[i] = 0;
        svStamp[i][0] = 0;
        if (on_progress(i, n))
            return -1;
        sync_saves_set(i, SY_CHECKING);
        if (stamps && mcfs_save_stamp(c->path, &sv[i], &stamp) == 0) {
            snprintf(svStamp[i], sizeof(svStamp[i]), "%016llx", stamp);
            if ((was = state_save(owner_of(c), sv[i].folder)) != NULL && !strcmp(state_save_stamp(owner_of(c), sv[i].folder), svStamp[i])) {
                svSig[i] = strtoull(was, NULL, 16);
                listed++;
                looked_at(c, i);
                continue;
            }
        }
#ifdef DEBUG_BUILD
        if (i * 2 + 1 >= n)
            debug_capture_if('J');   /* (script: the screen of the saves being checked, past half of them) */
#endif
#ifdef DEBUG_BUILD
        log_msg("signature of %s...", sv[i].folder);
#endif
        if (mcfs_export_psu(c->path, sv[i].folder, &psu) == MCFS_OK) {
            for (k = 0; k < psu.len; k++)
                h = (h ^ psu.data[k]) * 1099511628211ULL;
            svSig[i] = h ? h : 1;
        }
        buf_free(&psu);
        looked_at(c, i);
    }
    log_msg("%s: %d of the %d saves list what they did when they were last sent: not read", c->id, listed, n);
    if (mc_no_answer()) {   /* (not "nothing changed": the backup didn't happen) */
        snprintf(googleError, sizeof(googleError), "%s", T(T_MEMCARD_NO_ANSWER));
        return -1;
    }
    return 0;
}

/* the card's Saves folder on Drive (folderId = the card's folder, in the first layout), found or made. save = the
 * save's folder: the Library's go into their game's (the second layout) */
static int saves_folder(const card_t *c, const char *save, const char *folderId, char *savesId, size_t size)
{
    char key[200];
    const char *cache;
    if (drive_layout() == 2)
        return drive_folder_of(c, 1, save, savesId, size);
    snprintf(key, sizeof(key), "%s/%s/Saves", cfg.drive_folder, c->folder);
    savesId[0] = 0;
    if (library_path(c->path)) {   /* (the Library's saves: in its own folder, "Library", which has nothing else) */
        snprintf(savesId, size, "%s", folderId);
        return 0;
    }
    if ((cache = state_folder(key)) != NULL)
        snprintf(savesId, size, "%s", cache);
    if (google_folder("Saves", folderId, savesId, size) != 0)
        return -1;
    state_set_folder(key, savesId);
    return 0;
}

/* -------- the screen while the saves go: each one's icon turning, its name, its card, a bar for it and "N of M" */

static save_view_t sending;

/* save i of the card, the k-th of m that go, on the screen of a save on its way */
static void sending_show(card_t *c, int i, int k, int m)
{
    buffer_t iconsys = {0}, ico = {0};
    icon_t *ic = NULL, *old = sending.icon;
    if (mcfs_save_icon(c->path, &sv[i], &iconsys, &ico) == 0)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    memset(&sending, 0, sizeof(sending));
    sending.s = sv[i];
    sending.card = c;
    sending.icon = ic;
    sending.tried = 1;
    save_title(&sending, ic);
    ui_unlock();
    save_sending(c, &sending, k, m);
    if (old)
        icon_free(old);   /* (the screen shows the new one already) */
#ifdef DEBUG_BUILD
    if (k * 2 > m)
        debug_capture_if('v');   /* (script: the screen of a save on its way, past the first half) */
#endif
}

/* the saves that changed since they were last sent (each one's signature, as the state has it). One that is the same
 * but lists something else now (its dates, written again with nothing changed) keeps what it lists, to be read no more */
static int changed(const card_t *c, int n, unsigned char *go)
{
    char hex[17];
    int i, m = 0;
    for (i = 0; i < n; i++) {
        snprintf(hex, sizeof(hex), "%016llx", svSig[i]);
        go[i] = goes(c, i);
        if (svSig[i] && !go[i] && svStamp[i][0] && strcmp(state_save_stamp(owner_of(c), sv[i].folder), svStamp[i]) != 0)
            state_set_save(owner_of(c), sv[i].folder, hex, svStamp[i]);
        looked_at(c, i);
        m += go[i];
    }
    return m;
}

static int backup_saves(card_t *c, const char *folderId)
{
    static drive_file_t old[200];
    static unsigned char go[MCFS_MAX_SAVES];
    char savesId[80] = "", name[160], desc[200], tag[140], hex[17], id[80], cardId[80];
    datetime_t when;
    int n, i, k, m, sent = 0, r, shown = 0, pending;
    u64 start = now_ms();
    if ((n = mcfs_list_saves(c->path, sv, MCFS_MAX_SAVES, NULL)) <= 0)
        return n < 0 ? -1 : 0;
    for (i = k = 0; i < n; i++)   /* (SD2Cloud's own folder, the SAS package: nobody's save) */
        if (strcasecmp(sv[i].folder, helper_place() + 1) != 0)
            sv[k++] = sv[i];
    if (!(n = k))
        return 0;
    sync_saves_start(c, sv, n);   /* (the list SELECT shows: these saves now) */
    work_text(T(T_SAVES_CHECKING));   /* (each save read, to tell which changed: a bar for them) */
    upload_screen(0, n);
    r = signatures(c, n);
    work_text(NULL);
    if (r != 0)
        return -1;
    pending = changed(c, n, go);
    log_msg("%s: %d saves, read in %u ms; %d changed since they were last sent", c->id, n, (unsigned)(now_ms() - start), pending);
    for (i = 0; i < n && !sync_stop(); i++) {
        buffer_t psu = {0};
        if (!go[i])
            continue;
        snprintf(hex, sizeof(hex), "%016llx", svSig[i]);
        sync_saves_set(i, SY_SENDING);
        sending_show(c, i, ++shown, pending);
        save_tag(c, sv[i].folder, tag, sizeof(tag));
        r = 0;
        if (!DRY) {
            if (!folderId) {   /* (the card's folder on Drive, found or made once a save has to go) */
                r = card_folder(c, cardId, sizeof(cardId));
                log_msg("%s: its folder on Drive %s", c->id, r == 0 ? "found" : "not found");
                if (r != 0)
                    return -1;
                folderId = cardId;
            }
            /* (once for the card; the Library's, for each save's game) */
            if ((!savesId[0] || library_path(c->path)) && saves_folder(c, sv[i].folder, folderId, savesId, sizeof(savesId)) != 0)
                return -1;
            local_time(&when);
            snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02dm%02d.psu", sv[i].folder, when.year, when.month, when.day,
                     when.hour, when.minute, when.second);
            snprintf(desc, sizeof(desc), "%s (%s) - " APP_NAME " " APP_VERSION, sv[i].folder, c->base);
            r = mcfs_export_psu(c->path, sv[i].folder, &psu) == MCFS_OK
                    ? google_upload_buffer(savesId, name, desc, psu.data, psu.len, on_progress, id, sizeof(id), tag) : -1;
            if (r == 0 && cfg.keep && (m = google_list(savesId, tag, old, 200)) > cfg.keep)
                for (k = 0; k < m - cfg.keep; k++)   /* (the oldest first: the newest stay) */
                    google_delete(old[k].id);
        }
        buf_free(&psu);
        log_msg("save %s of %s, by itself to Drive: %d%s", sv[i].folder, c->id, r, DRY ? " (dry)" : "");
        sync_saves_set(i, r == 0 ? SY_SENT : sync_stop() ? SY_WAITING : SY_FAILED);
        if (r == 0) {
            state_set_save(owner_of(c), sv[i].folder, hex, svStamp[i][0] ? svStamp[i] : NULL);
            sent++;
        }
    }
    if (shown) {   /* (the card's screen again, before the save's icon is let go of) */
        save_sending_done();
        icon_free(sending.icon);
        memset(&sending, 0, sizeof(sending));
    }
    state_write();
    return sent;
}

int saves_backup(card_t *c, const char *folderId)
{
    int r = backup_saves(c, folderId);
    sync_saves_end();   /* (the list is the sync's cards again) */
    return r;
}

/* -------- "Versions on Drive", on a save's page: the ones the backup by save (or "Send to Drive") kept of it */

static buffer_t got;
static int to_buffer(const unsigned char *d, size_t n, void *u)
{
    (void)u;
    return buf_append(&got, d, n) ? 1 : 0;
}

void save_versions_screen(card_t *c, save_view_t *v)
{
    static drive_file_t list[200];
    static const char *items[200];
    static char text[200][48];
    char folderId[80], savesId[80], tag[140], t[300], when[40], name[100], fresh[200];
    const drive_file_t *f;
    int n, i, k, r;
    googleError[0] = 0;
    if ((r = ensure_google(1)) != 0) {
        if (r > 0) {
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(0, NULL, COLOR_ERROR, t);
        }
        return;
    }
    message(0, NULL, COLOR_TEXT, T(T_HIST_LOADING));
    save_tag(c, v->s.folder, tag, sizeof(tag));
    n = card_folder(c, folderId, sizeof(folderId)) == 0 && saves_folder(c, v->s.folder, folderId, savesId, sizeof(savesId)) == 0
            ? google_list(savesId, tag, list, 200) : -1;
    if (n < 0) {
        snprintf(t, sizeof(t), "%s %s", T(T_ERR_INTERNET), googleError);
        message_wait(0, NULL, COLOR_ERROR, t);
        return;
    }
    if (!n) {
        message_wait(0, NULL, COLOR_TEXT, T(T_SAVE_NO_VERSIONS));
        return;
    }
    for (i = 0; i < n; i++) {   /* (the newest first) */
        backup_when(c, &list[n - 1 - i], text[i], sizeof(text[0]));
        items[i] = text[i];
    }
    if ((k = choose(T(T_SAVE_VERSIONS), items, n, 0)) < 0)
        return;
    f = &list[n - 1 - k];
    snprintf(when, sizeof(when), "%s", text[k]);
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_SAVE_VERSION_ASK), name, when);
    if (!confirm(t, T(T_SAVE_VERSION_TEXT), T_RESTORE) || card_free(c, NULL))
        return;
    /* the version, downloaded; then in place of the save as it is now (which is kept aside, and put back when the
     * version can't go in) */
    message(0, NULL, COLOR_TEXT, T(T_RESTORE_STEP_DOWNLOAD));
    snprintf(fresh, sizeof(fresh), "%ssave-version.psu", dataDir);
    buf_free(&got);
    r = google_download(f->id, to_buffer, NULL) == 0 && file_write(fresh, got.data, got.len) == 0 ? 0 : -1;
    buf_free(&got);
    if (r == 0) {
        message(0, NULL, COLOR_TEXT, T(T_RESTORE_STEP_WRITE));
        r = mcfs_replace_save(NULL, fresh, v->s.folder, c->path, NULL);
    }
    unlink(fresh);
    log_msg("save %s of %s: version %s put back: %d", v->s.folder, c->id, f->name, r);
    if (r == 0 && mcfs_slot_port(c->path) < 0 && !card_file_card(c))
        cards_recheck(c);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_SAVE_VERSION_DONE : T_SAVE_VERSION_FAILED));
}
