/* SD2Cloud -- one save: its page, and what is done with it (marked to be copied, moved to another card, deleted, sent
 * to Drive, started when it is a program). */
#include "app.h"

/* "Start application", on a save that is a program: it runs from the memory card slot, in its own folder, as when it
 * is started from the PS2 browser. The device has to be on that card for it: it is told to take it when it isn't */
static void start_app(card_t *c, const char *folder, const char *boot)
{
    char path[160];
    find_active();
    if (!(activeCard >= 0 && &cards[activeCard] == c)) {
        if (can_insert(c))
            message(0, NULL, COLOR_TEXT, T(T_SWITCHING));
        if (insert_card(c) != 0) {
            insert_failed(c);
            return;
        }
    }
    snprintf(path, sizeof(path), "mc%c:/%s/%s", strncmp(sdRoot, "mmce", 4) ? '0' : sdRoot[4], folder, boot);
    log_msg("starting %s of %s", path, c->id);
    leave(path);
}

/* -------- a save's page, like the PS2 browser's: the icon big on the left; the card, the name, the date and the
 * size on the right, and what can be done with it. Also the page of a save still in a .psu file, which has the
 * file's name where the card's would be */

static const int saveOptions[] = {T_COPY, T_MOVE, T_DELETE, T_TO_CLOUD};
static const int appOptions[] = {T_START_APP, T_COPY, T_MOVE, T_DELETE, T_TO_CLOUD};   /* a save that is a program */
const int psuOptions[] = {T_IMPORT};
static struct {
    save_view_t *v;
    char where[160];          /* the card the save is on, or the .psu file it is in */
    const int *options;
    int nOptions;
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
    if (v->icon || (v->tried && cubeIcon))
        icon_draw(v->icon ? v->icon : cubeIcon, 190, 232, 214, (now_ms() - sp.since) / 1000.0f);
    text_center_shadow(FONT_TEXT, cx, y, 0xE8E8EC, sp.where);
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
    for (i = 0, y = 232; i < sp.nOptions; i++, y += sp.nOptions > 4 ? 28 : 31) {
        const char *s = T(sp.options[i]);
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

void op_result(int r, int okText, const card_t *other)
{
    char t[300];
    if (r == MCFS_OK)
        snprintf(t, sizeof(t), T(okText), other ? other->base : "");
    else
        snprintf(t, sizeof(t), T(r == MCFS_ERR_EXISTS ? T_ERR_EXISTS : r == MCFS_ERR_FULL ? T_ERR_FULL
                                 : r == MCFS_ERR_CHECK ? T_ERR_MC_CHECK : r == MCFS_ERR_BAD ? T_ERR_PSU : T_ERR_MC_WRITE),
                 other ? other->base : "");
    message_wait(0, NULL, r == MCFS_OK ? COLOR_OK : COLOR_ERROR, t);
}

/* the save's name as the questions show it: its icon.sys title, or the folder when it has none */
void save_name(const save_view_t *v, char *out, size_t size)
{
    if (v->line1[0] || v->line2[0])
        snprintf(out, size, "%s%s%s", v->line1, v->line1[0] && v->line2[0] ? " " : "", v->line2);
    else
        snprintf(out, size, "%s", v->s.folder);
}

/* move the save to another card. 1 = it went: the card it was on changed */
static int save_move(card_t *c, save_view_t *v)
{
    card_t *to;
    char game[12], name[100], t[200];
    long long bytes = 0;
    int files = 0, r;
    save_game_id(v->s.folder, game);   /* (a save that tells its game can always go to a new card of that game) */
    if (nCards < 2 && !game[0]) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_OTHER_CARDS));
        return 0;
    }
    mcfs_save_info(c->path, v->s.folder, &bytes, &files);
    if (!(to = choose_dest(c, T_MOVE_TO, bytes, v->s.folder)))
        return 0;
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_CONFIRM_MOVE), name, to->base);
    if (!confirm(t, to == &destNew ? T(T_NEWCARD_NOTE) : NULL, T_MOVE) || !(to = dest_real(to)))
        return 0;
    if (card_free(to, c) || card_free(c, to)) {
        card_back();
        return 0;
    }
    save_into(to, v, T_WORKING_MOVE);
    r = mcfs_copy_save(c->path, v->s.folder, to->path, save_progress);
    if (r == MCFS_OK) {
        save_into_fixed();
        r = mcfs_delete_save(c->path, v->s.folder);
    }
    save_into_done();
    log_msg("move %s from %s to %s: %d", v->s.folder, c->id, to->id, r);
    if (r == MCFS_OK)
        transferDest = to;
    cards_recheck(to);
    cards_recheck(c);
    card_back();
    if (r == MCFS_ERR_CANCELLED)
        return 0;   /* given up, and said so already: back to the save's page */
    op_result(r, T_DONE_MOVE, to);
    return r == MCFS_OK;
}

/* 1 = deleted */
static int save_delete(card_t *c, save_view_t *v)
{
    char name[100], t[300];
    int r;
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
    if (card_free(c, NULL))
        return 0;
    message(0, NULL, COLOR_TEXT, T(T_WORKING_DELETE));
    r = mcfs_delete_save(c->path, v->s.folder);
    log_msg("delete %s from %s: %d", v->s.folder, c->id, r);
    cards_recheck(c);
    card_back();
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

/* what the page shows: the save, where it is, its size and what can be done with it */
void save_page(save_view_t *v, const char *where, long long bytes, const int *options, int n)
{
    unsigned long long w = v->s.when;
    datetime_t d;
    card_time_local((int)(w >> 40), (w >> 32) & 0xFF, (w >> 24) & 0xFF, (w >> 16) & 0xFF, (w >> 8) & 0xFF, w & 0xFF, &d);
    ui_lock();
    sp.v = v;
    snprintf(sp.where, sizeof(sp.where), "%s", where);
    fit(FONT_TEXT, sp.where, sizeof(sp.where), 304);
    sp.options = options;
    sp.nOptions = n;
    if (i18n_is_pt())
        snprintf(sp.date, sizeof(sp.date), "%02d/%02d/%04d  %02d:%02d:%02d", d.day, d.month, d.year, d.hour, d.minute, d.second);
    else
        snprintf(sp.date, sizeof(sp.date), "%04d-%02d-%02d  %02d:%02d:%02d", d.year, d.month, d.day, d.hour, d.minute, d.second);
    snprintf(sp.size, sizeof(sp.size), T(T_SAVE_KB), (int)((bytes + 1023) / 1024));
    sp.cursor = 0;
    sp.since = now_ms();
    ui_unlock();
}

/* the page until something is chosen: the option's text id, or 0 (circle) */
int save_page_choice(void)
{
    for (;;) {
        u32 b;
        ui_scene(scene_save);
        b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return 0;
        }
        if (b & PAD_CROSS) {
            sound_play(SND_CONFIRM);
            return sp.options[sp.cursor];
        }
        if (sp.nOptions > 1) {
            ui_lock();
            sp.cursor = (b & PAD_UP) ? (sp.cursor + sp.nOptions - 1) % sp.nOptions : (sp.cursor + 1) % sp.nOptions;
            ui_unlock();
            sound_play(SND_MOVE);
        }
    }
}

/* 1 = the card changed (the list has to be read again) */
int save_screen(card_t *c, int i)
{
    save_view_t *v = &brw.saves[i];
    char boot[40];
    long long bytes = 0;
    int files = 0, k;
    mcfs_save_info(c->path, v->s.folder, &bytes, &files);
    if (c == &fileCard)
        save_page(v, c->base, bytes, saveOptions, 1);   /* a card file is only read: a save can be copied out of it */
    else if (mcfs_save_app(c->path, &v->s, boot, sizeof(boot)))
        save_page(v, c->base, bytes, appOptions, 5);
    else
        save_page(v, c->base, bytes, saveOptions, 4);
    while ((k = save_page_choice()) != 0) {
        switch (k) {
        case T_START_APP:
            start_app(c, v->s.folder, boot);
            break;
        case T_COPY:   /* marked (marks.c): the screen of saves goes on from here, to mark others and say where to */
            if (copy_start(v))
                return 0;
            break;
        case T_MOVE:
            if (save_move(c, v))
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
    return 0;
}

/* A card the device no longer opens for its game (Game2Folder.ini gives the game another folder): its saves go to
 * the card of that folder, on the same channel, which is made now if it isn't there. A save that card already has
 * of the same name stays where it is. How many went is said at the end */
void card_move_saves(card_t *c)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    char name[64], t[300], note[200];
    card_t *to;
    int i, n, moved = 0, r;
    snprintf(name, sizeof(name), "%.44s-%d", c->moved, c->channel);
    snprintf(t, sizeof(t), T(T_MOVE_SAVES_ASK), c->base, name);
    snprintf(note, sizeof(note), T(T_MOVE_SAVES_NOTE), c->moved);
    if (!confirm(t, note, T_MOVE))
        return;
    if ((n = mcfs_list_saves(c->path, list, MCFS_MAX_SAVES, NULL)) < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    if (!(to = dest_card(c->moved, c->channel)))
        return;
    if (card_free(to, c) || card_free(c, to)) {
        card_back();
        return;
    }
    card_work(c, 1, T(T_WORKING_MOVE), 1);
    for (i = 0; i < n; i++) {
        upload_screen(i, n);
        if ((r = mcfs_copy_save(c->path, list[i].folder, to->path, NULL)) == MCFS_OK)
            r = mcfs_delete_save(c->path, list[i].folder);
        log_msg("move %s from %s to %s: %d", list[i].folder, c->id, to->id, r);
        moved += r == MCFS_OK;
        if (r == MCFS_ERR_FULL)   /* (no room for the next ones either) */
            break;
    }
    upload_screen(n, n);
    card_work_done();
    cards_recheck(to);
    cards_recheck(c);
    card_back();
    snprintf(t, sizeof(t), T(T_MOVED_N), moved);
    dlg_new(moved < n ? COLOR_WARN : COLOR_OK, t);
    if (moved < n) {
        snprintf(note, sizeof(note), T(T_NOT_MOVED_N), n - moved);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, note);
    }
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}
