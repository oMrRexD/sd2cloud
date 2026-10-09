/* SD2Cloud -- the cards as the screens list them: in groups (numbered cards, games, boot cards, files), one group
 * shown at a time, with the selected card drawn big on the right. */
#include "app.h"

/* -------- the big glass card on the right (the main screen and the backups list) */

#define LIST_W     208    /* its width: the tabs and the counter are centered on it */
#define LIST_Y     104
#define ROW_H      30
#define ROWS       5

/* a numbered card's number: what comes after "Card" (or what the device calls them) in its folder's name */
int card_number(const card_t *c) { return atoi(c->folder + strlen(dev->numbered)); }

static void card_label(const card_t *c, char *number, char *label)
{
    number[0] = label[0] = 0;
    if (c->type == TYPE_NORMAL && !strncmp(c->folder, dev->numbered, strlen(dev->numbered)))
        snprintf(number, 8, "%s", c->folder + strlen(dev->numbered));
    else
        snprintf(label, 48, "%s", c->folder);
}

/* a card's big picture: the glass card with its number, or with its name and, on a game card, its save's icon */
void draw_card_picture(const card_t *c, icon_t *ic, float t)
{
    char number[8], label[48];
    card_label(c, number, label);
    look_card(CARD_X, CARD_Y, number, ic ? NULL : label);
    if (ic) {
        icon_draw(ic, CARD_CX, CARD_Y + 82, 104, t);
        if (ui_measure(FONT_SMALL, label) <= LOOK_CARD_W - 60)
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + 146, 0x8CD4DA, label);
        else
            ui_text_fit(FONT_SMALL, CARD_X + 30, CARD_Y + 146, LOOK_CARD_W - 60, 0x8CD4DA, label);
    }
}

static float rowsDx;   /* the rows are drawn this far to the side (a folder's cards sliding into the list) */
int rowsWide;          /* and their text may be this wide (0 = as usual): the folders' names are longer than a card's */

/* the rows of a list: scrolls with the cursor, arrows when there's more */
void list_rows(int n, int cursor, int top, const char *(*text)(int i, char *buf), u32 (*dot)(int i))
{
    char buf[64];
    int i, y;
    for (i = top, y = ROW_Y0; i < n && i < top + ROWS; i++, y += ROW_H) {
        const char *s = text(i, buf);
        float x = LIST_X + rowsDx;
        if (dot) {   /* the card's status: a small colored light */
            float cy = y + ui_line_height(FONT_TEXT) / 2.0f + 1;
            ui_image(IMG_GLOW, x + 21, cy - 7, 14, 14, dot(i), 0x80);
            ui_rect(x + 27, cy - 1, 2, 2, dot(i), 0x80);
        }
        /* without the status light (the backups' dates) the text starts further left: a whole date fits */
        if (i == cursor)
            look_item(x + (dot ? 42 : 24), y, s, 1, 0);
        else
            ui_text_fit(FONT_TEXT, x + (dot ? 42 : 24), y, rowsWide ? rowsWide : dot ? 150 : 172, COLOR_ITEM, s);
    }
    if (top > 0)
        look_arrow(LIST_X + 186, ROW_Y0 - 14, 0, 0x6E9AE0);
    if (top + ROWS < n)
        look_arrow(LIST_X + 186, ROW_Y0 + ROWS * ROW_H, 1, 0x6E9AE0);
    if (n > 0) {   /* "3/12" under the list: where the cursor is in it */
        char t[24];
        snprintf(t, sizeof(t), "%d/%d", cursor + 1, n);
        ui_text_center(FONT_SMALL, LIST_X + LIST_W / 2.0f, LIST_Y + 222, 0x8E98AA, t);
    }
}

void scroll_to(int cursor, int *top)
{
    if (cursor < *top)
        *top = cursor;
    if (cursor >= *top + ROWS)
        *top = cursor - ROWS + 1;
    if (*top < 0)
        *top = 0;
}
/* -------- the cards in groups, one shown at a time, chosen with left/right (or L1/R1) over the list: the numbered
 * cards (CardN, with the folders the user named), the game cards (Game ID) and the boot cards. A group without cards
 * isn't offered; with a single group there are no tabs at all. Used by the main screen and by "Copy to" / "Move to".
 * The game cards come in folders, one per game and named after it (a game may have cards of more than one ID, and a
 * card more than one channel): the group lists the folders, and X on one puts its cards in the list's place, sliding
 * in; circle brings the folders back.
 * The main screen has one more group, which isn't of cards: the devices whose files can be browsed (a .psu to import
 * into a card, a folder to export a save to) */

static const int tabText[TABS] = {T_TAB_CARDS, T_TAB_GAMES, T_TAB_BOOT, T_TAB_FILES};
#define TAB_Y      78     /* the tabs: over the list, on the line of the title on the right */
#define TAB_GAP    22
const int deviceText[FDEVS] = {T_DEV_SD, T_DEV_USB};

#define FOLDER_W   236    /* a folder's name in the list: up to where the big card's picture starts */

int tab_of(const card_t *c)
{
    return c->type == TYPE_GAMEID ? TAB_GAMES : c->type == TYPE_BOOT ? TAB_BOOT : TAB_CARDS;
}

/* what a game card's folder is called: the game, or the card's own folder when the game isn't known */
const char *game_of(const card_t *c) { return c->game[0] ? c->game : c->folder; }

/* is the list one of folders right now? */
int tabs_on_folders(const tabs_t *g) { return g->tab == TAB_GAMES && g->open < 0; }

/* is "New card" offered in the list shown for the game of the save on its way (newGame)? Among the folders when none
 * of them is that game's; in a folder when it is that game's (a folder of the list is a game, which may be more than
 * one folder of the microSD: versions of the same game under one name) */
static int tabs_new_game(const tabs_t *g)
{
    int k, i;
    if (!g->newGame[0] || g->tab != TAB_GAMES)
        return 0;
    for (k = g->open >= 0 ? g->open : 0; k < (g->open >= 0 ? g->open + 1 : g->folders); k++)
        for (i = g->first[k]; i < g->first[k + 1]; i++)
            if (!strcasecmp(cards[g->games[i]].folder, g->newGame))
                return g->open >= 0;
    return g->open < 0;
}

/* how many rows that aren't a card or a folder the list shown starts with (idx -1): "New card", or "All saves" */
static int tabs_lead(const tabs_t *g) { return g->add || tabs_new_game(g) || (g->all && tabs_on_folders(g) && g->nGames > 1); }

/* the Games group's list: its folders, or the cards of the one that is open (with ui_lock held) */
static void tabs_games(tabs_t *g)
{
    int i, k;
    g->nGames = g->folders = 0;
    for (i = 0; i < nCards; i++) {
        if (&cards[i] == g->skip || tab_of(&cards[i]) != TAB_GAMES)
            continue;
        for (k = g->nGames++; k > 0 && strcasecmp(game_of(&cards[g->games[k - 1]]), game_of(&cards[i])) > 0; k--)
            g->games[k] = g->games[k - 1];
        g->games[k] = i;
    }
    for (i = 0; i < g->nGames; i++)
        if (!i || strcasecmp(game_of(&cards[g->games[i - 1]]), game_of(&cards[g->games[i]])) != 0) {
            snprintf(g->label[g->folders], sizeof(g->label[0]), "%s", game_of(&cards[g->games[i]]));
            fit(FONT_TEXT, g->label[g->folders], sizeof(g->label[0]), FOLDER_W);
            g->first[g->folders++] = i;
        }
    g->first[g->folders] = g->nGames;
    if (g->open >= g->folders)
        g->open = -1;
    g->n = 0;
    if (tabs_lead(g))
        g->idx[g->n++] = -1;
    if (g->open < 0)
        for (i = 0; i < g->folders; i++)
            g->idx[g->n++] = g->games[g->first[i]];
    else
        for (i = g->first[g->open]; i < g->first[g->open + 1]; i++)
            g->idx[g->n++] = g->games[i];
}

/* X on a folder: its cards take the list's place */
void tabs_open(tabs_t *g)
{
    g->open = g->cursor - tabs_lead(g);
    g->openTop = g->top;
    g->cursor = g->top = 0;
    tabs_games(g);
    g->moved = ui_clock();
    g->way = 1;
}

/* circle inside a folder: back to the folders, on the one that was open. slide = 0: at once (the group is left) */
static void tabs_close(tabs_t *g, int slide)
{
    g->cursor = g->open;
    g->top = g->openTop;
    g->open = -1;
    tabs_games(g);
    g->cursor += tabs_lead(g);
    if (slide) {
        g->moved = ui_clock();
        g->way = -1;
    }
}

/* shows a group, on the card it was left on (with ui_lock held) */
void tabs_show(tabs_t *g, int tab)
{
    int i;
    if (g->tab == TAB_GAMES && g->open >= 0)
        tabs_close(g, 0);
    g->keep[g->tab][0] = g->cursor;
    g->keep[g->tab][1] = g->top;
    memset(g->count, 0, sizeof(g->count));
    g->n = 0;
    if (g->add) {   /* a group that has no card yet is offered too: one can be added to it */
        g->count[TAB_CARDS] = g->count[TAB_GAMES] = 1;
        g->count[TAB_BOOT] = dev->sd2psx;
        g->idx[g->n++] = -1;
    }
    for (i = 0; i < nCards; i++) {
        if (&cards[i] == g->skip)
            continue;
        g->count[tab_of(&cards[i])]++;
        if (tab_of(&cards[i]) == tab)
            g->idx[g->n++] = i;
    }
    if (g->newGame[0] && !g->count[TAB_GAMES])   /* no game card yet: the group is there for the new one */
        g->count[TAB_GAMES] = 1;
    if (g->files)
        g->count[TAB_FILES] = FDEVS;
    if (tab == TAB_FILES)
        g->n = FDEVS;
    g->tab = tab;
    if (tab == TAB_GAMES)
        tabs_games(g);
    g->cursor = g->keep[tab][0] < g->n ? g->keep[tab][0] : 0;
    g->top = g->keep[tab][1];
    scroll_to(g->cursor, &g->top);
}

/* from scratch, on the first group that has cards; skip = a card to leave out; files = with the Files group (with
 * ui_lock held) */
void tabs_init(tabs_t *g, const card_t *skip, int files)
{
    int k;
    memset(g, 0, sizeof(*g));
    g->skip = skip;
    g->files = files;
    g->open = -1;
    g->moved = -1;
    tabs_show(g, TAB_CARDS);
    for (k = 0; k < TABS && !g->count[k]; k++)
        ;
    tabs_show(g, k < TABS ? k : TAB_CARDS);
}

/* the next group with cards to that side (dir -1 / +1); -1 = none */
static int tabs_next(const tabs_t *g, int dir)
{
    int k;
    for (k = g->tab + dir; k >= 0 && k < TABS; k += dir)
        if (g->count[k])
            return k;
    return -1;
}

/* the selected card, or the first card of the selected folder (NULL = the group has none, or it's the Files group) */
card_t *tabs_card(const tabs_t *g)
{
    return g->n && g->tab != TAB_FILES && g->idx[g->cursor] >= 0 ? &cards[g->idx[g->cursor]] : NULL;
}

/* puts the cursor on a card of the group shown: in the Games group, on its folder */
void tabs_find(tabs_t *g, int card)
{
    int k, i;
    for (k = 0; k < g->n; k++)
        if (g->idx[k] == card)
            g->cursor = k;
    if (tabs_on_folders(g))
        for (k = 0; k < g->folders; k++)
            for (i = g->first[k]; i < g->first[k + 1]; i++)
                if (g->games[i] == card)
                    g->cursor = k + tabs_lead(g);
    scroll_to(g->cursor, &g->top);
}

/* X on a folder opens it, circle inside one closes it (with the sound). 1 = the press was one of those */
int tabs_folder_nav(tabs_t *g, u32 b)
{
    if ((b & (PAD_CROSS | PAD_TRIANGLE)) && tabs_on_folders(g) && g->n && g->idx[g->cursor] >= 0) {
        sound_play(SND_CONFIRM);
        ui_lock();
        tabs_open(g);
        ui_unlock();
        return 1;
    }
    if ((b & PAD_CIRCLE) && g->tab == TAB_GAMES && g->open >= 0) {
        sound_play(SND_BACK);
        ui_lock();
        tabs_close(g, 1);
        ui_unlock();
        return 1;
    }
    return 0;
}

/* up/down move the cursor, left/right change the group (with the sound). 1 = the press was one of those */
int tabs_nav(tabs_t *g, u32 b)
{
    if (b & (PAD_UP | PAD_DOWN)) {
        if (g->n) {
            ui_lock();
            g->cursor = (b & PAD_UP) ? (g->cursor + g->n - 1) % g->n : (g->cursor + 1) % g->n;
            scroll_to(g->cursor, &g->top);
            ui_unlock();
            sound_play(SND_MOVE);
        }
        return 1;
    }
    if (b & TAB_KEYS) {
        int k = tabs_next(g, (b & (PAD_LEFT | PAD_L1)) ? -1 : 1);
        if (k >= 0) {
            ui_lock();
            tabs_show(g, k);
            ui_unlock();
            sound_play(SND_MOVE);
        }
        return 1;
    }
    return 0;
}

static const tabs_t *rowsOf;   /* the groups list_rows is drawing (its callbacks only get the row) */

static const char *tabs_text(int i, char *buf)
{
    (void)buf;
    if (rowsOf->tab == TAB_FILES)
        return T(deviceText[i]);
    if (rowsOf->idx[i] < 0)
        return T(rowsOf->add || rowsOf->newGame[0] ? T_NEW_CARD : T_ALL_SAVES);
    return tabs_on_folders(rowsOf) ? rowsOf->label[i - tabs_lead(rowsOf)] : cards[rowsOf->idx[i]].base;
}

static u32 tabs_dot(int i) { return rowsOf->idx[i] < 0 ? COLOR_DIM : status_color(&cards[rowsOf->idx[i]]); }

/* the groups over the list (the one shown bright, in a soft light; the others dim; a small arrow on each side that
 * has another group) and the shown group's cards. dots = each card's status light */
void tabs_draw(const tabs_t *g, int dots)
{
    int k, shown = 0, w = 0, x, lh = ui_line_height(FONT_SMALL);
    float cy = TAB_Y + lh / 2.0f + 1;
    if (g->tab == TAB_FILES)
        dots = 0;
    for (k = 0; k < TABS; k++)
        if (g->count[k])
            w += (shown++ ? TAB_GAP : 0) + ui_measure(FONT_SMALL, T(tabText[k]));
    if (shown > 1) {
        x = LIST_X + LIST_W / 2 - w / 2;
        if (tabs_next(g, -1) >= 0)
            ui_triangle(x - 18, cy, x - 11, cy - 6, x - 11, cy + 6, 0x6E9AE0, 0x70);
        for (k = 0; k < TABS; k++) {
            int tw;
            if (!g->count[k])
                continue;
            tw = ui_measure(FONT_SMALL, T(tabText[k]));
            if (k == g->tab) {
                ui_light(x + tw / 2.0f, cy, tw / 2.0f + 26, lh * 0.8f, COLOR_GLOW, 0x30);
                ui_text(FONT_SMALL, x, TAB_Y, COLOR_ITEM_ON, T(tabText[k]));
            } else
                ui_text(FONT_SMALL, x, TAB_Y, 0x6F7C94, T(tabText[k]));
            x += tw + TAB_GAP;
        }
        if (tabs_next(g, 1) >= 0)
            ui_triangle(x - TAB_GAP + 18, cy, x - TAB_GAP + 11, cy + 6, x - TAB_GAP + 11, cy - 6, 0x6E9AE0, 0x70);
    }
    rowsOf = g;
    {
        /* a folder just opened or closed: the list comes in from the side it went to, fading in */
        int folders = tabs_on_folders(g), sliding = 0, i;
        float p = (ui_clock() - g->moved) / 0.22f;
        if (g->tab == TAB_GAMES && g->moved >= 0 && p < 1) {
            sliding = 1;
            rowsDx = g->way * (1 - p) * (1 - p) * 44;
            ui_alpha((int)(128 * p));
        }
        rowsWide = folders ? FOLDER_W : 0;
        list_rows(g->n, g->cursor, g->top, tabs_text, folders ? NULL : dots ? tabs_dot : NULL);
        for (k = g->top; g->tab != TAB_FILES && k < g->n && k < g->top + ROWS; k++) {
            float x = LIST_X + rowsDx, y = ROW_Y0 + (k - g->top) * ROW_H + ui_line_height(FONT_TEXT) / 2.0f + 1;
            int here = g->idx[k] >= 0 && g->idx[k] == activeCard;
            if (g->idx[k] < 0 && (g->add || g->newGame[0])) {   /* "New card": a plus where a folder has its picture */
                ui_rect(x + 4, y - 1, 12, 2, COLOR_ACCENT, 0x70);
                ui_rect(x + 9, y - 6, 2, 12, COLOR_ACCENT, 0x70);
            } else if (g->idx[k] < 0) {   /* "All saves": two small cards, one behind the other */
                ui_rect(x + 8, y - 11, 10, 13, 0x6E9AE0, 0x38);   /* (in the arrows' blue, the one behind fainter) */
                ui_rect(x + 2, y - 6, 11, 14, 0x6E9AE0, 0x80);
                ui_rect(x + 4, y + 1, 7, 4, 0x16223C, 0x58);     /* the label's place on the one in front */
            } else if (folders) {   /* a game's folder: a small arrow, as the tabs have (it opens) */
                /* (grey, when the device opens another folder for that game now) */
                ui_triangle(x + 5, y - 6, x + 5, y + 6, x + 14, y,
                            cards[g->games[g->first[k - tabs_lead(g)]]].moved[0] ? 0x5A6478 : 0x6E9AE0, 0x80);
                for (i = g->first[k - tabs_lead(g)]; i < g->first[k - tabs_lead(g) + 1]; i++)
                    here |= g->games[i] == activeCard;
                x -= 22;   /* the mark of the card in use goes before the folder */
            }
            if (here) {   /* the card the sd2psx is using: before it, the big card's own glass, small */
                ui_image(IMG_GLOW, x - 6, y - 15, 30, 30, COLOR_ACCENT, 0x48);
                ui_image(IMG_CARD, x + 1, y - 10, 17, 20, 0xFFFFFF, 0x80);
            }
        }
        if (g->tab == TAB_GAMES && g->open >= 0) {   /* over a folder's cards, small: which folder this is */
            ui_triangle(LIST_X + 27, LIST_Y + 23, LIST_X + 27, LIST_Y + 33, LIST_X + 35, LIST_Y + 28, 0x6E9AE0, 0x70);
            ui_text_fit(FONT_SMALL, LIST_X + 44, LIST_Y + 21, FOLDER_W - 20, 0x8E98AA, game_of(&cards[g->idx[tabs_lead(g)]]));
        }
        rowsDx = 0;
        rowsWide = 0;
        if (sliding)
            ui_alpha(128);
    }
}
