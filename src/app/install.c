/* SD2Cloud -- a memory card file (.mcd, .mc2, .ps2, .zip): looked into, and installed as a card of the microSD. */
#include "app.h"

/* -------- installing a card file (□ in its saves): it becomes a card of the microSD. The cards come in their groups,
 * as when a save's destination is picked, each list starting with "New card": X on that makes a new card of the group
 * (the lowest numbered card there is none of; the next channel of the BootCard; the next channel of a game's folder,
 * the one that is open or the one of the game the file is of), X on a card puts the file in its place */

#define MAX_FILE_GAMES 16
static char fileGames[MAX_FILE_GAMES][12];   /* the games (IDs) the file may be a card of */
static int nFileGames;
char fileGameFolder[48];              /* with a single one: the folder the sd2psx keeps its cards in */

/* Which game a card file is of, the way the sd2psx would know: the ID its name starts with (the sd2psx and the
 * MemCard PRO2 both name a game's card after it, SLUS-21065-1; OPL names a game's virtual card after the game's
 * program, SLUS_210.65_0), then the IDs its saves are named after (BASLUS-21065...), the newest save first */
static void file_games(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    char id[12];
    int n, i, k, len = 10;
    nFileGames = 0;
    snprintf(id, sizeof(id), "%.11s", cardFileName);
    for (i = 0; id[i]; i++)
        id[i] = toupper((unsigned char)id[i]);
    if (i == 11 && id[4] == '_' && id[8] == '.') {   /* SLUS_210.65 is SLUS-21065 */
        id[4] = '-';
        id[8] = id[9];
        id[9] = id[10];
        len = 11;
    }
    id[10] = 0;
    if (is_game_id(id) && !isalnum((unsigned char)cardFileName[len]))
        strcpy(fileGames[nFileGames++], id);
    n = cardFile.zip && !cardFile.image ? 0 : mcfs_list_saves(MCFS_IMAGE, list, MCFS_MAX_SAVES, NULL);
    for (i = 0; i < n && nFileGames < MAX_FILE_GAMES; i++) {
        const char *s = list[i].folder;
        if (strlen(s) < 12 || s[0] != 'B' || !isupper((unsigned char)s[1]))
            continue;
        snprintf(id, sizeof(id), "%.10s", s + 2);
        for (k = 0; k < nFileGames && strcmp(fileGames[k], id); k++)
            ;
        if (is_game_id(id) && k == nFileGames)
            strcpy(fileGames[nFileGames++], id);
    }
    fileGameFolder[0] = 0;
    if (nFileGames == 1)
        game_folder(fileGames[0], fileGameFolder, sizeof(fileGameFolder));
    log_msg("install: %s may be of %d game(s)%s%s", cardFileName, nFileGames, nFileGames ? ", first " : "", nFileGames ? fileGames[0] : "");
}

/* the game the new card is of: asked when the file tells of more than one. 0 = none can be told (said so), or circle */
static int pick_game(char id[12])
{
    static char text[MAX_FILE_GAMES][100];
    static const char *items[MAX_FILE_GAMES];
    char title[64];
    int i, k = 0;
    if (!nFileGames) {
        message_wait(0, NULL, COLOR_WARN, T(T_INSTALL_NO_GAME));
        return 0;
    }
    for (i = 0; i < nFileGames; i++) {
        game_title(fileGames[i], title, sizeof(title));
        snprintf(text[i], sizeof(text[i]), "%s%s%s", fileGames[i], title[0] ? "  " : "", title);
        items[i] = text[i];
    }
    if (nFileGames > 1 && (k = choose(T(T_INSTALL_WHICH_GAME), items, nFileGames, 0)) < 0)
        return 0;
    strcpy(id, fileGames[k]);
    return 1;
}

/* 0 = circle. Else *to = the card picked, to be replaced, or NULL: a new card of the group *tab (of a game: in
 * gameFolder, "" when the game has to be asked). again = back from a question that was answered with circle: the
 * cards are where they were left (the group, the folder that was open, the cursor) */
static int install_dest(card_t **to, int *tab, char gameFolder[48], int again)
{
    int i, r = 0;
    ui_lock();
    if (!again) {
        tabs_init(&dst.g, NULL, 0);
        dst.g.add = 1;
        tabs_show(&dst.g, TAB_CARDS);
        for (i = 0; i < MAX_CARDS; i++)
            dst.freeBytes[i] = -2;
    }
    dst.title = T(T_INSTALL_TO);
    dst.need = 0;
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
        if ((b & PAD_CROSS) && dst.g.n) {
            sound_play(SND_CONFIRM);
            *to = tabs_card(&dst.g);
            *tab = dst.g.tab;
            snprintf(gameFolder, 48, "%s", dest_game_folder());
            r = 1;
            break;
        }
    }
    ui_scene(scene_frame);   /* off the screen before the icon goes */
    ui_lock();
    icon_free(dst.icon);
    dst.icon = NULL;
    dst.iconCard = -1;
    ui_unlock();
    return r;
}

/* Is the file's card read whole before the card it goes to is touched (card_file_install)? One that doesn't fit in
 * memory isn't: it is written as it is read */
static int file_read_first(void)
{
    void *volatile p;   /* (volatile: or the compiler takes the room for granted and asks for none) */
    if (cardFile.image || cardFile.zip)
        return 1;
    if (!(p = malloc((size_t)cardFile.size)))
        return 0;
    free(p);
    return 1;
}

/* 1 = the file is a card of the microSD now */
int install_card(void)
{
    char folder[48], base[56], gameFolder[48], id[12], title[64], t[300], active[96];
    card_t fresh, *to;
    int tab, r, max, more = 0, again = 0;   /* more = the folder is to have that many channels (0 = as it is) */
    file_games();
    for (;;) {
        if (!install_dest(&to, &tab, gameFolder, again))
            return 0;
        again = 1;
        title[0] = 0;
        if (to) {
            snprintf(t, sizeof(t), T(T_INSTALL_REPLACE_ASK), to->base);
            dlg_new(COLOR_TITLE, t);
            dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_INSTALL_REPLACE_TEXT));
            if (to->type == TYPE_BOOT)   /* what the console starts from may be on it */
                dlg_line(FONT_TEXT, COLOR_WARN, 6, T(T_INSTALL_BOOT_WARN));
            if (!file_read_first()) {   /* what a file that stops reading halfway can cost, said before it does */
                snprintf(t, sizeof(t), T(T_INSTALL_BIG_WARN), to->base);
                dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
            }
            dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_REPLACE);
        } else {
            if (tab == TAB_GAMES) {
                id[0] = 0;
                if (!gameFolder[0]) {
                    if (!pick_game(id))
                        continue;
                    game_folder(id, gameFolder, sizeof(gameFolder));
                }
                /* the game, under the card's name: the folder is its ID, unless Game2Folder.ini gave it another */
                if (!game_title(gameFolder, title, sizeof(title)) && id[0])
                    game_title(id, title, sizeof(title));
            }
            max = tab == TAB_CARDS ? 0 : max_channels(tab == TAB_BOOT ? "BOOT" : gameFolder);
            more = 0;
            if (!new_card(tab, gameFolder, max, folder, base)) {
                /* every channel the folder has is taken. The sd2psx goes as far as the folder's .ini says (8 without
                 * one): it can be told of one more */
                if (!dev->sd2psx || max >= 255) {
                    snprintf(t, sizeof(t), T(T_INSTALL_NO_CHANNEL), max);
                    message_wait(0, NULL, COLOR_WARN, t);
                    continue;
                }
                if (!ask_more_channels(max))
                    continue;
                more = max + 1;
                if (!new_card(tab, gameFolder, more, folder, base)) {   /* (that one's file is there already) */
                    snprintf(t, sizeof(t), T(T_INSTALL_NO_CHANNEL), more);
                    message_wait(0, NULL, COLOR_WARN, t);
                    continue;
                }
            }
            dlg_new(COLOR_TITLE, T(T_INSTALL_NEW_ASK));
            dlg_line(FONT_TEXT, COLOR_ACCENT, title[0] ? 2 : 0, base);
            dlg_line(FONT_SMALL, COLOR_DIM, 0, title);
            dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_INSTALL_YES);
        }
        next.wide = 1;
        dlg_show();
        if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)
            break;
        sound_play(SND_BACK);   /* back to the cards */
    }
    sound_play(SND_CONFIRM);
    if (!to) {   /* a card to be: its folder first, and its channel within the sd2psx's reach */
        memset(&fresh, 0, sizeof(fresh));
        snprintf(fresh.folder, sizeof(fresh.folder), "%s", folder);
        snprintf(fresh.base, sizeof(fresh.base), "%s", base);
        snprintf(fresh.id, sizeof(fresh.id), "%s/%s", folder, base);
        snprintf(fresh.path, sizeof(fresh.path), "%s%s/%s/", sdRoot, dev->cards, folder);
        ensure_dir(fresh.path);
        snprintf(fresh.path, sizeof(fresh.path), "%s%s/%s/%s%s", sdRoot, dev->cards, folder, base, dev->ext);
        if (file_exists(fresh.path)) {   /* (made by the device since the place was picked: left as it is) */
            message_wait(0, NULL, COLOR_ERROR, T(T_NEWCARD_FAILED));
            return 0;
        }
        if (more && max_channels_set(folder, more) != 0) {
            message_wait(0, NULL, COLOR_ERROR, T(T_ERR_EXPORT));
            return 0;
        }
    } else if (card_free(to, NULL))
        return 0;
    restoreFile = cardFile.image ? 2 : 3;
    restoreNew = !to && (!cardFile.zip || cardFile.image);   /* (a .zip inflated over the card can't be stopped) */
    lastRestoreDraw = 0;
    lastRestorePhase = -1;
    card_work(to ? to : &fresh, 0, T(T_LOADING), 1);   /* (what it says is set right as soon as it starts) */
    r = card_file_install(&cardFile, to ? to : &fresh, restore_progress);
    card_work_done();
    restoreFile = restoreNew = 0;
    log_msg("install %s as %s: %d", cardFileName, to ? to->id : fresh.id, r);
    if (to) {
        cards_recheck(to);
        card_back();
    } else if (r != 0) {
        unlink(fresh.path);   /* a card that isn't whole is no card; nor is its folder to stay, if it was made for it */
        snprintf(t, sizeof(t), "%s%s/%s", sdRoot, dev->cards, folder);
        rmdir(t);
    } else {
        /* it joins the cards, which move over for it: the one the sd2psx is on and the main screen's are found again */
        snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
        snprintf(t, sizeof(t), "%s%s", base, dev->ext);
        if ((to = cards_add(folder, t)) != NULL)
            cards_recheck(to);
        for (activeCard = nCards - 1; activeCard >= 0 && strcmp(cards[activeCard].id, active); activeCard--)
            ;
        menu_refresh();
    }
    if (r == -2)
        return 0;   /* cancelled: before writing, or a new card, which is no more */
    dlg_new(r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_INSTALL_OK : T_INSTALL_FAILED));
    dlg_line(FONT_TEXT, COLOR_ACCENT, 4, to ? to->base : base);
    if (r != 0)
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, googleError);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    sound_play(r == 0 ? SND_EXIT : SND_BACK);
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    return r == 0;
}

/* a .zip's card on its way to memory: the bar of the box on screen; circle gives up */
static int open_progress(int phase, long long done, long long total)
{
    (void)phase;
    watch_cancel();
    if (cancelLatched)
        return 1;
    ui_lock();
    dlg.permille = total ? (int)(done * 1000 / total) : 0;
    ui_unlock();
    return 0;
}

/* X on a card file: the card it holds, shown as the cards of the microSD are. Its saves can be copied to those, and □
 * installs it as one of them (install_card). A .zip's card that doesn't fit in memory can't be shown: installing it
 * is offered right away. install = square on the file itself: installed without being shown first */
void card_file_screen(const char *file, int install)
{
    char path[660];
    size_t n = strlen(file);
    int r;
    snprintf(path, sizeof(path), "%s%s", fb.dir, file);
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_LOADING));
    if (n > 4 && !strcasecmp(file + n - 4, ".zip")) {
        dlg_bar(0, NULL);
        dlg_buttons(BUTTON_CIRCLE, T_CANCEL, 0, 0);
    }
    dlg_show();
    cancelLatched = 0;
    r = card_file_open(&cardFile, path, open_progress);
    if (r == -2) {
        sound_play(SND_BACK);
        return;
    }
    if (r != 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_CARD_FILE));
        return;
    }
#ifdef DEBUG_BUILD
    debug_log_memory("a card file is open");   /* a .zip's card is in memory now */
#endif
    snprintf(cardFileName, sizeof(cardFileName), "%s", file);
    ui_lock();
    memset(&fileCard, 0, sizeof(fileCard));
    snprintf(fileCard.base, sizeof(fileCard.base), "%.*s", (int)(n - 4), file);
    utf8_fix(fileCard.base, sizeof(fileCard.base));
    fit(FONT_BROWSER, fileCard.base, sizeof(fileCard.base), 250);   /* a save's name goes on its right */
    snprintf(fileCard.id, sizeof(fileCard.id), "file/%.80s", file);
    snprintf(fileCard.path, sizeof(fileCard.path), "%s", MCFS_IMAGE);
    fileCard.size = cardFile.size;
    fileCard.type = TYPE_NORMAL;
    ui_unlock();
    if (install)
        install_card();
    else if (!cardFile.zip || cardFile.image)
        card_screen(&fileCard);
    else if (confirm(T(T_FILE_TOO_BIG), NULL, T_INSTALL_YES))
        install_card();
    card_file_close(&cardFile);
}
