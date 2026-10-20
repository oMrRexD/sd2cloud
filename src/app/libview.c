/* SD2Cloud -- the Library on the screens: its games in a group of the main screen, each one's saves opened as a card's
 * are, and the Library as a place a save is copied or moved to (in the list of cards it is picked in). The saves
 * themselves are kept by library.c. */
#include "app.h"

card_t libRoot;   /* the whole Library, as the place a save goes to ("lib:") */
card_t libView;   /* one game's saves, as its screen opens them ("lib:<game ID>") */

void library_start(void)
{
    char dir[200];
    /* (the data folder on a memory card, with no sd2psx: no room there for it, as for the templates) */
    if (standalone && appOnCard)
        library_init(NULL);
    else {
        snprintf(dir, sizeof(dir), "%sLibrary/", dataDir);
        library_init(dir);
    }
    memset(&libRoot, 0, sizeof(libRoot));
    snprintf(libRoot.path, sizeof(libRoot.path), "%s", LIBRARY_PATH);
    snprintf(libRoot.id, sizeof(libRoot.id), "library");
    snprintf(libRoot.folder, sizeof(libRoot.folder), "Library");
    libRoot.type = TYPE_LIBRARY;
}

card_t *library_root(void)
{
    snprintf(libRoot.base, sizeof(libRoot.base), "%s", T(T_TAB_LIBRARY));   /* (in the language shown now) */
    return &libRoot;
}

const char *library_row_name(int k)
{
    return library_game_name(k)[0] ? library_game_name(k) : T(T_LIBRARY_OTHER);
}

/* (one game's, as a card_t the screens of saves take) */
static card_t *view_of(int k)
{
    memset(&libView, 0, sizeof(libView));
    snprintf(libView.path, sizeof(libView.path), "%s%s", LIBRARY_PATH, library_game_id(k));
    snprintf(libView.id, sizeof(libView.id), "library/%s", library_game_id(k));
    snprintf(libView.base, sizeof(libView.base), "%s", T(T_TAB_LIBRARY));   /* (short: the screen's title; the game is
                                                                               each save's) */
    snprintf(libView.folder, sizeof(libView.folder), "%s", library_game_id(k));
    libView.type = TYPE_LIBRARY;
    return &libView;
}

void library_open(int k)
{
    card_screen(view_of(k));
}

icon_t *library_icon(int k)
{
    buffer_t iconsys = {0}, ico = {0};
    char path[48], folder[33];
    icon_t *ic = NULL;
    snprintf(path, sizeof(path), "%s%s", LIBRARY_PATH, library_game_id(k));
    if (mcfs_newest_save_icon(path, NULL, folder, &iconsys, &ico) == 0)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    return ic;
}

void library_picture(int k, icon_t *ic, float t)
{
    static card_t shown;   /* (drawn as a game's card is: its ID on the label, its newest save turning) */
    char s[64];
    memset(&shown, 0, sizeof(shown));
    shown.type = strcmp(library_game_id(k), LIBRARY_OTHER) ? TYPE_GAMEID : TYPE_NAMED;
    snprintf(shown.folder, sizeof(shown.folder), "%s", shown.type == TYPE_GAMEID ? library_game_id(k) : T(T_LIBRARY_OTHER));
    draw_card_picture(&shown, ic, t);
    snprintf(s, sizeof(s), T(T_LIBRARY_SAVES), library_game_saves(k));
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, s);
}

/* -------- the Library as the place saves go to: its row says the game they go under (their folder in it), and its
 * card is that game's; with saves of several games, how many games */

static char destId[12];   /* the game the saves go under ("" = several) */
static int destGames;
static char destName[80];

static void dest_named(void)
{
    char name[64];
    if (destGames > 1)
        snprintf(destName, sizeof(destName), T(T_LIBRARY_GAMES), destGames);
    else if (!strcmp(destId, LIBRARY_OTHER))
        snprintf(destName, sizeof(destName), "%s", T(T_LIBRARY_OTHER));
    else
        snprintf(destName, sizeof(destName), "%s", game_title(destId, name, sizeof(name)) ? name : destId);
}

void library_dest(const char *folder)
{
    library_game_of(folder ? folder : "", destId);
    destGames = 1;
    dest_named();
}

void library_dest_marked(void)
{
    char id[12], seen[MARKS_MAX][12];
    int i, k;
    destGames = 0;
    for (i = 0; i < marks.n; i++) {
        if (library_path(marks.save[i].card->path))
            continue;   /* (those stay where they are) */
        library_game_of(marks.save[i].s.folder, id);
        for (k = 0; k < destGames && strcmp(seen[k], id); k++)
            ;
        if (k == destGames)
            snprintf(seen[destGames++], sizeof(seen[0]), "%s", id);
    }
    snprintf(destId, sizeof(destId), "%s", destGames == 1 ? seen[0] : "");
    dest_named();
}

const char *library_dest_name(void) { return destName; }

void library_place(int words)
{
    static card_t shown;   /* (the game's card, as the Library's group draws it; with several, the Library's) */
    if (destGames == 1) {
        memset(&shown, 0, sizeof(shown));
        shown.type = strcmp(destId, LIBRARY_OTHER) ? TYPE_GAMEID : TYPE_NAMED;
        snprintf(shown.folder, sizeof(shown.folder), "%s", shown.type == TYPE_GAMEID ? destId : T(T_LIBRARY_OTHER));
        draw_card_picture(&shown, NULL, ui_clock());
    } else
        look_card(CARD_X, CARD_Y, NULL, T(T_TAB_LIBRARY));
    if (words)
        ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 2, PANEL_TITLE_W + 26, COLOR_DIM, T(T_LIBRARY_HINT));
}
