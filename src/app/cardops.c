/* SD2Cloud -- a card as a whole, from its options (△ on the main screen): made again of another size; deleted, or
 * its whole folder deleted. */
#include "app.h"

static void cards_gone(const char *active);

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
    if (!(mb = card_size_pick(T_RESIZE_TITLE, used, now, 0)) || mb == now)
        return;
    snprintf(t, sizeof(t), T(T_RESIZE_ASK), c->base, now, mb);
    dlg_new(COLOR_TITLE, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_RESIZE_TEXT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_CONTINUE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return;
    }
    sound_play(SND_CONFIRM);
    if (card_free(c, NULL))
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
int card_rename_screen(card_t *c)
{
    char name[48], t[300], active[96], old[48];
    int i, r;
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
    int i, others = 0, r;
    snprintf(t, sizeof(t), T(T_DELETE_CARD_ASK), c->base);
    dlg_new(COLOR_WARN, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_DELETE_CARD_TEXT));
    backup_line(c);
    if (!ask_delete() || card_free(c, NULL))
        return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    snprintf(folder, sizeof(folder), "%s", c->folder);
    for (i = 0; i < nCards; i++)
        others += &cards[i] != c && !strcasecmp(cards[i].folder, folder);
    r = cards_delete(c);
    if (r == 0 && !others)   /* (nothing of it is left to keep: its .ini, its pictures, the folder itself) */
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
            card_state_t *e = state_card(cards[i].id, 0);
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
    if (!ask_delete())
        return 0;
    for (i = 0; i < n; i++)   /* (the device is on one of them, at most: that one is let go of) */
        if (card_free(own[i], NULL))
            return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    r = cards_delete_folder(folder);
    cards_gone(active);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_DONE_DELETE_FOLDER : T_DELETE_FOLDER_FAILED));
    return 1;
}
