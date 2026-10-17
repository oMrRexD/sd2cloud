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

/* whose saves they are, for the state and on Drive: the Memory Card is one, whichever slot it is in */
static const char *owner_of(const card_t *c) { return mcfs_slot_port(c->path) >= 0 ? memcard_id() : c->id; }

/* what a save's versions are tagged with on Drive (google_list lists them by it) */
static void save_tag(const card_t *c, const char *folder, char *out, size_t size)
{
    snprintf(out, size, "save:%.80s/%s", owner_of(c), folder);
}

/* each save's signature: the engine's, which reads the card's file; a Memory Card's, from its .psu (mcman reads it:
 * the engine doesn't read its files one by one) */
static int signatures(const card_t *c, int n)
{
    int i;
    if (mcfs_slot_port(c->path) < 0)
        return mcfs_save_signatures(c->path, sv, n, svSig);
    for (i = 0; i < n; i++) {
        buffer_t psu = {0};
        unsigned long long h = 1469598103934665603ULL;   /* (FNV-1a, 64 bits) */
        size_t k;
        svSig[i] = 0;
        if (mcfs_export_psu(c->path, sv[i].folder, &psu) == MCFS_OK) {
            for (k = 0; k < psu.len; k++)
                h = (h ^ psu.data[k]) * 1099511628211ULL;
            svSig[i] = h ? h : 1;
        }
        buf_free(&psu);
    }
    return 0;
}

/* the card's Saves folder on Drive (folderId = the card's folder), found or made */
static int saves_folder(const card_t *c, const char *folderId, char *savesId, size_t size)
{
    char key[200];
    const char *cache;
    snprintf(key, sizeof(key), "%s/%s/Saves", cfg.drive_folder, c->folder);
    savesId[0] = 0;
    if ((cache = state_folder(key)) != NULL)
        snprintf(savesId, size, "%s", cache);
    if (google_folder("Saves", folderId, savesId, size) != 0)
        return -1;
    state_set_folder(key, savesId);
    return 0;
}

int saves_backup(card_t *c, const char *folderId)
{
    static drive_file_t old[200];
    char savesId[80] = "", name[160], desc[200], tag[140], hex[17], id[80];
    const char *was;
    datetime_t when;
    int n, i, k, m, sent = 0, r;
    if ((n = mcfs_list_saves(c->path, sv, MCFS_MAX_SAVES, NULL)) <= 0)
        return n < 0 ? -1 : 0;
    if (signatures(c, n) != 0)
        return -1;
    for (i = 0; i < n && !backupCancelled; i++) {
        buffer_t psu = {0};
        if (!svSig[i])
            continue;   /* (it couldn't be read: as it was) */
        snprintf(hex, sizeof(hex), "%016llx", svSig[i]);
        if ((was = state_save(owner_of(c), sv[i].folder)) != NULL && !strcmp(was, hex))
            continue;   /* (as it was last sent) */
        save_tag(c, sv[i].folder, tag, sizeof(tag));
        r = 0;
        if (!DRY) {
            if (!savesId[0] && saves_folder(c, folderId, savesId, sizeof(savesId)) != 0)
                return -1;
            local_time(&when);
            snprintf(name, sizeof(name), "%s %04d-%02d-%02d %02dh%02dm%02d.psu", sv[i].folder, when.year, when.month, when.day,
                     when.hour, when.minute, when.second);
            snprintf(desc, sizeof(desc), "%s (%s) - " APP_NAME " " APP_VERSION, sv[i].folder, c->base);
            r = mcfs_export_psu(c->path, sv[i].folder, &psu) == MCFS_OK
                    ? google_upload_buffer(savesId, name, desc, psu.data, psu.len, NULL, id, sizeof(id), tag) : -1;
            if (r == 0 && cfg.keep && (m = google_list(savesId, tag, old, 200)) > cfg.keep)
                for (k = 0; k < m - cfg.keep; k++)   /* (the oldest first: the newest stay) */
                    google_delete(old[k].id);
        }
        buf_free(&psu);
        log_msg("save %s of %s, by itself to Drive: %d%s", sv[i].folder, c->id, r, DRY ? " (dry)" : "");
        if (r == 0) {
            state_set_save(owner_of(c), sv[i].folder, hex);
            sent++;
        }
    }
    state_write();
    return sent;
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
    char folderId[80], savesId[80], tag[140], t[300], when[40], name[100], fresh[200], before[200];
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
    n = card_folder(c, folderId, sizeof(folderId)) == 0 && saves_folder(c, folderId, savesId, sizeof(savesId)) == 0
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
    /* the version, downloaded; the save as it is now, kept beside it; then the one in place of the other (and the save
     * as it was put back, when the version can't go in) */
    message(0, NULL, COLOR_TEXT, T(T_RESTORE_STEP_DOWNLOAD));
    snprintf(fresh, sizeof(fresh), "%ssave-version.psu", dataDir);
    snprintf(before, sizeof(before), "%ssave-before.psu", dataDir);
    buf_free(&got);
    r = google_download(f->id, to_buffer, NULL) == 0 && file_write(fresh, got.data, got.len) == 0 ? 0 : -1;
    buf_free(&got);
    if (r == 0) {
        buffer_t now = {0};
        message(0, NULL, COLOR_TEXT, T(T_RESTORE_STEP_WRITE));
        r = mcfs_export_psu(c->path, v->s.folder, &now) == MCFS_OK && file_write(before, now.data, now.len) == 0 ? 0 : -1;
        buf_free(&now);
        if (r == 0 && (r = mcfs_delete_save(c->path, v->s.folder)) == MCFS_OK &&
            (r = mcfs_import_psu(fresh, c->path, NULL)) != MCFS_OK)
            log_msg("save version: not put in (%d); the save as it was, back: %d", r, mcfs_import_psu(before, c->path, NULL));
    }
    unlink(fresh);
    unlink(before);
    log_msg("save %s of %s: version %s put back: %d", v->s.folder, c->id, f->name, r);
    if (r == 0 && mcfs_slot_port(c->path) < 0 && !card_file_card(c))
        cards_recheck(c);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_SAVE_VERSION_DONE : T_SAVE_VERSION_FAILED));
}
