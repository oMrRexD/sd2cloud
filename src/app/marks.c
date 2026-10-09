/* SD2Cloud -- saves marked on the cards, whichever cards they are on, for what comes next: a template is made of them
 * (tools.c), or they are copied together to a card or to a folder (a save's "Copy"). The marks themselves, the list
 * of cards they are made from, and that copy. */
#include "app.h"

/* While this is on, X on a screen of saves (saves.c) marks and unmarks the save under the cursor instead of opening
 * it, and START ends the marking */
struct marks_s marks;

/* which mark that save has (-1 = none) */
int marked(const card_t *c, const char *folder)
{
    int i;
    for (i = 0; i < marks.n; i++)
        if (marks.save[i].card == c && !strcmp(marks.save[i].s.folder, folder))
            return i;
    return -1;
}

/* X while marking. What the saves go into has one save of each folder: marking one unmarks the one of that folder on
 * another card. 0 = no more can be marked */
int mark_toggle(const save_view_t *v)
{
    int k = marked(v->card, v->s.folder), i, ok = 1;
    ui_lock();
    if (k < 0)
        for (i = 0; i < marks.n; i++)
            if (!strcmp(marks.save[i].s.folder, v->s.folder))
                k = i;
    if (k >= 0) {
        int same = marks.save[k].card == v->card;
        memmove(&marks.save[k], &marks.save[k + 1], sizeof(marks.save[0]) * (marks.n - k - 1));
        marks.n--;
        if (same)
            k = -2;   /* (unmarked, and that is all) */
    }
    if (k != -2) {
        if (marks.n < MARKS_MAX) {
            marks.save[marks.n].card = v->card;
            marks.save[marks.n].s = v->s;
            marks.n++;
        } else
            ok = 0;
    }
    ui_unlock();
    return ok;
}

/* nothing is marked any more, and nothing is marking (the device was taken out, or what the marks were for is over) */
void marks_reset(void)
{
    ui_lock();
    memset(&marks, 0, sizeof(marks));
    ui_unlock();
}

/* ------------------------------------------------------------ the cards the saves are marked on
 *
 * The cards, in their groups as on the main screen; X opens one, and there X marks and unmarks a save (saves.c).
 * The marks stay from card to card. START, here or on a card, ends it. */

static struct {
    tabs_t g;
    char title[80];
    icon_t *icon;     /* the selected game card's icon, once read */
    int iconCard;
} mk = {.iconCard = -1};

static void scene_mark(float t)
{
    char s[64];
    const card_t *c;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    tabs_draw(&mk.g, 0);
    {
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)},
                         {BUTTON_START, T(marks.copy ? T_PASTE : T_FINISH)}};
        look_legend(l, 3, 0);
    }
    look_title(CARD_CX, 82, mk.title, 1);
    snprintf(s, sizeof(s), T(T_MARKED_N), marks.n);
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, marks.n ? COLOR_OK : COLOR_DIM, s);
    if (mk.g.tab == TAB_FILES)   /* a device: the marked saves can go to a folder of it */
        return;
    if (mk.g.n && mk.g.idx[mk.g.cursor] < 0) {   /* "All saves": the game cards together */
        look_card(CARD_X, CARD_Y, NULL, T(T_TAB_GAMES));
        return;
    }
    if ((c = tabs_card(&mk.g)) != NULL)
        draw_card_picture(c, mk.iconCard == mk.g.idx[mk.g.cursor] ? mk.icon : NULL, ui_clock());
}

static int copy_to_device(int device);

/* the selected game card's icon, read when the cursor rests a moment */
static void mark_icon(void)
{
    card_t *c = tabs_card(&mk.g);
    icon_t *ic;
    if (!c || mk.iconCard == mk.g.idx[mk.g.cursor])
        return;
    ic = newest_icon(c);
    ui_lock();
    icon_free(mk.icon);
    mk.icon = ic;
    mk.iconCard = mk.g.idx[mk.g.cursor];
    ui_unlock();
}

/* The list, until START with something marked (1) or circle with the marks let go of (0). at = the card it opens on
 * (&allGames = "All saves"; NULL = the first one). For a copy the list has the Files group too: X or START on a
 * device opens its folders, where START writes the marked saves as .psu files (2 = that was done) */
static int mark_cards(const card_t *at)
{
    int r = -1;
    ui_lock();
    marks.done = 0;
    marks.into = NULL;
    tabs_init(&mk.g, NULL, marks.copy);
    mk.g.all = 1;
    tabs_show(&mk.g, mk.g.tab);
    if (at == &allGames)
        tabs_show(&mk.g, TAB_GAMES);
    else if (at) {   /* (a game's card is inside its folder) */
        tabs_show(&mk.g, tab_of(at));
        tabs_find(&mk.g, (int)(at - cards));
        if (tabs_on_folders(&mk.g)) {
            tabs_open(&mk.g);
            mk.g.moved = -1;   /* (it opens on it: nothing slides) */
            tabs_find(&mk.g, (int)(at - cards));
        }
    }
    snprintf(mk.title, sizeof(mk.title), "%s", T(T_TPL_MARK_TITLE));
    mk.icon = NULL;
    mk.iconCard = -1;
    ui_unlock();
    while (r < 0) {
        u32 keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE | PAD_START, b;
        ui_scene(scene_mark);
        if (!(b = wait_nav_ms(keys, 250))) {
            mark_icon();
            b = wait_nav(keys);
        }
        if (tabs_nav(&mk.g, b) || tabs_folder_nav(&mk.g, b))
            continue;
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            if (!marks.n || confirm(T(T_MARK_DROP), NULL, T_MARK_DROP_YES))
                r = 0;
        } else if (mk.g.tab == TAB_FILES) {
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n && copy_to_device(mk.g.cursor))
                r = 2;
        } else if (b & PAD_START) {
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n) {   /* (on a card: that is the card a copy pastes them into) */
                marks.into = tabs_on_folders(&mk.g) ? NULL : tabs_card(&mk.g);
                r = 1;
            }
        } else if ((b & PAD_CROSS) && mk.g.n) {
            card_t *c = mk.g.idx[mk.g.cursor] < 0 ? &allGames : tabs_card(&mk.g);
            if (c) {
                sound_play(SND_CONFIRM);
                card_screen(c);
                if (marks.done)
                    r = 1;
            }
        }
    }
    ui_scene(scene_frame);   /* off the screen before the icon goes */
    ui_lock();
    icon_free(mk.icon);
    mk.icon = NULL;
    mk.iconCard = -1;
    ui_unlock();
    return r;
}

/* Saves marked for a template. 1 = saves were marked (marks) and START ended it; 0 = circle, and nothing is marked */
int mark_saves(void)
{
    int r;
    brwIcon = NULL;   /* (the cards' saves have their own icons: not the ones of a template that is open) */
    marks_reset();
    marks.on = 1;
    r = mark_cards(NULL);
    ui_lock();
    marks.on = 0;
    if (!r)
        marks.n = 0;
    ui_unlock();
    return r;
}

/* ------------------------------------------------------------ a marked save, on a screen
 *
 * A mark is a card and a save's folder. What shows it wants the save as the screens of saves have one, with its icon
 * and its name: read from the card, one at a time. */

static save_view_t shown;

/* shown is that marked save from now on, read now unless it is the one that is there. Returns the icon of the one
 * before, for the caller to let go of once nothing draws it (NULL = there is none to) */
static icon_t *view_read(int i)
{
    buffer_t iconsys = {0}, ico = {0};
    icon_t *ic = NULL, *old = shown.icon;
    card_t *c = marks.save[i].card;
    if (shown.tried && shown.card == c && !strcmp(shown.s.folder, marks.save[i].s.folder))
        return NULL;
    if (mcfs_save_icon(c->path, &marks.save[i].s, &iconsys, &ico) == 0)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    memset(&shown, 0, sizeof(shown));
    shown.s = marks.save[i].s;
    shown.card = c;
    shown.icon = ic;
    shown.tried = 1;
    save_title(&shown, ic);
    ui_unlock();
    return old;
}

/* that marked save, for a question that names it or a screen of its own (mark_show_done when done with it) */
save_view_t *mark_view(int i)
{
    icon_t *old = view_read(i);
    ui_lock();
    icon_free(old);
    ui_unlock();
    return &shown;
}

/* that marked save on its way: the screen of a save going into a card (work.c), counting it among the marked ones.
 * to = the card it goes to (NULL = it goes somewhere else: the card it is on is the one shown) */
void mark_show(const card_t *to, int i, int text)
{
    icon_t *old = view_read(i);
    save_into(to ? to : marks.save[i].card, &shown, text);   /* (from here on nothing draws the icon of the one before) */
    ui_lock();
    icon_free(old);
    currentN = i + 1;
    totalN = marks.n;
    ui_unlock();
}

void mark_show_done(void)
{
    save_into_done();
    ui_lock();
    icon_free(shown.icon);
    memset(&shown, 0, sizeof(shown));
    ui_unlock();
}

/* what became of the marked saves, said until a button is pressed: how many were copied and, when some weren't,
 * how many */
void marks_report(int copied, int failed)
{
    char t[100], w[100];
    snprintf(t, sizeof(t), T(T_TPL_COPIED_N), copied);
    dlg_new(failed ? COLOR_WARN : COLOR_OK, t);
    if (failed) {
        snprintf(w, sizeof(w), T(T_TPL_NOT_COPIED), failed);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, w);
    }
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* ------------------------------------------------------------ a save's "Copy"
 *
 * The save is marked, on the screen of saves it is on, and others can be: there, and on the other cards once that
 * screen is left (the list above). START pastes them into the card it is pressed on; with no card to take them
 * there, it asks where they go: a card, one that is there or a new one of their game, or a folder of a device, as
 * .psu files. */

/* "Copy" on a save's page. 1 = the save is marked and the marking is on: the screen of saves the page is over goes
 * on from there. 0 = there is nowhere to copy it to (said) */
int copy_start(const save_view_t *v)
{
    char game[12];
    if (!nCards && !save_game_id(v->s.folder, game)) {   /* a save of a card file, with no card on the microSD */
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
        return 0;
    }
    marks_reset();
    marks.on = marks.copy = 1;
    mark_toggle(v);
    return 1;
}

/* The marked saves to a folder of a device (FDEV_*), as .psu files: its folders are browsed, and START there
 * writes them into the one shown. 1 = written: the copy is over. 0 = the user came back: the marks stay */
static int copy_to_device(int device)
{
    fbGive.c = marks.save[0].card;
    fbGive.v = marks.n == 1 ? mark_view(0) : NULL;   /* (one save is asked about by its name) */
    fbGive.done = 0;
    files_screen(device);
    fbGive.c = NULL;
    mark_show_done();
    return fbGive.done;
}

/* START with saves marked for a copy ("Paste"). into = the card it was pressed on: the one whose saves were open,
 * or the one under the cursor in the list of cards. That card takes them, when it lacks at least one of them; with
 * no such card (START on the card they all are on, on every game card's saves, on a card file's) where they go is
 * picked: a card, a new one of their game, or a folder of a device. It is asked about, and they are copied one by
 * one, each read back and compared. A save the card has under the same name stays out, as one that is on that very
 * card does. 1 = it is over (they were copied, or couldn't be, and that was said): nothing is marked any more.
 * 0 = the user came back from it: the marks stay, and so does the marking */
int copy_marked(card_t *into)
{
    card_t *from = marks.save[0].card, *to = NULL;
    const char *game = marks.save[0].s.folder;   /* a save that tells their game, when they are all of one */
    char id[12], first[12], t[300];
    long long size[MARKS_MAX], need = 0;
    int i, n = 0, k = 0, total = 0, one = 0, last = MCFS_OK, r = MCFS_OK;
    if (marks.n > 1)   /* (each save's size is read from its card) */
        message(0, NULL, COLOR_TEXT, T(T_LOADING));
    if (into && (into < cards || into >= cards + nCards))
        into = NULL;   /* (not one of the cards: nothing is pasted into it) */
    save_game_id(game, first);
    for (i = 0; i < marks.n; i++) {
        int files = 0;
        size[i] = 0;
        if (marks.save[i].card != from)
            from = NULL;   /* of more than one card: none is left out of where they can go */
        if (!first[0] || !save_game_id(marks.save[i].s.folder, id) || strcmp(id, first) != 0)
            game = NULL;
        mcfs_save_info(marks.save[i].card->path, marks.save[i].s.folder, &size[i], &files);
        need += size[i];
        if (into && marks.save[i].card != into)
            to = into;   /* the card START was pressed on lacks this one: it is where they go */
    }
    if (to) {   /* a card without room for them is left as it is (picking one, such a card can't be picked either) */
        long long room = -1;
        for (need = 0, i = 0; i < marks.n; i++)
            if (marks.save[i].card != to)
                need += size[i];
        if (mcfs_list_saves(to->path, NULL, 0, &room) >= 0 && room >= 0 && room < need) {
            snprintf(t, sizeof(t), T(T_ERR_FULL), to->base);
            message_wait(0, NULL, COLOR_WARN, t);
            return 0;
        }
    } else {
        if (!nCards && !game) {
            message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
            return 0;
        }
        /* they can also go to a folder of the microSD or of a USB drive, as .psu files (not from a card file: the
         * folders are already being browsed, on the screen underneath) */
        destFiles = marks.save[0].card != &fileCard;
        if (!(to = choose_dest(from, T_COPY_TO, need, game)))
            return destDevice >= 0 && copy_to_device(destDevice);
    }
    for (i = 0; i < marks.n; i++)   /* how many go: the ones that card doesn't have already */
        if (marks.save[i].card != to) {
            total++;
            one = i;
        }
    if (total == 1) {
        char name[100];
        save_name(mark_view(one), name, sizeof(name));
        snprintf(t, sizeof(t), T(T_CONFIRM_COPY), name, to->base);
    } else
        snprintf(t, sizeof(t), T(T_CONFIRM_COPY_N), total, to->base);
    if (!confirm(t, to == &destNew ? T(T_NEWCARD_NOTE) : NULL, T_COPY) || !(to = dest_real(to)) || card_free(to, from)) {
        card_back();
        mark_show_done();
        return 0;
    }
    transferDest = NULL;
    for (i = 0; i < marks.n && r != MCFS_ERR_CANCELLED; i++) {   /* (given up: the ones copied before it stay) */
        card_t *c = marks.save[i].card;
        if (c == to)   /* (it is there already) */
            continue;
        mark_show(to, i, T_WORKING_COPY);
        ui_lock();
        currentN = ++k;
        totalN = total;
        ui_unlock();
        r = mcfs_copy_save(c->path, marks.save[i].s.folder, to->path, save_progress);
        log_msg("copy %s from %s to %s: %d", marks.save[i].s.folder, c->id, to->id, r);
        if (r == MCFS_OK)
            n++;
        else if (r != MCFS_ERR_CANCELLED)
            last = r;
    }
    mark_show_done();
    if (n)
        transferDest = to;
    cards_recheck(to);
    card_back();
    if (r == MCFS_ERR_CANCELLED && !n)
        return 0;   /* back to the marked saves, as they were */
    if (total == 1 || !n)
        op_result(n ? MCFS_OK : last, T_DONE_COPY, to);
    else
        marks_report(n, total - n);
    return 1;
}

/* Circle on the screen of saves where a copy's marking began (c = whose saves it shows). A card's saves: the screen
 * closes and the cards are listed, to mark saves on others too; START there goes on to where they are copied to.
 * 1 = that was done: the screen is left, and the copy is over. 0 = the screen stays: nothing was marked any more,
 * and the marking is over; or they are a card file's saves, which has no other cards to go to: the marks are let go
 * of if the user says so, and the marking goes on if not */
int copy_leave(const card_t *c)
{
    if (marks.n && c != &fileCard) {
        browser_close();
        while (mark_cards(c) == 1 && !copy_marked(marks.into))
            ;   /* (back from where they were to go: the cards again. 0 = let go of, 2 = written to a folder) */
        marks_reset();
        return 1;
    }
    if (!marks.n || confirm(T(T_MARK_DROP), NULL, T_MARK_DROP_YES))
        marks_reset();
    return 0;
}
