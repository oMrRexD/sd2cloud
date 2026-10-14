/* SD2Cloud -- where a save goes: the card picked among the microSD's cards, or a new card made for it on the
 * spot. */
#include "app.h"

/* -------- where to copy or move a save to: the cards in the same groups as the main screen (without the card the save
 * is on), the destination big on the right with its free space. A card without room for the save can't be chosen */

struct dst_s dst;

/* "New card", picked as where a save goes (choose_dest): the card it will be, which is only made once the question
 * that follows is answered (dest_real) */
card_t destNew;

void scene_dest(float t)
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
    if (dst.g.tab == TAB_FILES && tabs_device(&dst.g) < 0) {   /* the Memory Card: its free space, as a card's */
        look_card(CARD_X, CARD_Y, NULL, T(T_MEMCARD));
        if ((f = dst.memFree) >= 0) {
            char l[64];
            snprintf(l, sizeof(l), T(T_FREE_KB), (int)(f / 1024));
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, f < dst.need ? COLOR_WARN : COLOR_DIM,
                           f < dst.need ? T(T_NO_ROOM) : l);
        }
        return;
    }
    if (!(c = tabs_card(&dst.g))) {
        if (dst.g.tab != TAB_FILES && dst.g.n && dst.g.idx[dst.g.cursor] == ROW_NEW) {   /* "New card": the card to be, and its name when it is known */
            look_card(CARD_X, CARD_Y, "+", NULL);
            ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_ITEM_ON, dst.newName);
        }
        return;
    }
    draw_card_picture(c, dst.iconCard == dst.g.idx[dst.g.cursor] ? dst.icon : NULL, ui_clock());
    if (tabs_on_folders(&dst.g))   /* a folder: nothing of one card to say yet */
        return;
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
void dest_info(void)
{
    card_t *c = tabs_card(&dst.g);
    icon_t *ic;
    int k;
    if (dst.g.tab == TAB_FILES && tabs_device(&dst.g) < 0 && dst.memFree == -2) {   /* (the Memory Card's) */
        long long f = -1;
        if (mcfs_list_saves(slotCard.path, NULL, 0, &f) < 0)
            f = -1;
        ui_lock();
        dst.memFree = f;
        ui_unlock();
    }
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

/* the game a save is of, by the ID in its folder's name (BASLUS-21065...). 1 = it has one, in id */
int save_game_id(const char *folder, char id[12])
{
    id[0] = 0;
    if (strlen(folder) < 12 || folder[0] != 'B' || !isupper((unsigned char)folder[1]))
        return 0;
    snprintf(id, 12, "%.10s", folder + 2);
    if (!is_game_id(id))
        id[0] = 0;
    return id[0] != 0;
}

/* the card picked (NULL = circle). from = a card to leave out, title = the text over the card, need = the save's size,
 * save = the save's folder (NULL = none): when its name tells the game, the Games group also offers a new card for
 * that game, and &destNew comes back when that is what was picked. again = back from the question about the card
 * picked, answered with circle: the cards are as they were left (the group, the folder that was open, the cursor) */
card_t *choose_dest(const card_t *from, int title, long long need, const char *save, int again)
{
    card_t *c = NULL;
    char id[12];
    int i;
    ui_lock();
    if (again)
        goto shown;
    tabs_init(&dst.g, from, 0);
    if (!dst.g.memcard && from != &slotCard && memcard_there()) {   /* (moved or installed into it too) */
        dst.g.memcard = 1;
        tabs_show(&dst.g, dst.g.tab);
    }
    dst.memFree = -2;
    if (save && save_game_id(save, id)) {
        game_folder(id, dst.g.newGame, sizeof(dst.g.newGame));
        /* (counted again: the Games group may be there only for this, and is then the one to open on) */
        tabs_show(&dst.g, dst.g.tab == TAB_FILES ? TAB_GAMES : dst.g.tab);
    }
    for (i = 0; i < MAX_CARDS; i++)
        dst.freeBytes[i] = -2;
shown:
    dst.title = T(title);
    dst.need = need;
    dst.icon = NULL;
    dst.iconCard = -1;
    dest_new_name();
    ui_unlock();
    ui_scene(scene_dest);
    for (;;) {
        u32 keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE, b = wait_nav_ms(keys, 250);
        if (!b) {
            dest_info();
            b = wait_nav(keys);
        }
        if (tabs_nav(&dst.g, b) || tabs_folder_nav(&dst.g, b)) {
            ui_lock();
            dest_new_name();
            ui_unlock();
            continue;
        }
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            break;
        }
        if ((b & PAD_CROSS) && dst.g.tab != TAB_FILES && dst.g.n && dst.g.idx[dst.g.cursor] == ROW_NEW) {   /* "New card" */
            sound_play(SND_CONFIRM);
            if (dest_new_place(dst.g.newGame)) {
                c = &destNew;
                break;
            }
            ui_scene(scene_dest);   /* back from a question: the cards again */
            continue;
        }
        if ((b & PAD_CROSS) && dst.g.tab == TAB_FILES && tabs_device(&dst.g) < 0) {   /* the Memory Card */
            if (dst.memFree == -2)
                dest_info();
            if (dst.memFree >= 0 && dst.memFree < dst.need) {   /* (no room: said on the screen) */
                sound_play(SND_BACK);
                continue;
            }
            sound_play(SND_CONFIRM);
            c = &slotCard;
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

/* is that card's file on the microSD? One that isn't among the cards here may still be there: past the cards SD2Cloud
 * handles, or made by the device itself since they were listed. A new card never takes the place of such a file */
static int card_file_there(const char *folder, const char *base)
{
    char path[200];
    snprintf(path, sizeof(path), "%s%s/%s/%s%s", sdRoot, dev->cards, folder, base, dev->ext);
    return file_exists(path);
}

/* The card a new one would be in a group: the lowest numbered card there is none of yet, on its first channel; or the
 * lowest channel that is free in a folder that has max of them (the BootCard's, or a game's: gameFolder).
 * 0 = that folder has them all */
int new_card(int tab, const char *gameFolder, int max, char folder[48], char base[56])
{
    int i, n;
    if (tab == TAB_CARDS) {
        for (n = 1;; n++) {
            snprintf(folder, 48, "%s%d", dev->numbered, n);
            snprintf(base, 56, "%s%d-1", dev->numbered, n);
            for (i = 0; i < nCards && strcasecmp(cards[i].folder, folder); i++)
                ;
            if (i == nCards && !card_file_there(folder, base))
                break;
        }
        return 1;
    }
    snprintf(folder, 48, "%s", tab == TAB_BOOT ? "BOOT" : gameFolder);
    for (n = 1; n <= max; n++) {
        for (i = 0; i < nCards && (strcasecmp(cards[i].folder, folder) || cards[i].channel != n); i++)
            ;
        snprintf(base, 56, "%.44s-%d", tab == TAB_BOOT ? "BootCard" : folder, n);
        if (i == nCards && !card_file_there(folder, base))
            return 1;
    }
    return 0;
}

/* the game folder a new card of the list shown goes to: the one that is open, or the one of the file's game ("" =
 * that is asked first) */
const char *dest_game_folder(void)
{
    if (dst.g.newGame[0])   /* a save is on its way: its own game's folder, whichever folder is open */
        return dst.g.newGame;
    return dst.g.tab == TAB_GAMES && dst.g.open >= 0 ? cards[dst.g.idx[1]].folder : fileGameFolder;
}

/* with the cursor on "New card": the card it would make, for the big card on the right (with ui_lock held) */
void dest_new_name(void)
{
    char folder[48];
    const char *game = dest_game_folder();
    dst.newName[0] = 0;
    /* (the next channel, even one past those the folder has: that is asked about when it is picked) */
    if (dst.g.n && dst.g.idx[dst.g.cursor] < 0 && (dst.g.tab != TAB_GAMES || game[0]) && !new_card(dst.g.tab, game, 255, folder, dst.newName))
        dst.newName[0] = 0;
}

/* every channel a folder has is taken, and the sd2psx can be told the folder has one more: asked. 1 = yes */
int ask_more_channels(int max)
{
    char t[300];
    snprintf(t, sizeof(t), T(T_INSTALL_RAISE_ASK), max, max + 1);
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_CONTINUE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    return 1;
}

static int destNewMore;    /* the channels its folder has to be told it has, for the new card to be in reach (0 = as it is) */
int cardsUnsorted;  /* a card was made and is at the end of cards[]: the list is put in order back on the main screen */

/* "New card" of a game picked as where saves go (game = its folder): where the card would be (destNew), the next free
 * channel of the game's folder. 0 = the folder has no place for one (said), or the user went back */
int dest_new_place(const char *game)
{
    char folder[48], base[56], t[300];
    int max = max_channels(game);
    destNewMore = 0;
    if (!new_card(TAB_GAMES, game, max, folder, base)) {
        if (!dev->sd2psx || max >= 255) {
            snprintf(t, sizeof(t), T(T_NEWCARD_NO_CHANNEL), max);
            message_wait(0, NULL, COLOR_WARN, t);
            return 0;
        }
        if (!ask_more_channels(max))
            return 0;
        destNewMore = max + 1;
        if (!new_card(TAB_GAMES, game, destNewMore, folder, base)) {   /* (that one's file is there already) */
            snprintf(t, sizeof(t), T(T_NEWCARD_NO_CHANNEL), destNewMore);
            message_wait(0, NULL, COLOR_WARN, t);
            return 0;
        }
    }
    memset(&destNew, 0, sizeof(destNew));
    snprintf(destNew.folder, sizeof(destNew.folder), "%s", folder);
    snprintf(destNew.base, sizeof(destNew.base), "%s", base);
    snprintf(destNew.id, sizeof(destNew.id), "%s/%s", folder, base);
    snprintf(destNew.path, sizeof(destNew.path), "%s%s/%s/%s%s", sdRoot, dev->cards, folder, base, dev->ext);
    destNew.type = TYPE_GAMEID;
    return 1;
}

/* ------------------------------------------------------------ a new card's size */

static struct {
    int mb[8];
    int device;       /* the size the device itself makes cards of is being picked */
    long long room;   /* what goes into the card (0 = nothing now) */
    char need[100];   /* "The saves take 340 KB." ("" = nothing goes into the card now) */
} sz;

/* beside the list of sizes: what the saves take and what the card would have left, and, on a size past the 8 MB of a
 * real card, what the sd2psx's firmware says of those: they lean on the microSD's speed, and a slow one can corrupt
 * them. For the size the sd2psx makes cards of: one under 8 MB it makes unformatted (its firmware's ps2_cardman.c, on
 * a device with PSRAM) */
static const char *size_note(int i)
{
    static char t[400];
    char left[24], rest[100] = "";
    const char *more = dev->sd2psx && sz.mb[i] > 8 ? T(T_SIZE_BIG) : sz.device && sz.mb[i] < 8 ? T(T_SIZE_UNFORMATTED) : "";
    if (sz.room) {
        format_size(mcfs_card_room((long long)sz.mb[i] << 20) - sz.room, left, sizeof(left));
        snprintf(rest, sizeof(rest), T(T_SIZE_FREE), left);
    }
    snprintf(t, sizeof(t), "%s%s%s%s%s", sz.need, rest[0] ? "\n" : "", rest, (sz.need[0] || rest[0]) && more[0] ? "\n\n" : "", more);
    return t;
}

/* A card's size, picked among the ones the device makes (dev->minMb to maxMb) that saves taking room (as
 * mcfs_save_room counts them; 0 = none go into it now) fit in, under title. current = the size it is now (0 = none),
 * marked so. The cursor starts on the one recommended for the saves (mcfs_recommended_mb), or, with none going in, on
 * current (or the 8 MB of a real card). device = the size the device makes its own cards of. The size in MB, or 0 =
 * back (or no size is big enough, which is said) */
int card_size_pick(int title, long long room, int current, int device)
{
    char items[8][48], need[24];
    const char *list[8];
    int mb, n = 0, start = 0, r, rec = room ? mcfs_recommended_mb(room, dev->minMb, dev->maxMb)
                                            : (dev->minMb <= 8 && dev->maxMb >= 8 ? 8 : dev->minMb);
    sz.device = device;
    sz.room = room;
    if (!rec) {
        message_wait(0, NULL, COLOR_WARN, T(T_SIZE_NONE));
        return 0;
    }
    for (mb = dev->minMb; mb <= dev->maxMb && n < 8; mb *= 2) {
        if (room > mcfs_card_room((long long)mb << 20))
            continue;   /* (too small for them) */
        snprintf(items[n], sizeof(items[0]), T(mb == current ? T_SIZE_CURRENT : mb == rec ? T_SIZE_RECOMMENDED : T_SIZE_MB), mb);
        list[n] = items[n];
        if (mb == (room || !current ? rec : current))   /* (saves going in: the one recommended for them) */
            start = n;
        sz.mb[n++] = mb;
    }
    sz.need[0] = 0;
    if (room) {
        format_size(room, need, sizeof(need));
        snprintf(sz.need, sizeof(sz.need), T(T_SIZE_NEED), need);
    }
    r = choose_noted(T(title), list, n, start, size_note);
    return r < 0 ? 0 : sz.mb[r];
}

int dest_new_size(long long room)
{
    int mb = card_size_pick(T_SIZE_TITLE, room, 0, 0);
    if (!mb)
        return 0;
    destNew.size = (long long)mb << 20;
    log_msg("new card %s: %d MB picked", destNew.id, mb);
    return 1;
}

const char *dest_new_note(void)
{
    static char t[200];
    snprintf(t, sizeof(t), T(T_NEWCARD_NOTE), (int)((destNew.size ? destNew.size : MCFS_CARD_8MB) >> 20));
    return t;
}

static void new_card_progress(long long done, long long total)
{
    upload_screen(done, total);
#ifdef DEBUG_BUILD
    if (done * 2 >= total)
        debug_capture_if('M');
#endif
}

/* The card a save goes to, once the user agreed to it. A card that is still to be made (&destNew) is made now: an
 * empty one of the size picked (destNew.size; none picked, the 8 MB of a real card), formatted as the PS2 does, in the
 * game's folder and under the device's own extension. It joins the cards at the end of the list, so the card the save
 * comes from stays where it is. NULL = it couldn't be made (said) */
card_t *dest_real(card_t *to)
{
    char dir[200], file[80];
    card_t *c = NULL;
    int r, there;
    if (to != &destNew)
        return to;
    card_work(&destNew, 0, T(T_NEWCARD_MAKING), 1);
    snprintf(dir, sizeof(dir), "%s%s/%s/", sdRoot, dev->cards, destNew.folder);
    ensure_dir(dir);
    there = file_exists(destNew.path);   /* (made by the device since the place was picked: left as it is) */
    r = there || (destNewMore && max_channels_set(destNew.folder, destNewMore) != 0)
            ? MCFS_ERR_IO
            : mcfs_new_card(destNew.path, destNew.size ? destNew.size : MCFS_CARD_8MB, new_card_progress);
    snprintf(file, sizeof(file), "%s%s", destNew.base, dev->ext);
    if (r == MCFS_OK && (c = cards_append(destNew.folder, file)) != NULL) {
        cards_recheck(c);
        cardsUnsorted = 1;
    }
    log_msg("new card %s: %d%s%s", destNew.id, r, c ? "" : ", not one of the cards", there ? " (its file was there already)" : "");
    card_work_done();
    if (!c) {
        if (!there) {   /* a card that isn't whole is no card; nor is its folder to stay, if it was made for it */
            unlink(destNew.path);
            dir[strlen(dir) - 1] = 0;
            rmdir(dir);
        }
        message_wait(0, NULL, COLOR_ERROR, T(T_NEWCARD_FAILED));
    }
    return c;
}

/* The card of a folder of games, on a channel: the one that is there or, made now as dest_real makes one, a new
 * one. NULL = it isn't there and couldn't be made (said) */
card_t *dest_card(const char *folder, int channel)
{
    int i;
    for (i = 0; i < nCards; i++)
        if (cards[i].channel == channel && !strcasecmp(cards[i].folder, folder))
            return &cards[i];
    destNewMore = 0;
    memset(&destNew, 0, sizeof(destNew));
    snprintf(destNew.folder, sizeof(destNew.folder), "%s", folder);
    snprintf(destNew.base, sizeof(destNew.base), "%.44s-%d", folder, channel);
    snprintf(destNew.id, sizeof(destNew.id), "%s/%s", destNew.folder, destNew.base);
    snprintf(destNew.path, sizeof(destNew.path), "%s%s/%s/%s%s", sdRoot, dev->cards, destNew.folder, destNew.base, dev->ext);
    destNew.type = TYPE_GAMEID;
    return dest_real(&destNew);
}
