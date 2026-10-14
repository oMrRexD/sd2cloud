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

/* X on a save of a copy whose folder is marked on another card: a card can't have two saves of one folder, so the
 * new one would take the other's place. Asked (with the card and the date of each). 1 = it can be marked, nothing
 * was asked; 2 = it can, asked; 0 = it can't (asked) */
int mark_same_folder(const save_view_t *v)
{
    char t[300], a[40], b[40], name[100];
    int i;
    for (i = 0; i < marks.n && !(marks.save[i].card != v->card && !strcmp(marks.save[i].s.folder, v->s.folder)); i++)
        ;
    if (i == marks.n)
        return 1;
    save_name(v, name, sizeof(name));
    snprintf(t, sizeof(t), T(T_MARK_SAME_ASK), name, marks.save[i].card->base);
    dlg_new(COLOR_WARN, t);
    format_save_when(marks.save[i].s.when, a, sizeof(a));
    format_save_when(v->s.when, b, sizeof(b));
    snprintf(t, sizeof(t), T(T_MARK_SAME_TEXT), marks.save[i].card->base, a, v->card->base, b);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_REPLACE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    return 2;
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
    char newName[56]; /* the card "New card" would make, for the game of the saves of a copy (g.newGame) */
} mk = {.iconCard = -1};

static void scene_mark(float t)
{
    char s[64];
    const card_t *c;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    view_draw();
    tabs_draw(&mk.g, 0);
    {
        /* only what the row does: "New card" has nothing to open (X pastes there too), and START pastes only where
         * the saves can go ("All saves" isn't a card, and the card they are all on has them) */
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}};
        int n = 1, row = mk.g.n ? mk.g.idx[mk.g.cursor] : ROW_ALL;
        if (mk.g.n && row != ROW_NEW)
            l[n].button = BUTTON_CROSS, l[n++].text = T(T_OPEN);
        if (!marks.copy)
            l[n].button = BUTTON_START, l[n++].text = T(T_FINISH);
        else if (mk.g.tab == TAB_FILES ? tabs_device(&mk.g) >= 0 || paste_here(&slotCard)
                 : row == ROW_NEW || (row >= 0 && (tabs_on_folders(&mk.g) || paste_here(&cards[row]))))
            l[n].button = BUTTON_START, l[n++].text = T(T_PASTE);
        look_legend(l, n, 0);
    }
    look_title(CARD_CX, 82, mk.title, 1);
    snprintf(s, sizeof(s), T(T_MARKED_N), marks.n);
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, marks.n ? COLOR_OK : COLOR_DIM, s);
    if (mk.g.tab == TAB_FILES) {   /* a device: the marked saves can go to a folder of it; the Memory Card, into it */
        if (tabs_device(&mk.g) < 0)
            look_card(CARD_X, CARD_Y, NULL, T(T_MEMCARD));
        return;
    }
    if (mk.g.n && mk.g.idx[mk.g.cursor] == ROW_NEW) {   /* "New card": the card to be, under the count */
        look_card(CARD_X, CARD_Y, "+", NULL);
        if (mk.g.tab == TAB_GAMES)   /* (their game's: its name; any other card has its place and size picked) */
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 26, COLOR_OK, mk.newName);
        return;
    }
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

/* The game of the saves marked for a copy, when they are all of one (its folder; "" = they aren't): the Games group
 * offers "New card" for it, and its folder comes first there. 1 = it changed (with ui_lock held) */
static int mark_game(void)
{
    char id[12], first[12] = "", folder[48] = "", f[48];
    int i;
    for (i = 0; marks.copy && i < marks.n; i++)
        if (!save_game_id(marks.save[i].s.folder, id) || (i && strcmp(id, first) != 0)) {
            first[0] = 0;
            break;
        } else
            snprintf(first, sizeof(first), "%s", id);
    if (first[0])
        game_folder(first, folder, sizeof(folder));
    if (!strcmp(folder, mk.g.newGame))
        return 0;
    snprintf(mk.g.newGame, sizeof(mk.g.newGame), "%s", folder);
    mk.newName[0] = 0;
    if (folder[0] && !new_card(TAB_GAMES, folder, 255, f, mk.newName))
        mk.newName[0] = 0;
    return 1;
}

/* START (or X) on "New card". In the Games group: the copy goes into a new card of their game, made once it is agreed
 * to (1 = picked). In the Cards group: a card made for them, wherever its place is picked, with them in it (2 = made,
 * and the copy is over) */
static int mark_new_card(void)
{
    sound_play(SND_CONFIRM);
    if (mk.g.tab == TAB_CARDS) {
        static save_ref_t refs[MARKS_MAX];
        int i;
        for (i = 0; i < marks.n; i++) {
            refs[i].card = marks.save[i].card;
            refs[i].folder = marks.save[i].s.folder;
        }
        return card_create(refs, marks.n) ? 2 : 0;
    }
    if (!dest_new_place(mk.g.newGame))
        return 0;
    marks.into = &destNew;
    return 1;
}

/* START on a folder of games: the card in it, when it has one; a folder of more than one opens, for the one to be
 * picked. 1 = that card takes them */
static int mark_folder(void)
{
    int k = mk.g.fold[mk.g.cursor];
    card_t *c = &cards[mk.g.games[mk.g.first[k]]];
    if (mk.g.first[k + 1] - mk.g.first[k] > 1) {
        sound_play(SND_CONFIRM);
        ui_lock();
        tabs_open(&mk.g);
        ui_unlock();
        return 0;
    }
    sound_play(paste_here(c) ? SND_CONFIRM : SND_BACK);
    if (!paste_here(c))
        return 0;
    marks.into = c;
    return 1;
}

/* The list, until START with something marked (1) or circle with the marks let go of (0). at = the card it opens on
 * (&allGames = "All saves"; NULL = the first one). For a copy the list has the Files group too: X or START on a
 * device opens its folders, where START writes the marked saves as .psu files (2 = that was done). again = back from
 * pasting them, given up (circle on its question): the list as it was left, the group, the folder and the cursor */
static int mark_cards(const card_t *at, int again)
{
    int r = -1;
    ui_lock();
    marks.done = 0;
    marks.into = NULL;
    if (again) {
        if (mark_game() || cardsUnsorted)   /* (a card made, and the rows with it) */
            tabs_redo(&mk.g);
        goto shown;
    }
    tabs_init(&mk.g, NULL, marks.copy);
    mk.g.all = 1;
    mk.g.create = marks.copy;   /* (a copy's saves can go into a card made for them) */
    mark_game();
    tabs_show(&mk.g, mk.g.tab);
    if (at == &allGames)
        tabs_show(&mk.g, TAB_GAMES);
    else if (at == &slotCard && mk.g.memcard) {   /* the Memory Card: the Files group's last row */
        tabs_show(&mk.g, TAB_FILES);
        mk.g.cursor = mk.g.n - 1;
    } else if (at && at >= cards && at < cards + nCards) {   /* (a game's card is inside its folder) */
        tabs_show(&mk.g, tab_of(at));
        tabs_find(&mk.g, (int)(at - cards));
        if (tabs_on_folders(&mk.g)) {
            tabs_open(&mk.g);
            mk.g.moved = -1;   /* (it opens on it: nothing slides) */
            tabs_find(&mk.g, (int)(at - cards));
        }
    }
shown:
    snprintf(mk.title, sizeof(mk.title), "%s", T(T_TPL_MARK_TITLE));
    mk.icon = NULL;
    mk.iconCard = -1;
    ui_unlock();
    while (r < 0) {
        u32 keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE | PAD_START | (device_other() ? PAD_R2 : 0), b;
        ui_scene(scene_mark);
        if (!(b = wait_nav_ms(keys, 250))) {
            mark_icon();
            b = wait_nav(keys);
        }
        if (tabs_nav(&mk.g, b) || tabs_folder_nav(&mk.g, b))
            continue;
        if (b & PAD_R2) {   /* the other device's cards: saves marked on them too, or pasted into them */
            int tab = mk.g.tab;
            sound_play(SND_MOVE);
            view_switch();
            ui_lock();
            tabs_init(&mk.g, NULL, marks.copy);
            mk.g.all = 1;
            mk.g.create = marks.copy;
            mark_game();
            tabs_show(&mk.g, mk.g.tab);
            if (mk.g.count[tab])   /* (the group it was on, when that device has it) */
                tabs_show(&mk.g, tab);
            icon_free(mk.icon);
            mk.icon = NULL;
            mk.iconCard = -1;
            ui_unlock();
            continue;
        }
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            if (!marks.n || confirm(T(T_MARK_DROP), NULL, T_MARK_DROP_YES))
                r = 0;
        } else if (mk.g.tab == TAB_FILES && tabs_device(&mk.g) < 0 && (b & PAD_START)) {   /* into the Memory Card */
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n) {
                marks.into = &slotCard;
                r = 1;
            }
        } else if (mk.g.tab == TAB_FILES && tabs_device(&mk.g) < 0) {   /* its saves, to mark some */
            sound_play(SND_CONFIRM);
            memcard_screen();
            if (marks.done)
                r = 1;
            ui_lock();
            tabs_show(&mk.g, TAB_FILES);
            ui_unlock();
        } else if (mk.g.tab == TAB_FILES) {
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n && copy_to_device(mk.g.cursor))
                r = 2;
            else if (marks.done)   /* (START on the saves of a card file opened there: they go into it) */
                r = 1;
        } else if ((b & PAD_START) && !marks.copy) {   /* (a template's: done) */
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n)
                r = 1;
        } else if (b & PAD_START) {   /* a copy's: pasted into the card START is on */
            if (!marks.n || !mk.g.n || mk.g.idx[mk.g.cursor] == ROW_ALL)   /* ("All saves" isn't a card) */
                sound_play(SND_BACK);
            else if (mk.g.idx[mk.g.cursor] == ROW_NEW) {
                int made = mark_new_card();
                r = made ? made : -1;
            }
            else if (tabs_on_folders(&mk.g))
                r = mark_folder() ? 1 : -1;
            else if (paste_here(tabs_card(&mk.g))) {
                sound_play(SND_CONFIRM);
                marks.into = tabs_card(&mk.g);
                r = 1;
            } else   /* (the card they are all on) */
                sound_play(SND_BACK);
        } else if ((b & PAD_CROSS) && mk.g.n && mk.g.idx[mk.g.cursor] == ROW_NEW) {   /* (nothing to open: pasted) */
            int made = mark_new_card();
            r = made ? made : -1;
        } else if ((b & PAD_CROSS) && mk.g.n) {
            card_t *c = mk.g.idx[mk.g.cursor] == ROW_ALL ? &allGames : tabs_card(&mk.g);
            if (c) {
                sound_play(SND_CONFIRM);
                card_screen(c);
                if (marks.done)
                    r = 1;
                ui_lock();   /* (saves marked or unmarked there: their game may be another one now) */
                if (mark_game())
                    tabs_redo(&mk.g);
                ui_unlock();
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
    r = mark_cards(NULL, 0);
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

/* Can the marked saves be pasted into that card: one of the cards, the Memory Card or a card file that can be
 * written, which lacks at least one of them? (Every game card's saves together aren't a card, nor is a .zip's) */
int paste_here(const card_t *c)
{
    if (!c || !marks.n)
        return 0;
    if (!(c >= cards && c < cards + nCards) && c != &slotCard && !(c == &fileCard && card_file_writable()))
        return 0;
    return marks_of(c) < marks.n;
}

/* START with saves marked for a copy ("Paste"). into = the card it was pressed on (paste_here): the one whose saves
 * were open, or the one under the cursor in the list of cards; or &destNew, a new card of their game, made once it
 * is agreed to. It is asked about, and they are copied one by one, each read back and compared. A save the card has
 * under the same name stays out, as one that is on that very card does. 1 = it is over (they were copied, or
 * couldn't be, and that was said): nothing is marked any more. 0 = the user came back from it: the marks stay, and
 * so does the marking */
int copy_marked(card_t *into)
{
    card_t *from = marks.save[0].card, *to = into;
    char t[300];
    long long size[MARKS_MAX], need = 0, room = 0;
    int i, n = 0, k = 0, total = 0, one = 0, last = MCFS_OK, r = MCFS_OK, made = into == &destNew;
    if (!made && !paste_here(into))
        return 0;   /* (the screens offer START only where it pastes) */
    if (marks.n > 1)   /* (each save's size is read from its card) */
        message(0, NULL, COLOR_TEXT, T(T_LOADING));
    for (i = 0; i < marks.n; i++) {
        int files = 0;
        size[i] = 0;
        if (marks.save[i].card != from)
            from = NULL;   /* of more than one card */
        mcfs_save_info(marks.save[i].card->path, marks.save[i].s.folder, &size[i], &files);
        if (marks.save[i].card != to) {
            need += size[i];
            room += mcfs_save_room(size[i], files);
        }
    }
    if (made && !dest_new_size(room)) {   /* (circle on the sizes: back to the marked saves) */
        card_back();
        mark_show_done();
        return 0;
    }
    if (!made) {   /* a card without room for them is left as it is */
        long long room = -1;
        if (mcfs_list_saves(to->path, NULL, 0, &room) >= 0 && room >= 0 && room < need) {
            snprintf(t, sizeof(t), T(T_ERR_FULL), to->base);
            message_wait(0, NULL, COLOR_WARN, t);
            return 0;
        }
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
    if (!confirm(t, to == &destNew ? dest_new_note() : NULL, T_COPY) || !(to = dest_real(to)) || card_free(to, from)) {
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
    if (made && n)   /* (a new game card: its templates too, as the device's new cards get them) */
        templates_new_card(to);
    if (r == MCFS_ERR_CANCELLED && !n)
        return 0;   /* back to the marked saves, as they were */
    if (total == 1 || !n)
        op_result(n ? MCFS_OK : last, T_DONE_COPY, to);
    else
        marks_report(n, total - n);
    return 1;
}

void marks_move(const card_t *from, card_t *to)
{
    int i;
    for (i = 0; i < marks.n; i++)
        if (marks.save[i].card == from)
            marks.save[i].card = to;
}

int marks_of(const card_t *c)
{
    int i, n = 0;
    for (i = 0; i < marks.n; i++)
        n += marks.save[i].card == c;
    return n;
}

/* the list of cards, with the Files group and the Memory Card, for the copy to go on (at = the card it opens on): until
 * the saves were pasted somewhere, or let go of */
void copy_go_on(const card_t *at)
{
    int again = 0;
    while (mark_cards(at, again) == 1 && !copy_marked(marks.into))
        again = 1;   /* (back from where they were to go: the cards again, as they were. 0 = let go of, 2 = written to a
                        folder) */
    marks_reset();
}

/* Circle on the screen of saves where a copy's marking began (c = whose saves it shows). A card's saves: the screen
 * closes and the cards are listed, to mark saves on others too; START there pastes them into the card it is on.
 * A card file's: the screen closes, and its folder is shown with them still marked (card_file_held): START there
 * pastes them, as .psu files. 1 = the screen is left. 0 = the screen stays: nothing was marked any more, and the
 * marking is over */
int copy_leave(const card_t *c)
{
    if (marks.n && c == &fileCard)   /* back to the card file's folder, with them (card_file_held) */
        return 1;
    if (marks.n && c != &fileCard) {
        browser_close();
        copy_go_on(c);
        return 1;
    }
    if (!marks.n || confirm(T(T_MARK_DROP), NULL, T_MARK_DROP_YES))
        marks_reset();
    return 0;
}
