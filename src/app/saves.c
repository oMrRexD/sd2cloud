/* SD2Cloud -- one card's saves, shown the way the PS2 browser shows them: a grid of their 3D icons. */
#include "app.h"

/* -------- one card: its saves, the PS2 browser's way */

#define GRID_COLS 5
#define GRID_ROWS 4
#define BRW_MAX   1024     /* the saves one screen holds: a card's, or every game card's */

static int brwUnread;      /* the card on screen couldn't be read (a MemCard PRO2 gave nothing of a card, seen 10/2026) */
struct brw_s brw;

/* the card of a file being looked into (card_file_screen): shown as any other, but it isn't one of cards[], it is only
 * read, and what can be done with it is to copy its saves and to install it on the microSD */
card_t fileCard;
card_file_t cardFile;       /* that file */
/* stands for every game card at once on the saves' screen ("All saves" in the Games group): each save there knows the
 * card it is on, and what is done to it is done to that card */
card_t allGames;
/* the card a save was last copied or moved to (save_transfer): on that screen it shows there right away */
card_t *transferDest;
char cardFileName[256];     /* and its name, which may tell which game the card is of */

/* A save's title, in its two lines, from its icon.sys. One written in Japanese comes empty (the font here has no
 * letters for it): the game's name takes its place, when the ID in the folder's name is one the sd2psx's list has,
 * in two lines when it is long (with ui_lock held) */
#define TITLE_W 330   /* as wide as a line of it gets on the save's own page */
void save_title(save_view_t *v, const icon_t *ic)
{
    char id[12], name[96], *cut, *p;
    int skip = 3;
    v->line1[0] = v->line2[0] = 0;
    if (ic) {
        snprintf(v->line1, sizeof(v->line1), "%s", ic->line1);
        snprintf(v->line2, sizeof(v->line2), "%s", ic->line2);
    }
    if (v->line1[0] || v->line2[0] || !save_game_id(v->s.folder, id) || !game_title(id, name, sizeof(name)))
        return;
    /* "Simple 2000 Series Vol.105 - The Maid Fuku to Kikanjuu" breaks at its dash; any other long name, at the space
     * nearest its middle */
    if (!(cut = strstr(name, " - ")) && ui_measure(FONT_BROWSER, name) > TITLE_W) {
        size_t half = strlen(name) / 2;
        skip = 1;
        for (p = name; (p = strchr(p, ' ')) != NULL; p++)
            if (!cut || labs((long)(p - name) - (long)half) < labs((long)(cut - name) - (long)half))
                cut = p;
    }
    if (cut) {
        *cut = 0;
        snprintf(v->line2, sizeof(v->line2), "%s", cut + skip);
        fit(FONT_BROWSER, v->line2, sizeof(v->line2), TITLE_W);
    }
    snprintf(v->line1, sizeof(v->line1), "%s", name);
    fit(FONT_BROWSER, v->line1, sizeof(v->line1), TITLE_W);
}

static void grid_cell(int i, float height, float *cx, float *cy)
{
    int k = i - brw.top * GRID_COLS;
    icon_cell_point(k % GRID_COLS, k / GRID_COLS, height, cx, cy);
}

/* the icons of the rows on screen; spin = the selected one turns */
void browser_icons(int spin)
{
    int i, first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    for (i = first; i < brw.n && i < last; i++) {
        float cx, cy;
        save_view_t *v = &brw.saves[i];
        if (v->icon) {
            int k = i - first;
            icon_draw_cell(v->icon, k % GRID_COLS, k / GRID_COLS, spin && i == brw.cursor ? (now_ms() - brw.since) / 1000.0f : -1);
        } else if (v->tried && cubeIcon) {   /* a save without an icon: the blue cube the PS2 browser gives it */
            int k = i - first;
            icon_draw_cell(cubeIcon, k % GRID_COLS, k / GRID_COLS, spin && i == brw.cursor ? (now_ms() - brw.since) / 1000.0f : -1);
        } else if (v->tried) {
            grid_cell(i, 2.5f, &cx, &cy);
            ui_rect(cx - 16, cy - 16, 32, 32, 0x505056, 0x60);
        }
    }
}

static void scene_browser(float t)
{
    char s[120];
    int first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    look_space();   /* the browser's layout, over the same space as the other screens */
    ui_alpha(look_fade(t));
    /* the card at the top left, its free space below */
    ui_image(IMG_MINICARD, 62, 38, 24, 28, 0xFFFFFF, 0x80);
    ui_text_shadow(FONT_BROWSER, 98, 32, 0xF4F4F4, brw.card->base);
    if (brw.card == &allGames) {   /* no card's free space to tell: how many saves there are, all together */
        snprintf(s, sizeof(s), T(T_SAVES_COUNT), brw.n);
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, s);
    } else if (brw.freeBytes >= 0) {
        snprintf(s, sizeof(s), T(T_FREE_KB), (int)(brw.freeBytes / 1024));
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, s);
    }
    if (!brw.n)   /* (a card that couldn't be read isn't an empty one) */
        ui_text_center(FONT_BROWSER, W / 2.0f, 200, 0xF0F0F0, T(brwUnread ? T_CARD_UNREADABLE : T_CARD_EMPTY));
    /* the selected save: a white light behind it, it turns; its name in yellow at the top right */
    if (brw.n) {
        float cx, cy;
        save_view_t *v = &brw.saves[brw.cursor];
        grid_cell(brw.cursor, 0.6f, &cx, &cy);   /* like the browser: a light at the selected icon's base */
        look_halo(cx, cy, 46, 0x34);
        if (v->line1[0] || v->line2[0]) {
            if (v->line1[0])
                ui_text_right(FONT_BROWSER, 596, 32, 0xE6E640, v->line1);
            if (v->line2[0])
                ui_text_right(FONT_BROWSER, 596, 32 + ui_line_height(FONT_BROWSER), 0xE6E640, v->line2);
        } else if (v->tried)
            ui_text_right(FONT_BROWSER, 596, 32, 0xE6E640, v->s.folder);
    }
    browser_icons(1);
    if (brw.top > 0)
        look_arrow(W / 2.0f, 92, 0, 0x3A5AE0);
    if (last < brw.n)
        look_arrow(W / 2.0f, 340, 1, 0x3A5AE0);
    {
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)},
                         {BUTTON_SQUARE, T(brw.card == &fileCard ? T_INSTALL : T_SYNC)}};
        look_legend(l, brw.card == &allGames ? 2 : 3, 0);   /* (no one card to sync there) */
    }
}

/* reads the icon of the first save on screen that wasn't read yet; 0 = there was none */
static int load_next_icon(void)
{
    int i, first = brw.top * GRID_COLS, last = first + GRID_COLS * GRID_ROWS;
    /* the selected one first */
    i = brw.saves[brw.cursor].tried || !brw.n ? -1 : brw.cursor;
    for (i = i >= 0 ? i : first; i >= 0 && i < brw.n && i < last && brw.saves[i].tried; i++)
        ;
    if (i < 0 || i >= brw.n || i >= last)
        return 0;
    {
        buffer_t iconsys = {0}, ico = {0};
        icon_t *ic = NULL;
        if (mcfs_save_icon(brw.saves[i].card->path, &brw.saves[i].s, &iconsys, &ico) == 0)
            ic = icon_load(&iconsys, &ico);
        buf_free(&iconsys);
        buf_free(&ico);
        ui_lock();
        brw.saves[i].icon = ic;
        brw.saves[i].tried = 1;
        save_title(&brw.saves[i], ic);
        ui_unlock();
    }
    return 1;
}

/* only the icons of the rows on screen (and one row around) stay in memory */
static void drop_far_icons(void)
{
    int i, lo = (brw.top - 1) * GRID_COLS, hi = (brw.top + GRID_ROWS + 1) * GRID_COLS;
    ui_lock();
    for (i = 0; i < brw.n; i++)
        if ((i < lo || i >= hi) && brw.saves[i].icon) {
            icon_free(brw.saves[i].icon);
            brw.saves[i].icon = NULL;
            brw.saves[i].tried = 0;
        }
    ui_unlock();
}

void browser_close(void)
{
    int i;
    ui_scene(scene_frame);   /* off the screen before the icons go */
    ui_lock();
    for (i = 0; i < brw.n; i++)
        icon_free(brw.saves[i].icon);
    brw.n = 0;
    ui_unlock();
}

/* the newest save's icon, for the big card (history) */
icon_t *newest_icon(const card_t *c)
{
    buffer_t iconsys = {0}, ico = {0};
    char folder[33];
    icon_t *ic = NULL;
    if (c->type == TYPE_GAMEID && mcfs_newest_save_icon(c->path, folder, &iconsys, &ico) == 0)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    return ic;
}

/* -------- one card's saves, the PS2 browser's way */

static save_view_t *brwSaves;   /* BRW_MAX of them, for whichever screen of saves is open */
static mcfs_save_t brwList[MCFS_MAX_SAVES];

/* the screen's saves are these n from now on, with the cursor there (with ui_lock held) */
static void browser_set(card_t *c, int n, int cursor, long long freeBytes)
{
    brw.card = c;
    brw.saves = brwSaves;
    brw.n = n;
    brw.cursor = cursor < n ? cursor : n > 0 ? n - 1 : 0;
    brw.top = 0;
    while (brw.cursor / GRID_COLS >= brw.top + GRID_ROWS)
        brw.top++;
    brw.freeBytes = freeBytes;
    brw.since = now_ms();
}

/* the icons of the saves on screen go: the list is about to be read again (with ui_lock held) */
static void browser_drop_icons(void)
{
    int i;
    for (i = 0; i < brw.n; i++)
        icon_free(brw.saves ? brw.saves[i].icon : NULL);
    brw.n = 0;
}

static void browser_load(card_t *c, int cursor)
{
    long long freeBytes = -1;
    int n, i;
    if (!brwSaves && !(brwSaves = calloc(BRW_MAX, sizeof(save_view_t))))
        return;
    n = mcfs_list_saves(c->path, brwList, MCFS_MAX_SAVES, &freeBytes);
    brwUnread = n < 0;
    if (n < 0) {
        log_msg("%s: its saves couldn't be read (%d)", c->id, n);
        n = 0;
    }
    ui_lock();
    browser_drop_icons();   /* a list read again (after a move or a delete): the old icons go */
    memset(brwSaves, 0, sizeof(save_view_t) * n);
    for (i = 0; i < n; i++) {
        brwSaves[i].s = brwList[i];
        brwSaves[i].card = c;
    }
    browser_set(c, n, cursor, freeBytes);
    ui_unlock();
    log_msg("%s: %d saves, %lld bytes free", c->id, n, freeBytes);
}

/* the newest first, whichever card each is on; two of the same moment, by name */
static int newer_view(const void *a, const void *b)
{
    const save_view_t *x = a, *y = b;
    return x->s.when < y->s.when ? 1 : x->s.when > y->s.when ? -1 : strcmp(x->s.folder, y->s.folder);
}

/* a card's saves onto the end of the n the screen has, for the screen of every game card's (with ui_lock held: the
 * list is brwList, read before). Returns how many there are then */
static int browser_append(card_t *c, int n, int count)
{
    int i;
    for (i = 0; i < count && n < BRW_MAX; i++, n++) {
        memset(&brwSaves[n], 0, sizeof(brwSaves[0]));
        brwSaves[n].s = brwList[i];
        brwSaves[n].card = c;
    }
    return n;
}

/* every game card's saves as those of one card. Each card's list is read in turn (a moment each on the sd2psx): a
 * bar meanwhile */
static void browser_load_all(void)
{
    int i, n = 0, count, k = 0, total = 0;
    if (!brwSaves && !(brwSaves = calloc(BRW_MAX, sizeof(save_view_t))))
        return;
    for (i = 0; i < nCards; i++)
        total += cards[i].type == TYPE_GAMEID;
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_LOADING));
    dlg_bar(0, NULL);
    dlg_show();
    ui_lock();
    browser_drop_icons();
    snprintf(allGames.base, sizeof(allGames.base), "%s", T(T_TAB_GAMES));
    snprintf(allGames.id, sizeof(allGames.id), "every game card");
    allGames.type = TYPE_GAMEID;
    ui_unlock();
    for (i = 0; i < nCards; i++) {
        if (cards[i].type != TYPE_GAMEID)
            continue;
        count = mcfs_list_saves(cards[i].path, brwList, MCFS_MAX_SAVES, NULL);
        n = browser_append(&cards[i], n, count < 0 ? 0 : count);   /* (nothing draws them yet: brw.n is 0) */
        ui_lock();
        dlg.permille = total ? ++k * 1000 / total : 1000;
        ui_unlock();
    }
    qsort(brwSaves, n, sizeof(brwSaves[0]), newer_view);
    ui_lock();
    browser_set(&allGames, n, 0, -1);
    ui_unlock();
    log_msg("every game card: %d saves on %d cards", n, total);
}

/* on that screen, the saves of one card again (one was copied to it, moved or deleted): the others stay as they are,
 * icons and all */
static void browser_refresh(card_t *c)
{
    int count = mcfs_list_saves(c->path, brwList, MCFS_MAX_SAVES, NULL), i, k = 0;
    ui_scene(scene_frame);   /* nothing of the list on screen while it changes */
    ui_lock();
    for (i = 0; i < brw.n; i++) {
        if (brw.saves[i].card == c)
            icon_free(brw.saves[i].icon);
        else
            brw.saves[k++] = brw.saves[i];
    }
    k = browser_append(c, k, count < 0 ? 0 : count);
    qsort(brwSaves, k, sizeof(brwSaves[0]), newer_view);
    browser_set(&allGames, k, brw.cursor, -1);
    ui_unlock();
    log_msg("every game card: %s read again, %d saves in all", c->id, k);
}

/* a card's saves. &allGames = every game card's, as if they were on one card */
void card_screen(card_t *c)
{
    int all = c == &allGames;
    brw.n = 0;
    if (all)
        browser_load_all();
    else
        browser_load(c, 0);
    ui_scene(scene_browser);
    for (;;) {
        /* reads the icons one by one while nobody presses anything */
        int pending = 1, n = brw.n;
        u32 b, keys = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE | (all ? 0 : PAD_SQUARE);
        while (pending && !(b = wait_nav_ms(keys, 0)))
            pending = load_next_icon();
        if (!pending)
            b = wait_nav(keys);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            browser_close();
            return;
        }
        if (b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
            int k = brw.cursor;
            if (b & PAD_LEFT)
                k = k > 0 ? k - 1 : k;
            else if (b & PAD_RIGHT)
                k = k < n - 1 ? k + 1 : k;
            else if (b & PAD_UP)
                k = k >= GRID_COLS ? k - GRID_COLS : k;
            else if (b & PAD_DOWN)
                k = k + GRID_COLS < n ? k + GRID_COLS : (k / GRID_COLS < (n - 1) / GRID_COLS ? n - 1 : k);
            if (k != brw.cursor) {
                ui_lock();
                brw.cursor = k;
                brw.since = now_ms();
                if (k / GRID_COLS < brw.top)
                    brw.top = k / GRID_COLS;
                if (k / GRID_COLS >= brw.top + GRID_ROWS)
                    brw.top = k / GRID_COLS - GRID_ROWS + 1;
                ui_unlock();
                drop_far_icons();
                sound_play(SND_MOVE);
            }
        } else if ((b & PAD_CROSS) && n) {
            card_t *on = brw.saves[brw.cursor].card;
            int changed;
            sound_play(SND_CONFIRM);
            transferDest = NULL;
            changed = save_screen(on, brw.cursor);
            if (all) {   /* only the cards that changed are read again */
                if (changed)
                    browser_refresh(on);
                if (transferDest && transferDest->type == TYPE_GAMEID)
                    browser_refresh(transferDest);
            } else if (changed)
                browser_load(c, brw.cursor);
            ui_scene(scene_browser);
        } else if (b & PAD_SQUARE) {
            sound_play(SND_CONFIRM);
            if (c == &fileCard)
                install_card();
            else
                sync_card(c);
            ui_scene(scene_browser);
        }
    }
}
