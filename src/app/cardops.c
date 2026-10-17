/* SD2Cloud -- a card as a whole, from its options (△ on the main screen): made again of another size; deleted, or
 * its whole folder deleted. */
#include "app.h"

static void cards_gone(const char *active);
static int name_free(const char *name);

/* ------------------------------------------------------------ another size */

/* the bar of a card made again (card_resize): the new card made, 20%; its folders copied, 50%; and, when it can't be
 * renamed into place, written over the card, the rest. Circle gives up while the folders are copied (the card is
 * then as it was) */
static int resize_progress(int phase, long long done, long long total)
{
    int at, text;
    if (phase == RESTORE_LOST)
        return 1;   /* (the new card, read from the microSD: when it can't be, it is kept as it is, and said) */
    if (phase == RESIZE_COPY) {
        watch_cancel();
        if (cancelLatched && confirm_cancel(T_WORK_CANCEL_TITLE, T_RESTORE_CANCEL_TEXT))
            return 1;
    }
    if (done > total)
        done = total;
    at = phase == RESIZE_MAKE ? 0 : phase == RESIZE_COPY ? 200 : phase == RESTORE_DOWNLOAD ? 700 : phase == RESTORE_WRITE ? 750 : 950;
    at += total ? (int)((phase == RESIZE_MAKE ? 200 : phase == RESIZE_COPY ? 500 : phase == RESTORE_DOWNLOAD ? 50
                                               : phase == RESTORE_WRITE ? 200 : 50) * done / total) : 0;
    text = phase == RESIZE_MAKE ? T_RESIZE_MAKING : phase == RESIZE_COPY ? T_RESIZE_COPYING : T_RESTORE_STEP_WRITE;
    ui_lock();
    workText = T(text);
    workFixed = phase != RESIZE_COPY;
    ui_unlock();
    upload_screen(at, 1000);
#ifdef DEBUG_BUILD
    if (phase == RESIZE_COPY && total && done * 2 >= total)
        debug_capture_if('R');
#endif
    return 0;
}

/* The card made again of another size, picked among the ones its folders fit in (the one it is, marked), asked
 * first. The sd2psx is moved off it while that is done, and back to it after. Its backup status stays as it was: what
 * it has is the same */
void card_resize_screen(card_t *c)
{
    char t[300];
    long long used = mcfs_card_used(c->path);
    int now = (int)(c->size >> 20), mb, r, synced = c->status == ST_UP_TO_DATE || c->status == ST_NO_BACKUP,
        resolved = c->resolved;
    if (used < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    if (c->type == TYPE_VMC)   /* (a VMC: the sizes the OPL makes) */
        mb = card_size_pick_in(T_RESIZE_TITLE, used, now, 0, 8, 64);
    else
        mb = card_size_pick(T_RESIZE_TITLE, used, now, 0);
    if (!mb || mb == now)
        return;
    snprintf(t, sizeof(t), T(T_RESIZE_ASK), c->base, now, mb);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 6, T(T_RESIZE_TEXT));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_SAFETY_NOTE));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_CONTINUE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return;
    }
    sound_play(SND_CONFIRM);
    if (card_free(c, NULL) || safety_copy(c) != 0)
        return;
    card_work(c, 1, T(T_RESIZE_MAKING), 1);
    cancelLatched = 0;
    googleError[0] = 0;
    r = card_resize(c, (long long)mb << 20, resize_progress);
    card_work_done();
    card_back();
    if (r != -3) {
        cards_recheck(c);
        if (r == 0)   /* (its saves are the very same: no backup is needed for it) */
            card_keep_synced(c, synced, resolved);
    }
    if (r == -2)
        return;
    if (r == 0)
        snprintf(t, sizeof(t), T(T_RESIZE_DONE), c->base, mb);
    else if (r == -3) {   /* (its file and ".new": what to install it from, in the Files group) */
        char file[80];
        snprintf(file, sizeof(file), "%s%s.new", c->base, dev->ext);
        snprintf(t, sizeof(t), T(T_RESIZE_LEFT), c->base, file);
    }
    else
        snprintf(t, sizeof(t), "%s", googleError[0] ? googleError : T(T_ERR_MC_WRITE));
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, t);
}

/* ------------------------------------------------------------ a new card
 *
 * Made empty from the main screen ("New card" in the Cards group), or with saves in it: the ones marked for a copy,
 * or every one "All saves" shows. It goes into a group of the device (the numbered cards, a folder of the user's
 * own, or a new one), or is a file of a folder of the microSD or of a USB drive, in the format of whatever is to open
 * it. Its size is picked, the one recommended for its saves first. */

/* the formats a new card can be a file in: the sd2psx's, a MemCard PRO2's, PCSX2's (with the ECC bytes of the card's
 * flash), OPL's (a VMC); each with the sizes the one that opens it takes */
static const struct {
    const char *ext, *name;
    int ecc, minMb, maxMb;
} formats[] = {{".mcd", ".mcd (sd2psx)", 0, 1, 64}, {".mc2", ".mc2 (MemCard PRO2)", 0, 8, 128},
               {".ps2", ".ps2 (PCSX2)", 1, 8, 64}, {".bin", ".bin (OPL)", 0, 8, 64}};
#define FORMATS ((int)(sizeof(formats) / sizeof(formats[0])))

static struct {
    const save_ref_t *saves;   /* what goes into it (n = 0: it is made empty) */
    int n;
    long long room;            /* what they take of a card, at most (mcfs_save_room) */
    int made;                  /* a card was made (as a file, from the folders) */
} nc;

static void made_progress(long long done, long long total) { upload_screen(done, total); }
static int written_progress(long long done, long long total)
{
    upload_screen(done, total);
    return 0;
}

/* The card made at path (size, as a .mcd; ecc = a .ps2, made as a .mcd in the data folder first and written with
 * its ECC), with the saves copied into it, each read back. name = what the screen calls it. How many saves went in
 * (failed = how many didn't), or -1 = it couldn't be made (nothing is left at path) */
static int make_card(const char *path, const char *name, long long size, int ecc, int *failed)
{
    card_t shown;
    char tmp[260];
    const char *at = path;
    int i, r, ok = 0;
    memset(&shown, 0, sizeof(shown));
    snprintf(shown.base, sizeof(shown.base), "%.55s", name);
    snprintf(shown.path, sizeof(shown.path), "%s", path);
    shown.type = TYPE_NAMED;
    if (ecc) {
        snprintf(tmp, sizeof(tmp), "%snew-card.tmp", dataDir);
        unlink(tmp);
        at = tmp;
    }
    *failed = 0;
    card_work(&shown, 0, T(T_NEWCARD_MAKING), 1);
    r = mcfs_new_card(at, size, made_progress);
    for (i = 0; r == MCFS_OK && i < nc.n; i++) {
        ui_lock();
        workText = T(T_RESIZE_COPYING);
        ui_unlock();
        upload_screen(i, nc.n);
        if (mcfs_copy_save(nc.saves[i].card->path, nc.saves[i].folder, at, NULL) == MCFS_OK)
            ok++;
        else
            (*failed)++;
        log_msg("new card: %s from %s", nc.saves[i].folder, nc.saves[i].card->id);
    }
    if (r == MCFS_OK && ecc) {
        ui_lock();
        workText = T(T_RESTORE_STEP_WRITE);
        ui_unlock();
        r = card_write_file(tmp, path, 1, written_progress) == 0 ? MCFS_OK : MCFS_ERR_IO;
        unlink(tmp);
    }
    card_work_done();
    log_msg("new card %s of %lld MB: %d, %d save(s) in, %d not", path, size >> 20, r, ok, *failed);
    if (r != MCFS_OK) {
        unlink(path);
        return -1;
    }
    return ok;
}

/* how it went: made, with how many of the saves */
static void made_report(const char *name, int ok, int failed)
{
    char t[200];
    if (ok < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_NEWCARD_FAILED));
        return;
    }
    snprintf(t, sizeof(t), T(T_CREATE_DONE), name);
    dlg_new(failed ? COLOR_WARN : COLOR_OK, t);
    if (nc.n) {
        snprintf(t, sizeof(t), T(T_TPL_COPIED_N), ok);
        dlg_line(FONT_TEXT, COLOR_TEXT, 4, t);
    }
    if (failed) {
        snprintf(t, sizeof(t), T(T_TPL_NOT_COPIED), failed);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
    }
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* asked before it is made: title (a card's or a file's), its name, its size, and what goes into it. 1 = yes */
static int ask_make(int title, const char *name, int mb, const char *where)
{
    char t[300];
    snprintf(t, sizeof(t), T(title), name, mb);
    dlg_new(COLOR_TITLE, t);
    if (where)
        dlg_line(FONT_SMALL, COLOR_DIM, 6, where);
    if (nc.n) {
        snprintf(t, sizeof(t), T(T_CREATE_SAVES), nc.n);
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    }
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_TPL_CREATE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    return 1;
}

/* △ in a folder of the Files group: the new card made there, as a file. Its format, its size and its name are
 * picked. 1 = it was made, or couldn't be (said): the folders are left */
static int create_here(const char *dir)
{
    const char *names[FORMATS];
    char name[48], file[64], path[700];
    int f, i, mb, ok, failed;
    for (i = 0; i < FORMATS; i++)
        names[i] = formats[i].name;
    if ((f = choose(T(T_CREATE_FORMAT), names, FORMATS, dev->sd2psx ? 0 : 1)) < 0)
        return 0;
    if (!(mb = card_size_pick_in(T_SIZE_TITLE, nc.room, 0, 0, formats[f].minMb, formats[f].maxMb)))
        return 0;
    snprintf(name, sizeof(name), "MemoryCard");
    for (;;) {   /* (a name a file of the folder has: said, and typed again) */
        if (!keyboard(T(T_FILE_NAME_TITLE), name, 33))
            return 0;
        snprintf(file, sizeof(file), "%s%s", name, formats[f].ext);
        snprintf(path, sizeof(path), "%s%s", dir, file);
        if (!file_exists(path))
            break;
        message_wait(0, NULL, COLOR_WARN, T(T_RENAME_EXISTS));
    }
    if (!ask_make(T_CREATE_FILE_ASK, file, mb, fb.title))
        return 0;
    ok = make_card(path, file, (long long)mb << 20, formats[f].ecc, &failed);
    made_report(file, ok, failed);
    nc.made = ok >= 0;
    return 1;
}

/* the cards of the device's own new card: in that folder, the next channel (base), made at its place */
static int make_device_card(const char *folder, const char *base)
{
    char dir[200], path[260], file[80];
    card_t *c;
    int mb, ok, failed;
    if (!(mb = card_size_pick(T_SIZE_TITLE, nc.room, 0, 0)) || !ask_make(T_CREATE_ASK, base, mb, NULL))
        return 0;
    snprintf(dir, sizeof(dir), "%s%s/%s/", sdRoot, dev->cards, folder);
    ensure_dir(dir);
    snprintf(file, sizeof(file), "%s%s", base, dev->ext);
    snprintf(path, sizeof(path), "%s%s", dir, file);
    ok = make_card(path, base, (long long)mb << 20, 0, &failed);
    if (ok >= 0 && (c = cards_append(folder, file)) != NULL) {
        cards_recheck(c);
        cardsUnsorted = 1;
    } else if (ok < 0) {   /* (a folder made for it goes too) */
        dir[strlen(dir) - 1] = 0;
        rmdir(dir);
    }
    made_report(base, ok, failed);
    return ok >= 0;
}

/* the saves that go into the card to be made, and the room they take */
static void nc_saves(const save_ref_t *saves, int n)
{
    long long bytes;
    int i, files;
    nc.saves = saves;
    nc.n = n;
    nc.made = 0;
    for (nc.room = 0, i = 0; i < n; i++) {
        bytes = 0, files = 0;
        mcfs_save_info(saves[i].card->path, saves[i].folder, &bytes, &files);
        nc.room += mcfs_save_room(bytes, files);
    }
}

int vmc_create(const save_ref_t *saves, int n)
{
    char name[56], path[200];
    card_t *c;
    int mb, ok, failed, i;
    nc_saves(saves, n);
    for (i = 0; i < 100; i++) {   /* (offered: generic_0, as the OPL calls its own, or the first number free) */
        snprintf(name, sizeof(name), "generic_%d", i);
        snprintf(path, sizeof(path), "%sVMC/%s.bin", sdRoot, name);
        if (!file_exists(path))
            break;
    }
    if (!(mb = card_size_pick_in(T_SIZE_TITLE, nc.room, 0, 0, 8, 64)))   /* (the sizes the OPL makes) */
        return 0;
    for (;;) {   /* (a name another VMC has: said, and typed again) */
        if (!keyboard(T(T_VMC_NAME_TITLE), name, 33) || !name[0])
            return 0;
        snprintf(path, sizeof(path), "%sVMC/%s.bin", sdRoot, name);
        if (!file_exists(path))
            break;
        message_wait(0, NULL, COLOR_WARN, T(T_RENAME_EXISTS));
    }
    if (!ask_make(T_CREATE_VMC_ASK, name, mb, NULL))
        return 0;
    ok = make_card(path, name, (long long)mb << 20, 0, &failed);
    if (ok >= 0 && (c = vmc_append(name)) != NULL) {
        cards_recheck(c);
        cardsUnsorted = 1;
    }
    made_report(name, ok, failed);
    return ok >= 0;
}

int card_create(const save_ref_t *saves, int n)
{
    enum { W_NUMBERED, W_GROUP, W_NEW_GROUP, W_FILE };
    static const int fileText[FDEVS] = {T_CREATE_FILE_SD, T_CREATE_FILE_USB, T_CREATE_FILE_HDD, T_CREATE_FILE_MX4SIO};
    static char text[40][100], folderOf[40][48], baseOf[40][56];
    const char *items[40];
    int kind[40], fileDev[40], devs[FDEVS], nDevs = fdev_list(devs), k = 0, i, j, pick;
    nc_saves(saves, n);
    /* where it can go: the next numbered card; the next channel of each folder of the user's own that has one free; a
     * new folder; a file */
    new_card(TAB_CARDS, "", 0, folderOf[k], baseOf[k]);
    snprintf(text[k], sizeof(text[0]), T(T_CREATE_NUMBERED), baseOf[k]);
    kind[k++] = W_NUMBERED;
    for (i = 0; i < nCards && k < 40 - 2 - FDEVS; i++) {
        if (cards[i].type != TYPE_NAMED)
            continue;
        for (j = 0; j < i && !(cards[j].type == TYPE_NAMED && !strcasecmp(cards[j].folder, cards[i].folder)); j++)
            ;
        if (j < i || !new_card(TAB_GAMES, cards[i].folder, max_channels(cards[i].folder), folderOf[k], baseOf[k]))
            continue;
        snprintf(text[k], sizeof(text[0]), T(T_CREATE_IN_GROUP), cards[i].folder, baseOf[k]);
        kind[k++] = W_GROUP;
    }
    if (dev->sd2psx) {
        snprintf(text[k], sizeof(text[0]), "%s", T(T_CREATE_NEW_GROUP));
        kind[k++] = W_NEW_GROUP;
    }
    for (i = 0; i < nDevs; i++) {   /* (a file, on each of the devices) */
        snprintf(text[k], sizeof(text[0]), "%s", T(fileText[devs[i]]));
        fileDev[k] = devs[i];
        kind[k++] = W_FILE;
    }
    for (i = 0; i < k; i++)
        items[i] = text[i];
    for (pick = 0;;) {   /* (back from what comes next: where it goes, again) */
        if ((pick = choose(T(T_CREATE_WHERE), items, k, pick)) < 0)
            return 0;
        if (kind[pick] == W_NUMBERED || kind[pick] == W_GROUP) {
            if (make_device_card(folderOf[pick], baseOf[pick]))
                return 1;
        } else if (kind[pick] == W_NEW_GROUP) {
            char name[48] = "", base[56];
            for (;;) {   /* (a name that can't be: said, and typed again) */
                if (!keyboard(T(T_FOLDER_NAME_TITLE), name, 16)) {
                    name[0] = 0;
                    break;
                }
                if (name_free(name))
                    break;
            }
            snprintf(base, sizeof(base), "%.44s-1", name);
            if (name[0] && make_device_card(name, base))
                return 1;
        } else {
            fbHere = create_here;
            fbHereText = T_CREATE_HERE;
            files_screen(fileDev[pick]);
            fbHere = NULL;
            if (nc.made)
                return 1;
        }
    }
}

/* ------------------------------------------------------------ another name */

/* A name typed for a folder of cards of the user's own: one the sd2psx takes (folder_renamable, 15 characters) and
 * that no folder has. 1 = it can be (said why not, otherwise) */
static int name_free(const char *name)
{
    char dir[200];
    static dir_entry_t one[1];
    if (!folder_renamable(name)) {
        message_wait(0, NULL, COLOR_WARN, T(T_RENAME_RESERVED));
        return 0;
    }
    snprintf(dir, sizeof(dir), "%s%s/%s/", sdRoot, dev->cards, name);
    if (dir_list(dir, one, 1) >= 0) {
        message_wait(0, NULL, COLOR_WARN, T(T_RENAME_EXISTS));
        return 0;
    }
    return 1;
}

/* △ "Rename" on a card. A card the sd2psx finds by its folder's name (a numbered one, a boot card, a game's) keeps
 * its file's: what changes is the name the sd2psx shows for it, in its folder's .ini. A folder of the user's own is
 * called something else, with its cards and files, asked first. 1 = cards[] changed (the main screen lists them
 * again) */
/* a VMC called something else: its file, and the settings of the OPL's games that use it. 1 = renamed */
static int vmc_rename_screen(card_t *c)
{
    char name[56], t[300];
    int r;
    snprintf(name, sizeof(name), "%s", c->base);
    if (!keyboard(T(T_VMC_NAME_TITLE), name, 33) || !name[0] || !strcmp(name, c->base))
        return 0;
    snprintf(t, sizeof(t), T(T_RENAME_VMC_ASK), c->base, name);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_RENAME_VMC_TEXT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_RENAME);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    message(0, NULL, COLOR_TEXT, T(T_RENAMING));
    r = vmc_rename(c, name);
    cardsUnsorted = 1;   /* (in its place among the others, by its new name) */
    if (r == 0)
        snprintf(t, sizeof(t), T(T_RENAME_DONE_VMC), name);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, r == 0 ? t : T(r == -2 ? T_RENAME_EXISTS : T_RENAME_FAILED));
    return r == 0;
}

int card_rename_screen(card_t *c)
{
    char name[48], t[300], active[96], old[48];
    int i, r;
    if (c->type == TYPE_VMC)
        return vmc_rename_screen(c);
    if (!folder_renamable(c->folder)) {
        snprintf(name, sizeof(name), "%s", c->name);
        if (!keyboard(T(T_CARD_NAME_TITLE), name, 33) || !strcmp(name, c->name))
            return 0;
        if (card_set_name(c, name) != 0)
            message_wait(0, NULL, COLOR_ERROR, T(T_RENAME_FAILED));
        return 0;
    }
    snprintf(name, sizeof(name), "%s", c->folder);
    for (;;) {   /* (a name that can't be: said, and typed again) */
        if (!keyboard(T(T_FOLDER_NAME_TITLE), name, 16) || !strcmp(name, c->folder))
            return 0;
        if (name_free(name))
            break;
    }
    snprintf(old, sizeof(old), "%s", c->folder);
    snprintf(t, sizeof(t), T(T_RENAME_FOLDER_ASK), old, name);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_RENAME_FOLDER_TEXT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_RENAME);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    for (i = 0; i < nCards; i++)   /* (the device is on one of its cards, at most: that one is let go of) */
        if (!strcasecmp(cards[i].folder, old) && card_free(&cards[i], NULL))
            return 0;
    message(0, NULL, COLOR_TEXT, T(T_RENAMING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    r = cards_rename_folder(old, name);
    if (r == 0 && active[0] && !strncasecmp(active, old, strlen(old)) && active[strlen(old)] == '/')
        snprintf(active, sizeof(active), "%s%s", name, strchr(active, '/'));   /* (the card in use is called so now) */
    card_back();   /* (the card it was moved off of is still there, under its new name) */
    cards_gone(active);
    if (r == 0)
        snprintf(t, sizeof(t), T(T_RENAME_DONE_FOLDER), name);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, r == 0 ? t : T(T_RENAME_FAILED));
    return 1;
}

/* ------------------------------------------------------------ checking */

void card_check_screen(card_t *c)
{
    mcfs_check_t r;
    char t[200];
    int ok;
    message(0, NULL, COLOR_TEXT, T(T_CHECKING));
    if (mcfs_check(c->path, &r) != 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    ok = mcfs_check_ok(&r);
    dlg_new(ok ? COLOR_OK : COLOR_WARN, T(ok ? T_CHECK_OK : T_CHECK_PROBLEMS));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 6, c->base);
    snprintf(t, sizeof(t), T(T_CHECK_CONTENTS), r.folders, r.files);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    snprintf(t, sizeof(t), T(T_CHECK_USED), r.used, r.clusters);
    dlg_line(FONT_TEXT, COLOR_TEXT, ok ? 0 : 6, t);
    if (r.badChains) {
        snprintf(t, sizeof(t), T(T_CHECK_CHAINS), r.badChains);
        dlg_line(FONT_TEXT, COLOR_WARN, 0, t);
    }
    if (r.crossLinked) {
        snprintf(t, sizeof(t), T(T_CHECK_CROSSED), r.crossLinked);
        dlg_line(FONT_TEXT, COLOR_WARN, 0, t);
    }
    if (r.lost) {
        snprintf(t, sizeof(t), T(T_CHECK_LOST), r.lost);
        dlg_line(FONT_TEXT, COLOR_WARN, 0, t);
    }
    if (r.badEntries) {
        snprintf(t, sizeof(t), T(T_CHECK_ENTRIES), r.badEntries);
        dlg_line(FONT_TEXT, COLOR_WARN, 0, t);
    }
    if (!ok)
        dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_CHECK_ADVICE));
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    next.wide = 1;
    dlg_show();
    sound_play(ok ? SND_EXIT : SND_BACK);
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* ------------------------------------------------------------ deleting */

/* After cards went out of cards[], the ones after them moved up: the card in use is found again by its name (gone
 * with them: none), and nothing else holds on to one. The main screen puts its list together again */
static void cards_gone(const char *active)
{
    for (activeCard = nCards - 1; activeCard >= 0 && strcmp(cards[activeCard].id, active); activeCard--)
        ;
    movedOff = NULL;   /* (the device stays where it was moved: the card it was on is no more) */
    transferDest = NULL;
    if (marks.n)
        marks_reset();
    cardsUnsorted = 1;
}

/* what is said under the question of a card that goes: when it was last backed up, or that it never was */
static void backup_line(const card_t *c)
{
    char when[40], t[200];
    last_backup(c, when, sizeof(when));
    if (when[0]) {
        snprintf(t, sizeof(t), T(T_DELETE_BACKUP), when);
        dlg_line(FONT_SMALL, COLOR_DIM, 0, t);
    } else
        dlg_line(FONT_SMALL, COLOR_WARN, 0, T(T_DELETE_NO_BACKUP));
}

/* the question made so far (dlg_new...), with its buttons: 1 = delete */
static int ask_delete(void)
{
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_DELETE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    return 1;
}

/* The card's file off the microSD, asked first. The card the device is using is let go of first (it is moved to
 * another one, which it stays on). When it was its folder's last card, the folder goes too, with its .ini. 1 = it
 * went (cards[] changed: the main screen lists them again) */
int card_delete(card_t *c)
{
    char t[300], active[96], folder[48];
    int i, others = 0, r, vmc = c->type == TYPE_VMC;
    snprintf(t, sizeof(t), T(T_DELETE_CARD_ASK), c->base);
    dlg_new(COLOR_WARN, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_DELETE_CARD_TEXT));
    backup_line(c);
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_SAFETY_NOTE));
    if (!ask_delete() || card_free(c, NULL) || safety_copy(c) != 0)
        return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    snprintf(folder, sizeof(folder), "%s", c->folder);
    for (i = 0; i < nCards; i++)
        others += &cards[i] != c && !strcasecmp(cards[i].folder, folder);
    r = cards_delete(c);
    if (r == 0 && !others && !vmc)   /* (nothing of it is left to keep: its .ini, its pictures, the folder itself) */
        cards_delete_folder(folder);
    cards_gone(active);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_DONE_DELETE_CARD : T_DELETE_CARD_FAILED));
    return 1;
}

/* the whole folder of that card, with every card of it, asked first. 1 = something went (as card_delete) */
int folder_delete(card_t *c)
{
    char t[300], active[96], folder[48];
    card_t *own[MAX_CARDS];
    int i, n = 0, never = 0, r;
    snprintf(folder, sizeof(folder), "%s", c->folder);
    for (i = 0; i < nCards; i++)
        if (!strcasecmp(cards[i].folder, folder)) {
            card_state_t *e = cardsOther ? NULL : state_card(cards[i].id, 0);
            own[n++] = &cards[i];
            never += !e || !e->sha[0];
        }
    snprintf(t, sizeof(t), T(T_DELETE_FOLDER_ASK), folder);
    dlg_new(COLOR_WARN, t);
    snprintf(t, sizeof(t), T(T_DELETE_FOLDER_TEXT), n);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, t);
    if (never) {
        snprintf(t, sizeof(t), T(T_DELETE_FOLDER_NO_BACKUP), never);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
    } else
        dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_DELETE_FOLDER_BACKUPS));
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_SAFETY_NOTE));
    if (!ask_delete())
        return 0;
    for (i = 0; i < n; i++)   /* (the device is on one of them, at most: that one is let go of) */
        if (card_free(own[i], NULL))
            return 0;
    for (i = 0; i < n; i++)   /* (a copy of each, first) */
        if (safety_copy(own[i]) != 0)
            return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    r = cards_delete_folder(folder);
    cards_gone(active);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_DONE_DELETE_FOLDER : T_DELETE_FOLDER_FAILED));
    return 1;
}
