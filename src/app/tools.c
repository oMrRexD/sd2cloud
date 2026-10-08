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
    snprintf(brw.note, sizeof(brw.note), T(T_TPL_SUMMARY), tplOpen->n, (int)((tplOpen->bytes + 1023) / 1024));
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

static void template_to_card(template_t *t)
{
    unsigned char lacks[TPL_SAVES];
    char s[300], note[200];
    long long bytes = 0, freeBytes = -1;
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
    if ((n = template_lacking(t, to->path, lacks, &bytes, &freeBytes)) < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    if (!n) {
        snprintf(s, sizeof(s), T(T_TPL_NOTHING), to->base);
        message_wait(0, NULL, COLOR_OK, s);
        return;
    }
    if (freeBytes >= 0 && bytes > freeBytes) {
        snprintf(s, sizeof(s), T(T_TPL_NO_ROOM), to->base, (int)((bytes + 1023) / 1024), (int)(freeBytes / 1024));
        message_wait(0, NULL, COLOR_WARN, s);
        return;
    }
    snprintf(s, sizeof(s), T(T_TPL_APPLY_ASK), t->name, to->base);
    snprintf(note, sizeof(note), T(T_TPL_APPLY_NOTE), n);
    if (!confirm(s, note, T_TPL_APPLY) || card_free(to, NULL))
        return;
    ap.to = to;
    cancelLatched = 0;
    r = template_apply(t, to->path, apply_before, save_progress, &put);
    apply_done();
    log_msg("template %s into %s: %d save(s), %d", t->name, to->id, put, r);
    cards_recheck(to);
    card_back();
    if (r == MCFS_ERR_CANCELLED)
        return;   /* given up, and said so already */
    if (r == MCFS_OK || r == MCFS_ERR_FULL)   /* (free space is in whole clusters: a save may not fit what looked enough) */
        snprintf(s, sizeof(s), T(r == MCFS_OK ? T_TPL_APPLIED : T_TPL_APPLIED_FULL), to->base, put);
    else {
        op_result(r, 0, to);
        return;
    }
    message_wait(0, NULL, r == MCFS_OK ? COLOR_OK : COLOR_WARN, s);
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
    const char *items[4];
    char name[TPL_NAME + 1], s[300];
    template_t *t;
    int k = 0;
    for (;;) {   /* circle in what comes next comes back here; circle here goes back to the template */
        items[0] = T(T_TPL_APPLY_CARD);
        items[1] = T(T_TPL_ADD_SAVES);
        items[2] = T(T_TPL_RENAME);
        items[3] = T(T_TPL_DELETE);
        if ((k = choose(tplOpen->name, items, 4, k)) < 0)
            return 1;
        if (k == 0)
            template_to_card(tplOpen);
        else if (k == 1) {
            template_add_saves(tplOpen);
            return 1;   /* back to the template, where what it has now shows */
        } else if (k == 2) {
            snprintf(name, sizeof(name), "%s", tplOpen->name);
            if (!ask_name(name, tplOpen) || !strcmp(name, tplOpen->name))
                continue;
            message(0, NULL, COLOR_TEXT, T(T_TPL_RENAMING));
            if ((t = template_rename(tplOpen, name)) != NULL) {
                tplOpen = t;
                return 1;
            }
            tplOpen = template_find(tplCard.base);   /* (it may have moved in the list) */
            message_wait(0, NULL, COLOR_ERROR, T(T_TPL_RENAME_FAILED));
            if (!tplOpen)
                return 0;
        } else {
            snprintf(s, sizeof(s), T(T_TPL_DELETE_ASK), tplOpen->name);
            if (!confirm(s, T(T_TPL_DELETE_NOTE), T_DELETE))
                continue;
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
    int n, cursor, top;   /* the rows: "New template", then the templates */
} tl;

static const char *list_text(int i, char *buf)
{
    (void)buf;
    return i ? templates[i - 1].name : T(T_TPL_NEW);
}

static void scene_templates(float t)
{
    char s[64];
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    list_rows(tl.n, tl.cursor, tl.top, list_text, NULL);
    if (tl.top == 0) {   /* "New template": a plus before it, as "New card" has */
        float y = ROW_Y0 + ui_line_height(FONT_TEXT) / 2.0f + 1;
        ui_rect(LIST_X + 4, y - 1, 12, 2, COLOR_ACCENT, 0x70);
        ui_rect(LIST_X + 9, y - 6, 2, 12, COLOR_ACCENT, 0x70);
    }
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_OPEN)}};
        look_legend(l, 2, 0);
    }
    look_title(CARD_CX, 82, T(T_TEMPLATES), 1);
    if (!tl.cursor) {
        look_card(CARD_X, CARD_Y, "+", NULL);
        return;
    }
    look_card(CARD_X, CARD_Y, NULL, templates[tl.cursor - 1].name);
    snprintf(s, sizeof(s), T(T_TPL_SUMMARY), templates[tl.cursor - 1].n, (int)((templates[tl.cursor - 1].bytes + 1023) / 1024));
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, s);
}

/* the list is the templates there are now, with the cursor on the one of that name ("" = on "New template") */
static void list_set(const char *name)
{
    template_t *t = name[0] ? template_find(name) : NULL;
    ui_lock();
    tl.n = nTemplates + 1;
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
