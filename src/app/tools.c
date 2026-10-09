/* SD2Cloud -- the tools (SELECT on the main screen): what the program has besides the cards and their backups. For
 * now, the templates: sets of saves kept on the microSD, to be put into cards (the device makes a new game's card
 * empty; a template gives it the saves every card should have, as the network settings). The list of them; one of
 * them open, shown as a card is (its saves in the browser's grid); making one, by marking saves on the cards; and
 * putting one into a card. */
#include "app.h"

/* what was done, said in a box until a button is pressed: a title and, under it, a line and a warning (NULL = none) */
static void report(u32 color, const char *title, const char *line, const char *warning)
{
    dlg_new(color, title);
    if (line)
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, line);
    if (warning)
        dlg_line(FONT_TEXT, COLOR_WARN, 0, warning);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* ------------------------------------------------------------ a name, typed */

/* 1 = name is a name the template can have (self = the template being renamed, NULL = a new one); 0 = circle */
static int ask_name(char name[TPL_NAME + 1], const template_t *self)
{
    for (;;) {
        int r;
        if (!keyboard(T(T_TPL_NAME), name, TPL_NAME + 1))
            return 0;
        if ((r = template_name_check(name, self)) == TPL_OK)
            return 1;
        message_wait(0, NULL, COLOR_WARN, T(r == TPL_ERR_TAKEN ? T_TPL_TAKEN : T_TPL_BAD_NAME));
    }
}

/* ------------------------------------------------------------ marking saves
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
        legend_t l[3] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_START, T(T_FINISH)}};
        look_legend(l, 3, 0);
    }
    look_title(CARD_CX, 82, mk.title, 1);
    snprintf(s, sizeof(s), T(T_MARKED_N), marks.n);
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, marks.n ? COLOR_OK : COLOR_DIM, s);
    if (mk.g.n && mk.g.idx[mk.g.cursor] < 0) {   /* "All saves": the game cards together */
        look_card(CARD_X, CARD_Y, NULL, T(T_TAB_GAMES));
        return;
    }
    if ((c = tabs_card(&mk.g)) != NULL)
        draw_card_picture(c, mk.iconCard == mk.g.idx[mk.g.cursor] ? mk.icon : NULL, ui_clock());
}

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

/* 1 = saves were marked (marks) and START ended it; 0 = circle, and nothing is marked */
static int mark_saves(void)
{
    int r = -1;
    ui_lock();
    memset(&marks, 0, sizeof(marks));
    marks.on = 1;
    tabs_init(&mk.g, NULL, 0);
    mk.g.all = 1;
    tabs_show(&mk.g, mk.g.tab);
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
        } else if (b & PAD_START) {
            sound_play(marks.n ? SND_CONFIRM : SND_BACK);
            if (marks.n)
                r = 1;
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
    marks.on = 0;
    if (!r)
        marks.n = 0;
    icon_free(mk.icon);
    mk.icon = NULL;
    mk.iconCard = -1;
    ui_unlock();
    return r;
}

/* The marked saves into a template, each as a .psu written and read back (one the template has of the same folder
 * is replaced: replace = 0 leaves those out). Returns how many went in; failed = how many couldn't be copied */
static int marks_into(template_t *t, int replace, int *failed)
{
    int i, n = 0;
    *failed = 0;
    card_work(marks.save[0].card, 0, T(T_TPL_COPYING), 1);
    for (i = 0; i < marks.n; i++) {
        if (!replace && template_find_save(t, marks.save[i].folder) >= 0)
            continue;
        ui_lock();
        current = marks.save[i].card;
        currentN = i + 1;
        totalN = marks.n;
        ui_unlock();
        upload_screen(i, marks.n);
        if (template_add(t, marks.save[i].card->path, marks.save[i].folder) == MCFS_OK)
            n++;
        else
            (*failed)++;
    }
    upload_screen(marks.n, marks.n);
    card_work_done();
    return n;
}

/* ------------------------------------------------------------ one template, open
 *
 * Shown as a card is: its saves in the browser's grid (saves.c), under its name. X on a save: its page, from where
 * it is taken out of the template. Triangle: what can be done with the template. */

static card_t tplCard;      /* stands for the template on the screen of saves */
static template_t *tplOpen;

/* (brwIcon) a save's icon, from its .psu */
static int tpl_icon(const save_view_t *v, buffer_t *iconsys, buffer_t *ico)
{
    char path[400];
    mcfs_psu_t info;
    int i = tplOpen ? template_find_save(tplOpen, v->s.folder) : -1;
    if (i < 0)
        return -1;
    template_path(tplOpen, i, path, sizeof(path));
    return mcfs_psu_info(path, &info, iconsys, ico) == MCFS_OK && ico->len ? 0 : -1;
}

/* the template on the screen of saves, as it is now */
static void tpl_show(int cursor)
{
    static mcfs_save_t list[TPL_SAVES];
    int i;
    memset(list, 0, sizeof(list));
    for (i = 0; i < tplOpen->n; i++) {
        snprintf(list[i].folder, sizeof(list[i].folder), "%s", tplOpen->saves[i].folder);
        list[i].when = tplOpen->saves[i].when;
    }
    ui_scene(scene_frame);   /* nothing of the list on screen while it changes */
    ui_lock();
    memset(&tplCard, 0, sizeof(tplCard));
    snprintf(tplCard.base, sizeof(tplCard.base), "%s", tplOpen->name);
    snprintf(tplCard.id, sizeof(tplCard.id), "template %s", tplOpen->name);
    ui_unlock();
    brwIcon = tpl_icon;
    browser_fill(&tplCard, list, tplOpen->n, cursor);
    ui_lock();
    if (tplOpen == template_main())   /* the main one says so, before what it has */
        snprintf(brw.note, sizeof(brw.note), "%s - ", T(T_TPL_MAIN));
    else
        brw.note[0] = 0;
    snprintf(brw.note + strlen(brw.note), sizeof(brw.note) - strlen(brw.note), T(T_TPL_SUMMARY), tplOpen->n,
             (int)((tplOpen->bytes + 1023) / 1024));
    brw.emptyText = T_TPL_EMPTY;
    brw.legend[0] = (legend_t){BUTTON_CIRCLE, T(T_BACK)};
    brw.legend[1] = (legend_t){BUTTON_CROSS, T(T_OPEN)};
    brw.legend[2] = (legend_t){BUTTON_TRIANGLE, T(T_OPTIONS)};
    brw.nLegend = 3;
    ui_unlock();
    browser_scene();
}

/* X on a save of the template: its page, as a card's save has one. 1 = it was taken out */
static int tpl_save_page(int i)
{
    static const int options[] = {T_TPL_REMOVE};
    char name[100], t[300];
    save_view_t *v = &brw.saves[i];
    int k = template_find_save(tplOpen, v->s.folder);
    if (k < 0)
        return 0;
    save_page(v, tplOpen->name, tplOpen->saves[k].bytes, options, 1);
    while (save_page_choice()) {
        save_name(v, name, sizeof(name));
        snprintf(t, sizeof(t), T(T_TPL_REMOVE_ASK), name);
        if (!confirm(t, NULL, T_TPL_REMOVE_YES))
            continue;
        if (template_remove(tplOpen, k) == 0)
            return 1;
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_MC_WRITE));
    }
    return 0;
}

/* -------- putting it into a card: the saves the card lacks, never over one it has */

static struct {
    card_t *to;
    save_view_t view;      /* the save on its way, for the screen: its icon and its name */
    int card, cards;       /* which card this is of how many, when it goes into more than one (cards 0 = into one) */
} ap;

/* (template_apply) the next save: the screen shows it */
static int apply_before(const template_t *t, int i)
{
    buffer_t iconsys = {0}, ico = {0};
    char path[400];
    mcfs_psu_t info;
    icon_t *ic = NULL, *old = ap.view.icon;
    template_path(t, i, path, sizeof(path));
    if (mcfs_psu_info(path, &info, &iconsys, &ico) == MCFS_OK && ico.len)
        ic = icon_load(&iconsys, &ico);
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    memset(&ap.view, 0, sizeof(ap.view));
    snprintf(ap.view.s.folder, sizeof(ap.view.s.folder), "%s", t->saves[i].folder);
    ap.view.icon = ic;
    ap.view.tried = 1;
    save_title(&ap.view, ic);
    ui_unlock();
    save_into(ap.to, &ap.view, T_TPL_APPLYING);   /* (from here on nothing shows the icon of the save before) */
    ui_lock();
    icon_free(old);
    if (ap.cards) {
        currentN = ap.card;
        totalN = ap.cards;
    }
    ui_unlock();
    return 0;
}

static void apply_done(void)
{
    save_into_done();
    ui_lock();
    icon_free(ap.view.icon);
    ap.view.icon = NULL;
    ui_unlock();
}

/* a card's root folder, as it was when the card was last read (read now, for a card that wasn't) */
static const char *root_of(card_t *c)
{
    if (!c->rootSig[0])
        mcfs_root_signature(c->path, c->rootSig);
    return c->rootSig;
}

/* Does another card have the same root folder as that one? (Two cards with nothing in them do.) The card in the
 * device is told from the others by its root folder: with a twin, which of the two the device is on isn't known.
 * (A numbered card the sd2psx names by its number) */
static int has_twin(const card_t *c)
{
    card_t *own = &cards[c - cards];
    int i;
    if (cardTold && c->type == TYPE_NORMAL)
        return 0;
    for (i = 0; i < nCards; i++)
        if (&cards[i] != c && (!cardTold || cards[i].type != TYPE_NORMAL) && !strcmp(root_of(&cards[i]), root_of(own)))
            return 1;
    return 0;
}

/* is the device using that card right now, for sure? Its file can't be touched then: it is changed through the slot.
 * (The card in use is the one find_active found, when no other card could be taken for it) */
static int in_slot(const card_t *c) { return activeCard >= 0 && &cards[activeCard] == c && !has_twin(c); }

#define TPL_LEFT (-100)   /* a card left as it was: the device may be using it, and wasn't moved off it */

/* how many saves of a template a card lacks: in its file or, for the card the device is using, in the slot (-1 = it
 * can't be told) */
static int card_lacks(const card_t *c, const template_t *t)
{
    unsigned char lacks[TPL_SAVES];
    int i, n = 0, has;
    if (!in_slot(c))
        return template_lacking(t, c->path, lacks, NULL, NULL);
    for (i = 0; i < t->n; i++) {
        if ((has = mc_has_folder(mc_slot(), t->saves[i].folder)) < 0)
            return -1;
        n += !has;
    }
    return n;
}

/* A template into a card: the saves the card lacks and, with replace, the template's in place of the ones the card
 * has that differ. In the card's file; or, for the card the device is using, through the slot, as a game would
 * write them. show = on the screen of a save on its way. MCFS_* (or TPL_LEFT); put = how many saves were written */
static int template_into(const template_t *t, card_t *c, int replace, int show, int *put)
{
    char path[400];
    int i, has, r = MCFS_OK, port = mc_slot();
    *put = 0;
    ap.to = c;
    if (!in_slot(c)) {
        /* (the device is asked, and what the PS2 sees in the slot compared with this card: it is moved off this one
         * if it has to be) */
        if (card_free(c, NULL))
            return TPL_LEFT;
        r = (replace ? template_update : template_apply)(t, c->path, show ? apply_before : NULL, show ? save_progress : NULL, put);
        cards_recheck(c);
        card_back();
        return r;
    }
    for (i = 0; i < t->n && r == MCFS_OK; i++) {
        if ((has = mc_has_folder(port, t->saves[i].folder)) < 0)
            r = MCFS_ERR_IO;
        else if (!has || replace) {
            if (show) {
                apply_before(t, i);
                save_into_fixed();   /* (nothing tells how far it is, and it can't be given up) */
            }
            template_path(t, i, path, sizeof(path));
            if (has && mc_delete_save(port, t->saves[i].folder) != 0)
                r = MCFS_ERR_IO;
            else if ((r = mc_put_save(port, path)) == MCFS_OK)
                (*put)++;
        }
    }
    log_msg("template %s into %s, through the slot: %d save(s), %d", t->name, c->id, *put, r);
    return r;
}

static void template_to_card(template_t *t)
{
    char s[300], note[200];
    card_t *to;
    int n, put = 0, r;
    if (!t->n) {
        message_wait(0, NULL, COLOR_WARN, T(T_TPL_EMPTY));
        return;
    }
    if (!nCards) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
        return;
    }
    destFiles = 0;
    if (!(to = choose_dest(NULL, T_TPL_APPLY_TO, 0, NULL)))
        return;
    find_active();
    if ((n = card_lacks(to, t)) < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    if (!n) {
        snprintf(s, sizeof(s), T(T_TPL_NOTHING), to->base);
        message_wait(0, NULL, COLOR_OK, s);
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_APPLY_ASK), t->name, to->base);
    snprintf(note, sizeof(note), T(T_TPL_APPLY_NOTE), n);
    if (!confirm(s, note, T_TPL_APPLY))
        return;
    ap.cards = 0;
    cancelLatched = 0;
    r = template_into(t, to, 0, 1, &put);
    apply_done();
    if (r == MCFS_ERR_CANCELLED || r == TPL_LEFT)
        return;   /* given up, or left alone: said already */
    if (r == MCFS_OK || r == MCFS_ERR_FULL)
        snprintf(s, sizeof(s), T(r == MCFS_OK ? T_TPL_APPLIED : T_TPL_APPLIED_FULL), to->base, put);
    else {
        op_result(r, 0, to);
        return;
    }
    message_wait(0, NULL, r == MCFS_OK ? COLOR_OK : COLOR_WARN, s);
}

/* -------- the cards a template is for: the main template is every game card's, and any template can be a game's */

static int gameCards[MAX_CARDS];   /* the cards a template is about to be put into (indexes in cards[]) */

/* is that game card one the template is for? */
static int card_of(const template_t *t, const card_t *c)
{
    return c->type == TYPE_GAMEID && (t == template_main() || template_has_game(t, c->folder));
}

/* how many saves a game card lacks of its templates, all together (-1 = it has no template, or it can't be told) */
static int card_lacks_its(const card_t *c)
{
    const template_t *list[TPL_MAX];
    int k, n = 0, m, count = templates_of_game(c->folder, list);
    for (k = 0; k < count; k++) {
        if ((m = card_lacks(c, list[k])) < 0)
            return -1;
        n += m;
    }
    return count ? n : -1;
}

/* Into gameCards: the game cards that lack saves of that template (its = only the cards it is for); or, with no
 * template, the ones that lack saves of their own templates and aren't settled (and one that lacks nothing is
 * settled, from now on). Returns how many */
static int cards_lacking(const template_t *t, int its)
{
    int i, n = 0, k, changed = 0;
    for (i = 0; i < nCards; i++) {
        card_t *c = &cards[i];
        if (c->type != TYPE_GAMEID)
            continue;
        if (t) {
            if ((!its || card_of(t, c)) && card_lacks(c, t) > 0)
                gameCards[n++] = i;
        } else if (!templates_settled(c->id, c->folder)) {
            if ((k = card_lacks_its(c)) > 0)
                gameCards[n++] = i;
            else if (k == 0) {
                templates_settle(c->id, c->folder);
                changed = 1;
            }
        }
    }
    if (changed)
        templates_save();
    return n;
}

/* The n cards of gameCards get that template, or (t NULL) each one its own templates, one card after the other on
 * the screen of a save on its way; replace = the template's saves also take the place of the card's that differ.
 * Circle gives up what is left. What happened is said at the end */
static void cards_get(const template_t *t, int n, int replace)
{
    const template_t *list[TPL_MAX];
    char got[100], left[100];
    int i, k, count, put, r, changed = 0, failed = 0;
    cancelLatched = 0;
    for (i = 0; i < n; i++) {
        card_t *c = &cards[gameCards[i]];
        int total = 0;
        list[0] = t;
        count = t ? 1 : templates_of_game(c->folder, list);
        ap.card = i + 1;
        ap.cards = n;
        for (k = 0, r = MCFS_OK; k < count && r == MCFS_OK; k++) {
            r = template_into(list[k], c, replace, 1, &put);
            total += put;
        }
        changed += total > 0;
        if (r == MCFS_ERR_CANCELLED)
            break;
        failed += r != MCFS_OK;
        if (r == MCFS_OK && card_lacks_its(c) == 0)
            templates_settle(c->id, c->folder);
    }
    apply_done();
    ap.cards = 0;
    templates_save();
    snprintf(got, sizeof(got), T(T_TPL_GAMES_GOT), changed);
    snprintf(left, sizeof(left), T(T_TPL_GAMES_LEFT), failed);
    report(failed ? COLOR_WARN : COLOR_OK, got, NULL, failed ? left : NULL);
}

/* "Apply to the game cards": every game card that lacks saves of the template gets them (its = only the cards the
 * template is for), once the user agrees. quiet = nothing is said when there is nothing to do */
static void template_to_game_cards(template_t *t, int quiet, int its)
{
    char s[300], f[100];
    int i, n, games = 0;
    for (i = 0; i < nCards; i++)
        games += cards[i].type == TYPE_GAMEID;
    if (!t->n || !games) {
        if (!quiet)
            message_wait(0, NULL, COLOR_WARN, T(t->n ? T_TPL_NO_GAME_CARDS : T_TPL_EMPTY));
        return;
    }
    message(0, NULL, COLOR_TEXT, T(T_TPL_CHECKING));
    find_active();
    if (!(n = cards_lacking(t, its))) {
        if (!quiet)
            message_wait(0, NULL, COLOR_OK, T(T_TPL_GAMES_NONE));
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_GAMES_ASK), t->name);
    snprintf(f, sizeof(f), T(T_TPL_GAMES_N), n);
    if (confirm(s, f, T_TPL_APPLY))
        cards_get(t, n, 0);
}

/* "Update on the cards": on the cards the template is for, its saves take the place of the ones that differ. The
 * only thing here that writes over a save, and it says so before */
static void template_update_cards(template_t *t)
{
    char s[300];
    int i, n = 0;
    for (i = 0; i < nCards; i++)
        if (card_of(t, &cards[i]))
            gameCards[n++] = i;
    if (!t->n || !n) {
        message_wait(0, NULL, COLOR_WARN, T(t->n ? T_TPL_NO_GAME_CARDS : T_TPL_EMPTY));
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_UPDATE_ASK), t->name);
    if (!confirm(s, T(T_TPL_UPDATE_NOTE), T_TPL_UPDATE_YES))
        return;
    find_active();
    cards_get(t, n, 1);
}

/* When the program opens: the game cards that lack saves of their templates and aren't settled (the card the device
 * made for a new game is one). Said once, with the three things to do about it: put them there now, later, or never
 * for those cards */
void templates_startup(void)
{
    char s[300], names[200] = "";
    int i, n;
    u32 b;
    if (!templates_in_use())
        return;
    templates_scan();
    if (!(n = cards_lacking(NULL, 0)))
        return;
    for (i = 0; i < n && i < 3; i++)   /* which ones: the first few, by their games */
        snprintf(names + strlen(names), sizeof(names) - strlen(names), "%s%s", i ? ", " : "", game_of(&cards[gameCards[i]]));
    if (n > 3)
        snprintf(names + strlen(names), sizeof(names) - strlen(names), "...");
    log_msg("templates: %d game card(s) lack saves of their templates", n);
    snprintf(s, sizeof(s), T(T_TPL_WARN), n);
    dlg_new(COLOR_TITLE, s);
    dlg_line(FONT_SMALL, COLOR_DIM, 0, names);
    dlg_buttons(BUTTON_CIRCLE, T_LATER, BUTTON_CROSS, T_TPL_APPLY);
    dlg_button(BUTTON_SQUARE, T_TPL_NO_WARN);
    next.wide = 1;
    dlg_show();
    b = wait_button(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE, 0);
    if (b & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        cards_get(NULL, n, 0);
    } else if (b & PAD_SQUARE) {
        sound_play(SND_CONFIRM);
        for (i = 0; i < n; i++)
            templates_settle(cards[gameCards[i]].id, cards[gameCards[i]].folder);
        templates_save();
    } else
        sound_play(SND_BACK);
}

/* After a game, with nobody at the controller (the automatic sync): the game cards about to be sent get the saves
 * they lack of their templates, each in its own file. The game's card isn't the one in the device by then: OPL has it
 * back on the BootCard, or on the card from before the game. A card's file is only written when that is sure: the
 * card the device is on is known, the PS2 sees in the slot the root folder that card's file has, and this card's
 * file has another. Any doubt leaves the card as it is, for the warning when the program is next opened. Nothing
 * here may hold the way back to OPL: whatever goes wrong is only logged */
void templates_after_game(void)
{
    const template_t *list[TPL_MAX];
    char seen[65], file[65];
    int i, k, n = 0, count, put, total, r, changed = 0;
    if (cfg.no_tpl_after_game || !templates_in_use())
        return;
    templates_scan();
    for (i = 0; i < nCards; i++)
        if (cards[i].type == TYPE_GAMEID && is_selected(&cards[i], 0) && templates_of_game(cards[i].folder, list) &&
            !templates_settled(cards[i].id, cards[i].folder))
            gameCards[n++] = i;
    if (!n)
        return;
    find_active();
    if (!active_sure(seen)) {
        log_msg("templates: after the game, %d card(s) left as they are: which card the device is on isn't sure", n);
        return;
    }
    message(COLOR_TITLE, T(T_IGR_TITLE), COLOR_DIM, T(T_TPL_APPLYING));
    for (i = 0; i < n; i++) {
        card_t *c = &cards[gameCards[i]];
        if (mcfs_root_signature(c->path, file) != 0 || !strcmp(seen, file)) {
            log_msg("templates: after the game, %s left as it is: it may be the card in the device", c->id);
            continue;
        }
        count = templates_of_game(c->folder, list);
        for (k = 0, total = 0, r = MCFS_OK; k < count && r == MCFS_OK; k++) {
            r = template_apply(list[k], c->path, NULL, NULL, &put);
            total += put;
        }
        log_msg("templates: after the game, %s got %d save(s) (%d)", c->id, total, r);
        if (total)
            cards_recheck(c);   /* what is sent is the card as it is now */
        if (r == MCFS_OK) {
            templates_settle(c->id, c->folder);
            changed = 1;
        }
    }
    if (changed)
        templates_save();
}

/* a template is made the main one, or stops being it */
static void template_toggle_main(template_t *t)
{
    char s[300];
    int was = t == template_main();
    template_set_main(was ? NULL : t);
    if (templates_save() != 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_MC_WRITE));
        return;
    }
    log_msg("templates: %s is %s the main one", t->name, was ? "no longer" : "now");
    if (was)
        return;
    snprintf(s, sizeof(s), T(T_TPL_MAIN_NOW), t->name);
    message_wait(0, NULL, COLOR_OK, s);
    template_to_game_cards(t, 1, 0);
}

/* -------- the games a template is for: the folders of game cards, each one marked or not */

#define LIST_ROWS  5    /* (the rows of a list, and how far apart: tabs.c) */
#define LIST_ROW_H 30

static struct {
    const template_t *t;
    int n, cursor, top;
    int first[MAX_CARDS];   /* the first card of each game's folder (indexes in cards[]) */
} gl;

static const char *games_text(int i, char *buf)
{
    snprintf(buf, 64, "%s", game_of(&cards[gl.first[i]]));
    fit(FONT_TEXT, buf, 64, 236);   /* (up to where the big card's picture starts) */
    return buf;
}

static void scene_games(float t)
{
    char s[64];
    int i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    list_rows(gl.n, gl.cursor, gl.top, games_text, NULL);
    for (i = gl.top; i < gl.n && i < gl.top + LIST_ROWS; i++)   /* a green light before each game it is for */
        if (template_has_game(gl.t, cards[gl.first[i]].folder)) {
            float y = ROW_Y0 + (i - gl.top) * LIST_ROW_H + ui_line_height(FONT_TEXT) / 2.0f + 1;
            ui_image(IMG_GLOW, LIST_X + 1, y - 9, 18, 18, COLOR_OK, 0x80);
            ui_rect(LIST_X + 8, y - 2, 4, 4, COLOR_OK, 0x80);
        }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_MARK)}};
        look_legend(l, 2, 0);
    }
    look_title(CARD_CX, 82, T(T_TPL_GAMES), 1);
    draw_card_picture(&cards[gl.first[gl.cursor]], NULL, ui_clock());
    snprintf(s, sizeof(s), T(T_MARKED_N), template_games(gl.t));
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, template_games(gl.t) ? COLOR_OK : COLOR_DIM, s);
}

static void template_games_screen(template_t *t)
{
    int i, k, changed = 0;
    ui_lock();
    gl.t = t;
    gl.n = gl.cursor = gl.top = 0;
    for (i = 0; i < nCards; i++) {
        if (cards[i].type != TYPE_GAMEID)
            continue;
        for (k = 0; k < gl.n && strcasecmp(cards[gl.first[k]].folder, cards[i].folder); k++)
            ;
        if (k == gl.n)
            gl.first[gl.n++] = i;
    }
    ui_unlock();
    if (!gl.n) {
        message_wait(0, NULL, COLOR_WARN, T(T_TPL_NO_GAME_CARDS));
        return;
    }
    ui_scene(scene_games);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            break;
        }
        ui_lock();
        if (b & PAD_CROSS) {
            const char *game = cards[gl.first[gl.cursor]].folder;
            template_set_game(t, game, !template_has_game(t, game));
            changed = 1;
        } else {
            gl.cursor = (b & PAD_UP) ? (gl.cursor + gl.n - 1) % gl.n : (gl.cursor + 1) % gl.n;
            scroll_to(gl.cursor, &gl.top);
        }
        ui_unlock();
        sound_play((b & PAD_CROSS) ? SND_CONFIRM : SND_MOVE);
    }
    if (!changed)
        return;
    if (templates_save() != 0)
        message_wait(0, NULL, COLOR_ERROR, T(T_ERR_MC_WRITE));
    else if (template_games(t))   /* the cards of those games that lack it: offered now */
        template_to_game_cards(t, 1, 1);
}

/* -------- changing it: more saves, another name, or none of it */

static void template_add_saves(template_t *t)
{
    char s[300], f[100];
    int i, taken = 0, fresh, replace = 1, failed, n;
    if (!nCards) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
        return;
    }
    if (!mark_saves())
        return;
    for (i = 0; i < marks.n; i++)
        taken += template_find_save(t, marks.save[i].folder) >= 0;
    fresh = marks.n - taken;
    if (taken) {   /* saves the template has: the marked ones take their place, or stay out */
        u32 b;
        snprintf(s, sizeof(s), T(T_TPL_REPLACE_ASK), taken);
        dlg_new(COLOR_TITLE, s);
        dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_REPLACE);
        dlg_button(BUTTON_SQUARE, T_TPL_KEEP);
        dlg_show();
        b = wait_button(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE, 0);
        if (!(b & (PAD_CROSS | PAD_SQUARE)) || (b & PAD_CIRCLE)) {
            sound_play(SND_BACK);
            return;
        }
        sound_play(SND_CONFIRM);
        replace = (b & PAD_CROSS) != 0;
    }
    if (t->n + fresh > TPL_SAVES) {
        snprintf(s, sizeof(s), T(T_TPL_FULL), TPL_SAVES);
        message_wait(0, NULL, COLOR_WARN, s);
        return;
    }
    if (!fresh && !replace)
        return;
    if (!taken) {
        snprintf(s, sizeof(s), T(T_TPL_ADD_ASK), t->name);
        snprintf(f, sizeof(f), T(T_MARKED_N), marks.n);
        if (!confirm(s, f, T_TPL_ADD))
            return;
    }
    n = marks_into(t, replace, &failed);
    snprintf(s, sizeof(s), T(T_TPL_ADDED), n);
    snprintf(f, sizeof(f), T(T_TPL_NOT_COPIED), failed);
    report(failed ? COLOR_WARN : COLOR_OK, s, NULL, failed ? f : NULL);
}

/* what triangle offers on the open template. 0 = the template is gone, or isn't the one it was (renamed: tplOpen is
 * the new one) */
static int template_options(void)
{
    enum { OPT_CARD, OPT_GAME_CARDS, OPT_MAIN, OPT_GAMES, OPT_UPDATE, OPT_ADD, OPT_RENAME, OPT_DELETE, OPTS };
    const char *items[OPTS];
    char name[TPL_NAME + 1], s[300];
    template_t *t;
    int id[OPTS], n, k = 0;
    for (;;) {   /* circle in what comes next comes back here; circle here goes back to the template */
        int isMain = tplOpen == template_main();
        n = 0;
        items[n] = T(T_TPL_APPLY_CARD), id[n++] = OPT_CARD;
        items[n] = T(T_TPL_APPLY_GAMES), id[n++] = OPT_GAME_CARDS;
        items[n] = T(isMain ? T_TPL_UNMAKE_MAIN : T_TPL_MAKE_MAIN), id[n++] = OPT_MAIN;
        if (!isMain)   /* (the main one is every game's already) */
            items[n] = T(T_TPL_GAMES), id[n++] = OPT_GAMES;
        if (isMain || template_games(tplOpen))   /* (only on the cards it is for) */
            items[n] = T(T_TPL_UPDATE), id[n++] = OPT_UPDATE;
        items[n] = T(T_TPL_ADD_SAVES), id[n++] = OPT_ADD;
        items[n] = T(T_TPL_RENAME), id[n++] = OPT_RENAME;
        items[n] = T(T_TPL_DELETE), id[n++] = OPT_DELETE;
        if ((k = choose(tplOpen->name, items, n, k < n ? k : 0)) < 0)
            return 1;
        switch (id[k]) {
        case OPT_CARD:
            template_to_card(tplOpen);
            break;
        case OPT_GAME_CARDS:
            template_to_game_cards(tplOpen, 0, 0);
            break;
        case OPT_MAIN:
            template_toggle_main(tplOpen);
            break;
        case OPT_GAMES:
            template_games_screen(tplOpen);
            break;
        case OPT_UPDATE:
            template_update_cards(tplOpen);
            break;
        case OPT_ADD:
            template_add_saves(tplOpen);
            return 1;   /* back to the template, where what it has now shows */
        case OPT_RENAME:
            snprintf(name, sizeof(name), "%s", tplOpen->name);
            if (!ask_name(name, tplOpen) || !strcmp(name, tplOpen->name))
                break;
            message(0, NULL, COLOR_TEXT, T(T_TPL_RENAMING));
            if ((t = template_rename(tplOpen, name)) != NULL) {
                tplOpen = t;
                return 1;
            }
            tplOpen = template_find(tplCard.base);   /* (it may have moved in the list) */
            message_wait(0, NULL, COLOR_ERROR, T(T_TPL_RENAME_FAILED));
            if (!tplOpen)
                return 0;
            break;
        default:
            snprintf(s, sizeof(s), T(T_TPL_DELETE_ASK), tplOpen->name);
            if (!confirm(s, T(T_TPL_DELETE_NOTE), T_DELETE))
                break;
            if (template_delete(tplOpen) == 0) {
                tplOpen = NULL;
                return 0;
            }
            message_wait(0, NULL, COLOR_ERROR, T(T_TPL_DELETE_FAILED));
        }
    }
}

/* a template, open. name comes back as the name it has when the screen is left ("" = it was deleted) */
static void template_screen(char name[TPL_NAME + 1])
{
    if (!(tplOpen = template_find(name)))
        return;
    tpl_show(0);
    for (;;) {
        u32 b = browser_wait(PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            break;
        }
        if ((b & PAD_CROSS) && brw.n) {
            int cursor = brw.cursor;
            sound_play(SND_CONFIRM);
            if (tpl_save_page(cursor))
                tpl_show(cursor);
            else
                browser_scene();
        } else if (b & PAD_TRIANGLE) {
            sound_play(SND_CONFIRM);
            if (!template_options())
                break;
            tpl_show(brw.cursor);
        }
    }
    browser_close();
    brwIcon = NULL;
    snprintf(name, TPL_NAME + 1, "%s", tplOpen ? tplOpen->name : "");
    tplOpen = NULL;
}

/* ------------------------------------------------------------ a new one */

/* asks its name, has its saves marked and copies them into it. name = the template made ("" = none was) */
static void template_create(char name[TPL_NAME + 1])
{
    char s[300], f[100], w[100];
    template_t *t;
    int i, n, failed;
    name[0] = 0;
    if (nTemplates >= TPL_MAX) {
        snprintf(s, sizeof(s), T(T_TPL_MANY), TPL_MAX);
        message_wait(0, NULL, COLOR_WARN, s);
        return;
    }
    if (!nCards) {
        message_wait(0, NULL, COLOR_WARN, T(T_NO_CARDS_SD));
        return;
    }
    for (i = 1; i < 100; i++) {   /* "Template 1", or the first number no template has */
        snprintf(name, TPL_NAME + 1, T(T_TPL_DEFAULT), i);
        if (!template_find(name))
            break;
    }
    if (!ask_name(name, NULL) || !mark_saves()) {
        name[0] = 0;
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_CREATE_ASK), name);
    snprintf(f, sizeof(f), T(T_MARKED_N), marks.n);
    if (!confirm(s, f, T_TPL_CREATE)) {
        name[0] = 0;
        return;
    }
    if (!(t = template_new(name))) {
        message_wait(0, NULL, COLOR_ERROR, T(T_TPL_FAILED));
        name[0] = 0;
        return;
    }
    n = marks_into(t, 1, &failed);
    log_msg("template %s made: %d save(s), %d not copied", name, n, failed);
    if (!n) {   /* a template of nothing is no template */
        template_delete(t);
        name[0] = 0;
        message_wait(0, NULL, COLOR_ERROR, T(T_TPL_FAILED));
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_CREATED), name);
    snprintf(f, sizeof(f), T(T_TPL_COPIED_N), n);
    snprintf(w, sizeof(w), T(T_TPL_NOT_COPIED), failed);
    report(COLOR_OK, s, f, failed ? w : NULL);
}

/* ------------------------------------------------------------ the list of them */

static struct {
    int n, cursor, top;   /* the rows: "New template", the templates, and whether they are applied after a game */
} tl;

static const char *list_text(int i, char *buf)
{
    (void)buf;
    return !i ? T(T_TPL_NEW) : i == tl.n - 1 ? T(T_TPL_AFTER_GAME) : templates[i - 1].name;
}

static void scene_templates(float t)
{
    char s[64];
    const template_t *mainOne = template_main(), *shown;
    int row = mainOne ? (int)(mainOne - templates) + 1 - tl.top : -1, last = tl.n - 1 - tl.top;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    rowsWide = 236;   /* (a template's name is longer than a card's: up to where the big card's picture starts) */
    list_rows(tl.n, tl.cursor, tl.top, list_text, NULL);
    rowsWide = 0;
    if (tl.top == 0) {   /* "New template": a plus before it, as "New card" has */
        float y = ROW_Y0 + ui_line_height(FONT_TEXT) / 2.0f + 1;
        ui_rect(LIST_X + 4, y - 1, 12, 2, COLOR_ACCENT, 0x70);
        ui_rect(LIST_X + 9, y - 6, 2, 12, COLOR_ACCENT, 0x70);
    }
    if (row >= 0 && row < LIST_ROWS) {   /* the main one: a yellow light before it */
        float y = ROW_Y0 + row * LIST_ROW_H + ui_line_height(FONT_TEXT) / 2.0f + 1;
        ui_image(IMG_GLOW, LIST_X + 1, y - 9, 18, 18, COLOR_TITLE, 0x80);
        ui_rect(LIST_X + 8, y - 2, 4, 4, COLOR_TITLE, 0x80);
    }
    if (last >= 0 && last < LIST_ROWS) {   /* after a game: a light that is green when it is on */
        float y = ROW_Y0 + last * LIST_ROW_H + ui_line_height(FONT_TEXT) / 2.0f + 1;
        u32 color = cfg.no_tpl_after_game ? 0x5A6478 : COLOR_OK;
        ui_image(IMG_GLOW, LIST_X + 1, y - 9, 18, 18, color, cfg.no_tpl_after_game ? 0x40 : 0x80);
        ui_rect(LIST_X + 8, y - 2, 4, 4, color, 0x80);
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(tl.cursor == tl.n - 1 ? T_TPL_CHANGE : T_OPEN)}};
        look_legend(l, 2, 0);
    }
    look_title(CARD_CX, 82, T(T_TEMPLATES), 1);
    if (!tl.cursor) {
        look_card(CARD_X, CARD_Y, "+", NULL);
        return;
    }
    if (tl.cursor == tl.n - 1) {   /* what that is, where a template's picture would be */
        ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + 30, cfg.no_tpl_after_game ? COLOR_DIM : COLOR_OK,
                       T(cfg.no_tpl_after_game ? T_TPL_OFF : T_TPL_ON));
        ui_paragraph(FONT_SMALL, CARD_X - 16, CARD_Y + 70, LOOK_CARD_W + 32, COLOR_DIM, T(T_TPL_AFTER_HINT));
        return;
    }
    shown = &templates[tl.cursor - 1];
    look_card(CARD_X, CARD_Y, NULL, shown->name);
    snprintf(s, sizeof(s), T(T_TPL_SUMMARY), shown->n, (int)((shown->bytes + 1023) / 1024));
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, s);
    if (shown == mainOne)
        ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_TITLE, T(T_TPL_MAIN));
}

/* the list is the templates there are now, with the cursor on the one of that name ("" = on "New template") */
static void list_set(const char *name)
{
    template_t *t = name[0] ? template_find(name) : NULL;
    ui_lock();
    tl.n = nTemplates + 2;
    tl.cursor = t ? (int)(t - templates) + 1 : 0;
    scroll_to(tl.cursor, &tl.top);
    ui_unlock();
}

static void templates_screen(void)
{
    char name[TPL_NAME + 1] = "";
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    templates_scan();
    ui_lock();
    tl.top = 0;
    ui_unlock();
    list_set("");
    for (;;) {
        u32 b;
        ui_scene(scene_templates);
        b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return;
        }
        if (b & (PAD_UP | PAD_DOWN)) {
            ui_lock();
            tl.cursor = (b & PAD_UP) ? (tl.cursor + tl.n - 1) % tl.n : (tl.cursor + 1) % tl.n;
            scroll_to(tl.cursor, &tl.top);
            ui_unlock();
            sound_play(SND_MOVE);
            continue;
        }
        sound_play(SND_CONFIRM);
        if (tl.cursor == tl.n - 1) {   /* after a game: on, or off */
            ui_lock();
            cfg.no_tpl_after_game = !cfg.no_tpl_after_game;
            ui_unlock();
            config_set("templates", "after_game", cfg.no_tpl_after_game ? "no" : "yes");
            continue;
        }
        if (!tl.cursor) {
            template_create(name);
            if (!name[0]) {
                list_set("");
                continue;
            }
        } else
            snprintf(name, sizeof(name), "%s", templates[tl.cursor - 1].name);
        list_set(name);   /* (a new one is in the list from now on, whatever comes next) */
        template_screen(name);
        list_set(name);
    }
}

/* ------------------------------------------------------------ the tools */

void tools_screen(void)
{
    const char *items[1];
    for (;;) {
        items[0] = T(T_TEMPLATES);
        if (choose(T(T_TOOLS), items, 1, 0) < 0)
            return;
        templates_screen();
    }
}
