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
/* where a save's icon comes from when the saves on screen aren't a card's (NULL = they are): a template's, each in
 * its .psu file */
int (*brwIcon)(const save_view_t *v, buffer_t *iconsys, buffer_t *ico);

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
    if (marks.on) {   /* marking: how many saves are marked, on all the cards */
        snprintf(s, sizeof(s), T(T_MARKED_N), marks.n);
        ui_text_shadow(FONT_TEXT, 100, 60, COLOR_OK, s);
    } else if (brw.note[0])
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, brw.note);
    else if (brw.card == &allGames) {   /* no card's free space to tell: how many saves there are, all together */
        snprintf(s, sizeof(s), T(T_SAVES_COUNT), brw.n);
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, s);
    } else if (brw.freeBytes >= 0) {
        snprintf(s, sizeof(s), T(T_FREE_KB), (int)(brw.freeBytes / 1024));
        ui_text_shadow(FONT_TEXT, 100, 60, 0xE6E6E6, s);
    }
    if (!brw.n)   /* (a card that couldn't be read isn't an empty one) */
        ui_text_center(FONT_BROWSER, W / 2.0f, 200, 0xF0F0F0,
                       T(brw.emptyText ? brw.emptyText : brwUnread ? T_CARD_UNREADABLE : T_CARD_EMPTY));
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
    if (marks.on) {   /* each marked save: a green tick over its icon, in a soft light */
        int i;
        for (i = first; i < brw.n && i < last; i++) {
            float cx, cy;
            int k;
            if (marked(brw.saves[i].card, brw.saves[i].s.folder) < 0)
                continue;
            grid_cell(i, 4.2f, &cx, &cy);
            ui_light(cx + 22, cy + 2, 16, 16, COLOR_OK, 0x60);
            for (k = 0; k < 3; k++) {
                ui_line(cx + 15, cy + 1 + k, cx + 20, cy + 6 + k, COLOR_OK, 0x80);
                ui_line(cx + 20, cy + 6 + k, cx + 30, cy - 5 + k, COLOR_OK, 0x80);
            }
        }
    }
    if (brw.top > 0)
        look_arrow(W / 2.0f, 92, 0, 0x3A5AE0);
    if (last < brw.n)
        look_arrow(W / 2.0f, 340, 1, 0x3A5AE0);
    if (marks.on) {   /* (START says what the marking is for) */
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_MARK)},
                         {BUTTON_START, T(marks.copy ? T_PASTE : T_FINISH)}};
        look_legend(l, 3, 0);
    } else if (brw.nLegend)
        look_legend(brw.legend, brw.nLegend, 0);
    else {
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_TRIANGLE, T(T_OPTIONS)},
                         {BUTTON_SQUARE, T(T_INSTALL)}};
        look_legend(l, brw.card == &fileCard ? 4 : 3, 0);   /* (a card file is installed with square, as in the list of files) */
    }
}

void browser_scene(void) { ui_scene(scene_browser); }

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
        if ((brwIcon ? brwIcon(&brw.saves[i], &iconsys, &ico) : mcfs_save_icon(brw.saves[i].card->path, &brw.saves[i].s, &iconsys, &ico)) == 0)
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

/* What a save is sorted by, by name: its game's name (by the ID in its folder's name, when the list of games has
 * it), then its folder; one that tells no game, its folder alone. (The title on screen is in its icon.sys, which is
 * only read for the saves the screen is showing) */
#define SORT_KEY 72
static void sort_key(const char *folder, char out[SORT_KEY])
{
    static char lastId[12], lastName[96];   /* (a card's saves are mostly of one game) */
    char id[12];
    if (save_game_id(folder, id)) {
        if (strcmp(id, lastId) != 0) {
            snprintf(lastId, sizeof(lastId), "%s", id);
            if (!game_title(id, lastName, sizeof(lastName)))
                lastName[0] = 0;
        }
        if (lastName[0]) {
            snprintf(out, SORT_KEY, "%s %s", lastName, folder);
            return;
        }
    }
    snprintf(out, SORT_KEY, "%s", folder);
}

/* a card's list (brwList, the newest first, as it is read) by name: the ones of the same name stay as they were */
static void list_by_name(int n)
{
    static char key[MCFS_MAX_SAVES][SORT_KEY];
    char t[SORT_KEY];
    int i, k;
    for (i = 0; i < n; i++)
        sort_key(brwList[i].folder, key[i]);
    for (i = 1; i < n; i++) {   /* (few of them: each one into its place) */
        mcfs_save_t s = brwList[i];
        memcpy(t, key[i], SORT_KEY);
        for (k = i; k > 0 && strcasecmp(key[k - 1], t) > 0; k--) {
            brwList[k] = brwList[k - 1];
            memcpy(key[k], key[k - 1], SORT_KEY);
        }
        brwList[k] = s;
        memcpy(key[k], t, SORT_KEY);
    }
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
    if (cfg.sort == SORT_NAME)
        list_by_name(n);
    else if (cfg.sort == SORT_OLDEST)   /* (it is read the newest first) */
        for (i = 0; i < n / 2; i++) {
            mcfs_save_t s = brwList[i];
            brwList[i] = brwList[n - 1 - i];
            brwList[n - 1 - i] = s;
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
    if (cfg.sort != SORT_NEWEST && n)
        log_msg("%s: %s, from %s", c->id, cfg.sort == SORT_NAME ? "by name" : "the oldest first", brwSaves[0].s.folder);
}

/* The screen's saves are these n, which aren't read from a card (a template's: brwIcon gives their icons). c = what
 * stands for a card on screen: its name is shown where a card's would be */
void browser_fill(card_t *c, const mcfs_save_t *list, int n, int cursor)
{
    int i;
    if (!brwSaves && !(brwSaves = calloc(BRW_MAX, sizeof(save_view_t))))
        return;
    ui_lock();
    browser_drop_icons();
    memset(brwSaves, 0, sizeof(save_view_t) * n);
    for (i = 0; i < n; i++) {
        brwSaves[i].s = list[i];
        brwSaves[i].card = c;
    }
    brwUnread = 0;
    browser_set(c, n, cursor, -1);
    ui_unlock();
}

/* -------- every game card's saves on one screen ("All saves")
 *
 * The saves as the cards have them are kept apart (allSaves); what the screen shows of them is made from that, in
 * the order the settings say (the newest first, or by name), whichever card each is on. The same save on more than
 * one card is there once, unless the settings say otherwise: the same folder with the same files, byte for byte,
 * which is what a copy of a save is (a template put into every game card would be all over the screen). Two saves of
 * the same name with anything different in them are different saves, a game's progress on two cards, and both are
 * there; so is one whose files couldn't be read. Dates don't tell: a console whose clock stands still gives every
 * save the same one. */

typedef struct {
    mcfs_save_t s;
    card_t *card;
    char key[SORT_KEY];        /* what it is sorted by, by name ("" = not found out yet) */
    unsigned long long sig;    /* what tells it from another save of its folder (mcfs_save_signatures; 0 = not read) */
    int asked, seen;           /* that was asked for; (all_sign) it was looked at, this time around */
} all_save_t;
static all_save_t *allSaves;   /* BRW_MAX of them */
static int nAll;

/* the newest first; two of the same moment, by name, and the same save on two cards, by card */
static int newer_all(const void *a, const void *b)
{
    const all_save_t *x = a, *y = b;
    int r = x->s.when < y->s.when ? 1 : x->s.when > y->s.when ? -1 : strcmp(x->s.folder, y->s.folder);
    return r ? r : x->card < y->card ? -1 : x->card > y->card;
}

/* the oldest first */
static int older_all(const void *a, const void *b)
{
    const all_save_t *x = a, *y = b;
    return x->s.when != y->s.when ? (x->s.when < y->s.when ? -1 : 1) : newer_all(a, b);
}

/* by name; the ones of the same name, the newest first */
static int named_all(const void *a, const void *b)
{
    const all_save_t *x = a, *y = b;
    int r = strcasecmp(x->key, y->key);
    return r ? r : newer_all(a, b);
}

/* a card's saves (brwList, read before) onto the end of allSaves */
static void all_append(card_t *c, int count)
{
    int i;
    for (i = 0; i < count && nAll < BRW_MAX; i++, nAll++) {
        allSaves[nAll].s = brwList[i];
        allSaves[nAll].card = c;
        allSaves[nAll].key[0] = 0;
        allSaves[nAll].sig = 0;
        allSaves[nAll].asked = 0;
    }
}

/* What was found out of a save's files (all_sign), kept while the program is open: every game card's saves are
 * looked at again each time their screen opens, and a save of a card that didn't change since (its fingerprint
 * tells) isn't read again for it */
#define KEPT_MAX 512
static struct {
    const card_t *card;
    char print[17], folder[33];
    unsigned long long when, sig;
} *kept;
static int nKept, keptNext;

static int kept_find(const card_t *c, const mcfs_save_t *s)
{
    int i;
    for (i = 0; kept && c->fingerprint[0] && i < nKept; i++)
        if (kept[i].card == c && kept[i].when == s->when && !strncmp(kept[i].print, c->fingerprint, 16) && !strcmp(kept[i].folder, s->folder))
            return i;
    return -1;
}

static void kept_add(const card_t *c, const mcfs_save_t *s, unsigned long long sig)
{
    int i;
    if (!sig || !c->fingerprint[0] || (!kept && !(kept = calloc(KEPT_MAX, sizeof(kept[0])))))
        return;
    for (i = 0; i < nKept && !(kept[i].card == c && !strcmp(kept[i].folder, s->folder)); i++)
        ;
    if (i == nKept && nKept < KEPT_MAX)
        nKept++;
    else if (i == nKept)   /* (full: the oldest ones give way) */
        i = keptNext++ % KEPT_MAX;
    kept[i].card = c;
    snprintf(kept[i].print, sizeof(kept[i].print), "%.16s", c->fingerprint);
    snprintf(kept[i].folder, sizeof(kept[i].folder), "%s", s->folder);
    kept[i].when = s->when;
    kept[i].sig = sig;
}

/* Before the same save on more than one card is shown once: the saves that may be such (the same folder, the same
 * date, on different cards) are told apart by what is in their files, read now, a card at a time (not with ui_lock
 * held). One that was read before isn't read again */
static void all_sign(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    static unsigned long long sig[MCFS_MAX_SAVES];
    static int which[MCFS_MAX_SAVES];
    int i, j, k, n;
    if (cfg.show_repeated)
        return;
    for (i = 0; i < nAll; i++)
        allSaves[i].seen = 0;
    for (i = 0; i < nAll; i++) {
        card_t *c = allSaves[i].card;
        if (allSaves[i].seen)
            continue;
        for (n = 0, k = i; k < nAll; k++) {   /* this card's, from here on */
            if (allSaves[k].card != c)
                continue;
            allSaves[k].seen = 1;
            if (allSaves[k].asked || n == MCFS_MAX_SAVES)
                continue;
            for (j = 0; j < nAll; j++)
                if (allSaves[j].card != c && allSaves[j].s.when == allSaves[k].s.when && !strcmp(allSaves[j].s.folder, allSaves[k].s.folder))
                    break;
            if (j == nAll)
                continue;
            if ((j = kept_find(c, &allSaves[k].s)) >= 0) {
                allSaves[k].sig = kept[j].sig;
                allSaves[k].asked = 1;
                continue;
            }
            list[n] = allSaves[k].s;
            which[n++] = k;
        }
        if (!n)
            continue;
        mcfs_save_signatures(c->path, list, n, sig);
        for (k = 0; k < n; k++) {
            allSaves[which[k]].sig = sig[k];
            allSaves[which[k]].asked = 1;
            kept_add(c, &list[k], sig[k]);
        }
    }
}
/* the screen's saves, made from allSaves, with the cursor there (with ui_lock held). An icon the screen had read
 * stays with its save; the icons of the saves that are no longer there go */
static void all_show(int cursor)
{
    save_view_t *old = NULL;
    int i, k, n = 0, was = brw.n;
    if (was && (old = malloc(sizeof(*old) * was)) != NULL)
        memcpy(old, brwSaves, sizeof(*old) * was);
    else
        browser_drop_icons();   /* (none to keep, or no memory to: they are read again) */
    for (i = 0; cfg.sort == SORT_NAME && i < nAll; i++)
        if (!allSaves[i].key[0])
            sort_key(allSaves[i].s.folder, allSaves[i].key);
    qsort(allSaves, nAll, sizeof(allSaves[0]), cfg.sort == SORT_NAME ? named_all : cfg.sort == SORT_OLDEST ? older_all : newer_all);
    for (i = 0; i < nAll; i++) {
        const char *folder = allSaves[i].s.folder;
        if (!cfg.show_repeated && allSaves[i].sig) {
            for (k = 0; k < i && (allSaves[k].sig != allSaves[i].sig || allSaves[k].s.when != allSaves[i].s.when ||
                                  strcmp(allSaves[k].s.folder, folder) != 0); k++)
                ;
            if (k < i)   /* (that very save is there already, from another card) */
                continue;
        }
        memset(&brwSaves[n], 0, sizeof(brwSaves[0]));
        brwSaves[n].s = allSaves[i].s;
        brwSaves[n].card = allSaves[i].card;
        for (k = 0; old && k < was; k++)
            if (old[k].tried && old[k].card == allSaves[i].card && !strcmp(old[k].s.folder, folder)) {
                brwSaves[n].icon = old[k].icon;
                brwSaves[n].tried = 1;
                memcpy(brwSaves[n].line1, old[k].line1, sizeof(old[k].line1));
                memcpy(brwSaves[n].line2, old[k].line2, sizeof(old[k].line2));
                old[k].icon = NULL;
                old[k].tried = 0;
                break;
            }
        n++;
    }
    for (k = 0; old && k < was; k++)
        icon_free(old[k].icon);
    free(old);
    browser_set(&allGames, n, cursor, -1);
}

/* every game card's saves as those of one card. Each card's list is read in turn (a moment each on the sd2psx): a
 * bar meanwhile */
static void browser_load_all(void)
{
    int i, count, k = 0, total = 0;
    if (!brwSaves && !(brwSaves = calloc(BRW_MAX, sizeof(save_view_t))))
        return;
    if (!allSaves && !(allSaves = calloc(BRW_MAX, sizeof(all_save_t))))
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
    nAll = 0;
    for (i = 0; i < nCards; i++) {
        if (cards[i].type != TYPE_GAMEID)
            continue;
        count = mcfs_list_saves(cards[i].path, brwList, MCFS_MAX_SAVES, NULL);
        all_append(&cards[i], count < 0 ? 0 : count);   /* (nothing draws them yet: brw.n is 0) */
        ui_lock();
        dlg.permille = total ? ++k * 1000 / total : 1000;
        ui_unlock();
    }
    all_sign();
    ui_lock();
    all_show(0);
    ui_unlock();
    log_msg("every game card: %d saves on %d cards, %d shown", nAll, total, brw.n);
}

/* on that screen, the saves of one card again (one was copied to it, moved or deleted): the others stay as they are,
 * icons and all */
static void browser_refresh(card_t *c)
{
    int count = mcfs_list_saves(c->path, brwList, MCFS_MAX_SAVES, NULL), i, k = 0;
    ui_scene(scene_frame);   /* nothing of the list on screen while it changes */
    ui_lock();
    for (i = 0; i < nAll; i++)
        if (allSaves[i].card != c)
            allSaves[k++] = allSaves[i];
    nAll = k;
    all_append(c, count < 0 ? 0 : count);
    ui_unlock();
    all_sign();
    ui_lock();
    all_show(brw.cursor);
    ui_unlock();
    log_msg("every game card: %s read again, %d saves in all, %d shown", c->id, nAll, brw.n);
}

/* Waits on the screen of saves for one of those buttons. Meanwhile the icons are read, one by one while nobody
 * presses anything, and the arrows move the cursor */
u32 browser_wait(u32 buttons)
{
    for (;;) {
        int pending = 1, n = brw.n, k = brw.cursor;
        u32 b, keys = PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | buttons;
        while (pending && !(b = wait_nav_ms(keys, 0)))
            pending = load_next_icon();
        if (!pending)
            b = wait_nav(keys);
        if (!(b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)))
            return b;
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
    }
}

/* -------- triangle on a card's saves: its options, in a box. "Sync now", for a card of the microSD; the order the
 * saves are in (by date, the newest first or the oldest, or by name), which is every such screen's; and, on the
 * screen of every game card's, the same save on more than one card shown once or each time. X changes the value of
 * the row it is on, and nothing on the screen underneath: the saves are shown the new way once, when the box is
 * closed. The settings keep both */

enum { OPT_SYNC, OPT_SORT, OPT_REPEATED };
#define OPT_W 380
static struct {
    int n, cursor, id[3];
    int sort, repeated;   /* as the rows say, for now */
} opt;

static const int sortText[] = {T_SORT_NEWEST, T_SORT_OLDEST, T_SORT_NAME};

static void scene_options(float t)
{
    int w = OPT_W, h = 56 + opt.n * 32 + 24, x = (W - w) / 2, y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, w, h);
    look_title(W / 2.0f, y + 20, T(T_OPTIONS), 1);
    for (i = 0; i < opt.n; i++) {   /* what it is on the left, how it is now on the right */
        const char *label = T(opt.id[i] == OPT_SYNC ? T_SYNC_NOW : opt.id[i] == OPT_SORT ? T_SORT_BY : T_REPEATED);
        const char *value = opt.id[i] == OPT_SORT ? T(sortText[opt.sort])
                            : opt.id[i] == OPT_REPEATED ? T(opt.repeated ? T_REPEATED_SHOWN : T_REPEATED_HIDDEN) : "";
        float ry = y + 56 + i * 32;
        if (i == opt.cursor) {
            look_glow_text(FONT_TEXT, x + 30, ry, COLOR_ITEM, COLOR_ITEM_ON, label);
            ui_text_right(FONT_TEXT, x + w - 30, ry, 0xD8E4F4, value);
        } else {
            ui_text(FONT_TEXT, x + 30, ry, COLOR_ITEM, label);
            ui_text_right(FONT_TEXT, x + w - 30, ry, COLOR_DIM, value);
        }
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(opt.id[opt.cursor] == OPT_SYNC ? T_SELECT : T_TPL_CHANGE)}};
        look_legend(l, 2, 0);
    }
}

/* the box, until it is closed. Returns 1 when the saves are to be shown another way, + 2 when the card is to be
 * synced */
static int browser_options(const card_t *c)
{
    static const char *const sortName[] = {"date", "date_asc", "name"};
    int r = 0;
    ui_lock();
    opt.n = opt.cursor = 0;
    if (c != &allGames && c != &fileCard)
        opt.id[opt.n++] = OPT_SYNC;
    opt.id[opt.n++] = OPT_SORT;
    if (c == &allGames)
        opt.id[opt.n++] = OPT_REPEATED;
    opt.sort = cfg.sort;
    opt.repeated = cfg.show_repeated;
    ui_unlock();
    ui_scene(scene_options);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            break;
        }
        if (b & (PAD_UP | PAD_DOWN)) {
            ui_lock();
            opt.cursor = (b & PAD_UP) ? (opt.cursor + opt.n - 1) % opt.n : (opt.cursor + 1) % opt.n;
            ui_unlock();
            sound_play(SND_MOVE);
            continue;
        }
        sound_play(SND_CONFIRM);
        if (opt.id[opt.cursor] == OPT_SYNC) {
            r = 2;
            break;
        }
        ui_lock();
        if (opt.id[opt.cursor] == OPT_SORT)
            opt.sort = (opt.sort + 1) % 3;
        else
            opt.repeated = !opt.repeated;
        ui_unlock();
    }
    if (opt.sort != cfg.sort) {
        cfg.sort = opt.sort;
        config_set("saves", "sort", sortName[cfg.sort]);
        r |= 1;
    }
    if (opt.repeated != cfg.show_repeated) {
        cfg.show_repeated = opt.repeated;
        config_set("saves", "repeated", cfg.show_repeated ? "show" : "hide");
        r |= 1;
    }
    return r;
}

/* a card's saves. &allGames = every game card's, as if they were on one card. While saves are being marked (marks.c)
 * X marks and unmarks the one under the cursor and START ends the marking: for a template, back in the list of cards
 * this was opened from; for a copy that began here (a save's "Copy"), on to where the marked saves go */
void card_screen(card_t *c)
{
    int all = c == &allGames, mine = 0;   /* mine = the marking going on began on this screen: a copy */
    brw.n = 0;
    brw.note[0] = 0;
    brw.emptyText = brw.nLegend = 0;
    if (all)
        browser_load_all();
    else
        browser_load(c, 0);
    ui_scene(scene_browser);
    for (;;) {
        u32 b = browser_wait(PAD_CROSS | PAD_CIRCLE | (marks.on ? PAD_START : PAD_TRIANGLE | (c == &fileCard ? PAD_SQUARE : 0)));
        int n = brw.n;
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            if (mine && !copy_leave(c)) {   /* (the screen stays: marking still, or with nothing marked any more) */
                mine = marks.on;
                ui_scene(scene_browser);
                continue;
            }
            browser_close();
            return;
        }
        if (marks.on) {   /* X marks and unmarks, START ends the marking (with something marked) */
            if ((b & PAD_CROSS) && n)
                sound_play(mark_toggle(&brw.saves[brw.cursor]) ? SND_CONFIRM : SND_BACK);
            else if ((b & PAD_START) && marks.n && mine) {
                sound_play(SND_CONFIRM);
                transferDest = NULL;
                if (copy_marked(all ? NULL : c)) {   /* copied: this is a card's saves again, with the card they went to read again */
                    marks_reset();
                    mine = 0;
                    if (all && transferDest && transferDest->type == TYPE_GAMEID)
                        browser_refresh(transferDest);
                }
                ui_scene(scene_browser);
            } else if ((b & PAD_START) && marks.n) {
                sound_play(SND_CONFIRM);
                marks.into = all ? NULL : c;
                marks.done = 1;
                browser_close();
                return;
            } else if (b & PAD_START)
                sound_play(SND_BACK);
            continue;
        }
        if ((b & PAD_CROSS) && n) {
            card_t *on = brw.saves[brw.cursor].card;
            int changed;
            sound_play(SND_CONFIRM);
            transferDest = NULL;
            changed = save_screen(on, brw.cursor);
            if (marks.on)   /* "Copy": the save is marked, and so can others be, here and on the other cards */
                mine = 1;
            else if (all) {   /* only the cards that changed are read again */
                if (changed)
                    browser_refresh(on);
                if (transferDest && transferDest->type == TYPE_GAMEID)
                    browser_refresh(transferDest);
            } else if (changed)
                browser_load(c, brw.cursor);
            ui_scene(scene_browser);
        } else if (b & PAD_TRIANGLE) {
            int k;
            sound_play(SND_CONFIRM);
            k = browser_options(c);
            if ((k & 1) && all) {   /* shown another way: every game card's, from what was read */
                message(0, NULL, COLOR_TEXT, T(T_LOADING));
                all_sign();
                ui_lock();
                all_show(0);
                ui_unlock();
                log_msg("every game card: %d saves, %d shown%s", nAll, brw.n,
                        cfg.sort == SORT_NAME ? ", by name" : cfg.sort == SORT_OLDEST ? ", the oldest first" : "");
            } else if (k & 1)   /* a card's, read again */
                browser_load(c, 0);
            if (k & 2)
                sync_card(c);
            ui_scene(scene_browser);
        } else if (b & PAD_SQUARE) {   /* (a card file's saves) */
            sound_play(SND_CONFIRM);
            install_card();
            ui_scene(scene_browser);
        }
    }
}
