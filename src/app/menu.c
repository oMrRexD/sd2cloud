/* SD2Cloud -- the main screen: the cards on the left, in their groups, the selected one big on the right. */
#include "app.h"

/* -------- △ on a card: what can be done with it as a whole */

static void card_options(card_t *c)
{
    static const char *items[4], *devices[FDEVS];
    int k = 0, d;
    for (;;) {   /* circle in what comes next comes back here; circle here goes back to the main screen */
        items[0] = T(T_SYNC_NOW);
        items[1] = T(T_RESTORE_BACKUP);
        items[2] = T(T_COPY_DEVICE);
        /* only a card the device can be told to take, and isn't on already */
        items[3] = T(dev->sd2psx ? T_INSERT : T_INSERT_PREVIEW);
        if ((k = choose(c->base, items, can_insert(c) && c != (activeCard >= 0 ? &cards[activeCard] : NULL) ? 4 : 3, k)) < 0)
            return;
        if (k == 0) {
            if (sync_card(c))
                return;   /* synced: back to the main screen, where its new status shows */
        } else if (k == 3) {
            insert_option(c);
            return;   /* back to the main screen, where the card in the sd2psx is marked */
        } else if (k == 2) {   /* the whole card, as a file, to a folder of the microSD or of a USB drive */
            for (d = 0; d < FDEVS; d++)
                devices[d] = T(deviceText[d]);
            if ((d = choose(T(T_COPY_DEVICE), devices, FDEVS, 0)) >= 0) {
                fbCard = c;
                files_screen(d);
                fbCard = NULL;
            }
        } else {
            icon_t *ic = newest_icon(c);
            history_screen(c, ic);
            ui_scene(scene_frame);
            ui_lock();
            icon_free(ic);
            ui_unlock();
        }
    }
}

/* -------- the main screen: the cards on the left, in their groups (tabs), the selected one big on the right */

static struct {
    tabs_t g;
    icon_t *icon;             /* the 3D icon of the selected game card, once read */
    int iconCard;             /* which card that icon is of (index in cards[], -1 = none) */
} menu = {.iconCard = -1};

/* a card joined cards[] and the others moved over: the main screen's list is made again */
void menu_refresh(void)
{
    ui_lock();
    icon_free(menu.icon);
    menu.icon = NULL;
    menu.iconCard = -1;
    tabs_show(&menu.g, menu.g.tab);
    ui_unlock();
}

static void scene_menu(float t)
{
    char s[64];
    const card_t *c;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    tabs_draw(&menu.g, 1);
    if (menu.g.tab == TAB_FILES) {   /* the devices: what the group is for, where a card's picture would be */
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_MENU_EXIT)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_SELECT, T(T_TOOLS)},
                         {BUTTON_START, T(T_SETTINGS)}};
        look_legend(l, 4, 2);
        ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, T(deviceText[menu.g.cursor]));
        ui_paragraph(FONT_SMALL, CARD_X - 16, CARD_Y + 70, LOOK_CARD_W + 32, COLOR_DIM, T(T_FILES_HINT));
        return;
    }
    {
        /* inside a folder of the Games group circle goes back to the folders ("Back" and the other four don't fit
         * the line: the tools and the settings stay on SELECT and START, unsaid) */
        int inside = menu.g.tab == TAB_GAMES && menu.g.open >= 0;
        legend_t l[5] = {{BUTTON_CIRCLE, T(inside ? T_BACK : T_MENU_EXIT)}, {BUTTON_CROSS, T(T_OPEN)},
                         {BUTTON_TRIANGLE, T(T_OPTIONS)}, {BUTTON_SELECT, T(T_TOOLS)}, {BUTTON_START, T(T_SETTINGS)}};
        if (menu.g.n && menu.g.idx[menu.g.cursor] < 0) {   /* "All saves": not a card, there are no options of one */
            l[2] = l[3], l[3] = l[4];
            look_legend(l, 4, 2);
        } else
            look_legend(l, inside ? 3 : 5, inside ? 0 : 2);
    }
    if (menu.g.n && menu.g.idx[menu.g.cursor] < 0) {   /* "All saves": the game cards, and how many they are */
        snprintf(s, sizeof(s), T(T_GAME_CARDS_N), menu.g.nGames);
        ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, T(T_ALL_SAVES));
        look_card(CARD_X, CARD_Y, NULL, T(T_TAB_GAMES));
        ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, s);
        return;
    }
    if (!(c = tabs_card(&menu.g))) {
        ui_paragraph(FONT_TEXT, LIST_X + 30, ROW_Y0, 150, COLOR_WARN, T(T_NO_CARDS));
        return;
    }
    if (tabs_on_folders(&menu.g)) {   /* a folder: the game's name whole, and the picture of its first card */
        const char *name = game_of(c);
        if (ui_measure(FONT_TEXT, name) <= LOOK_CARD_W + 56)
            ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, name);
        else
            ui_text_fit(FONT_TEXT, CARD_CX - (LOOK_CARD_W + 56) / 2, 82, LOOK_CARD_W + 56, 0x7E8AA0, name);
        draw_card_picture(c, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
        return;
    }
    ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, c->name[0] ? c->name : c->base);
    draw_card_picture(c, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, status_color(c), status_text(c));
    last_backup(c, s, sizeof(s));
    if (s[0]) {
        char l[96];
        snprintf(l, sizeof(l), T(T_CARD_LAST), s);
        ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_DIM, l);
    }
}

/* the selected game card's icon, for its big card (read after the cursor rests on it a moment) */
static void menu_icon(void)
{
    icon_t *ic = NULL;
    card_t *c = tabs_card(&menu.g);
    if (!c || menu.iconCard == menu.g.idx[menu.g.cursor])
        return;
    if (c->type == TYPE_GAMEID)
        ic = newest_icon(c);
    ui_lock();
    icon_free(menu.icon);
    menu.icon = ic;
    menu.iconCard = menu.g.idx[menu.g.cursor];
    ui_unlock();
}

/* A card made while a save's destination was picked sits at the end of cards[], so that the cards that were being
 * worked with kept their places. Back on the main screen nothing points at a card any more: the list is put in order,
 * and the card in use and the one the device was moved off of are found again by their names */
static void cards_resort(void)
{
    char active[96], moved[96], shown[96];
    const card_t *c = tabs_card(&menu.g);
    int i, inside = menu.g.tab == TAB_GAMES && menu.g.open >= 0;
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    snprintf(moved, sizeof(moved), "%s", movedOff ? movedOff->id : "");
    snprintf(shown, sizeof(shown), "%s", c ? c->id : "");
    ui_lock();
    cards_sort();
    for (activeCard = nCards - 1; activeCard >= 0 && strcmp(cards[activeCard].id, active); activeCard--)
        ;
    movedOff = NULL;
    for (i = 0; i < nCards && moved[0]; i++)
        if (!strcmp(cards[i].id, moved))
            movedOff = &cards[i];
    transferDest = NULL;
    cardsUnsorted = 0;
    icon_free(menu.icon);
    menu.icon = NULL;
    menu.iconCard = -1;
    /* the main screen, on the card it was on: a folder that was open may be another one of the list now */
    menu.g.open = -1;
    tabs_show(&menu.g, menu.g.tab);
    for (i = 0; i < nCards && strcmp(cards[i].id, shown); i++)
        ;
    if (shown[0] && i < nCards) {
        tabs_find(&menu.g, i);
        if (inside) {
            tabs_open(&menu.g);
            menu.g.moved = -1;   /* (it was open already: nothing slides) */
            tabs_find(&menu.g, i);
        }
    }
    ui_unlock();
}

void manual(void)
{
    int n, r, i;
    message(0, NULL, COLOR_TEXT, T(T_SEARCHING));
    n = cards_scan();
    if (n < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_NO_SD));
        leave(cfg.manual_return);
    }
    cards_check(checking_progress);
#ifdef DEBUG_BUILD
    if (debug_take('V'))
        debug_preview(debug_digit());
#endif
    helperState = helper_status();
    find_active();
#ifdef DEBUG_BUILD
    {   /* checks the in-use detection on PCSX2: the root signature of the card in slot 1 against each .mcd's */
        char seen[65], file[65];
        if (mc_root_signature(0, seen) == 0)
            for (i = 0; i < nCards; i++)
                if (mcfs_root_signature(cards[i].path, file) == 0)
                    log_msg("root signature: slot 1 %.16s, %s %.16s%s%s", seen, cards[i].id, file, strcmp(seen, file) ? "" : "  <- same card",
                            cards[i].rootSig[0] && strcmp(cards[i].rootSig, file) ? "  (NOT the one read with its index)" : "");
    }
#endif
    if (!google_has_access() && !cfg.no_ask_connect && ask_connect()) {
        googleError[0] = 0;
        r = ensure_google(1);
        if (r == 0)
            offer_auto_sync();
        else if (r > 0) {
            char t[400];
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            dlg_new(COLOR_ERROR, T(T_LOGIN_ERROR));
            dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
            dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
            dlg_show();
            wait_button(PAD_CROSS | PAD_CIRCLE, 0);
            sound_play(SND_BACK);
        }
    }
    note_igr_auto(NULL);
#ifdef DEBUG_BUILD
    if (debug_take('N'))
        debugNoLinkOnce = 1;
    if (debug_take('n'))
        debugNoDhcpOnce = 1;
    if (debug_take('u'))
        debugNoZero = 1;
#endif
#ifdef DEBUG_BUILD
    if (debug_take('A'))   /* the questions after the first sign-in, without signing in */
        offer_auto_sync();
#endif
    if (google_has_access() && state_empty())
        first_run();
    remember_unseen();
    templates_startup();   /* a game card that lacks the main template (the card of a new game): said now */
    /* the cursor starts on the first card that needs a backup, in its group */
    for (i = 0; i < nCards && !is_selected(&cards[i], 1); i++)
        ;
    ui_lock();
    tabs_init(&menu.g, NULL, 1);
    menu.g.all = 1;
    tabs_show(&menu.g, menu.g.tab);
    if (i < nCards) {
        tabs_show(&menu.g, tab_of(&cards[i]));
        tabs_find(&menu.g, i);
    }
    ui_unlock();
    for (;;) {
        u32 b, keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_START | PAD_SELECT;
        ui_scene(scene_menu);
#ifdef DEBUG_BUILD
        if (debug_take('G')) {
            debugGone = now_ms() + 3000;
            device_lost();
        }
#endif
        if (cardsUnsorted)   /* a card made for a save: into its place, now that nothing holds on to the others */
            cards_resort();
        /* the selected card's icon is read when the cursor rests a moment */
        b = wait_nav_ms(keys, 250);
        if (!b) {
            menu_icon();
            b = wait_nav(keys);
        }
        if (tabs_nav(&menu.g, b) || tabs_folder_nav(&menu.g, b))
            continue;
        if ((b & PAD_CROSS) && menu.g.tab == TAB_GAMES && menu.g.n && menu.g.idx[menu.g.cursor] < 0) {
            sound_play(SND_CONFIRM);
            card_screen(&allGames);   /* "All saves" */
        } else if ((b & PAD_CROSS) && menu.g.tab == TAB_FILES) {
            sound_play(SND_CONFIRM);
            files_screen(menu.g.cursor);
        } else if ((b & PAD_CROSS) && tabs_card(&menu.g)) {
            sound_play(SND_CONFIRM);
            card_screen(tabs_card(&menu.g));
        } else if ((b & PAD_TRIANGLE) && tabs_card(&menu.g)) {
            sound_play(SND_CONFIRM);
            card_options(tabs_card(&menu.g));
        } else if (b & PAD_START) {
            sound_play(SND_CONFIRM);
            settings_screen();
        } else if (b & PAD_SELECT) {
            sound_play(SND_CONFIRM);
            tools_screen();
        } else if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            exit_menu();
        }
    }
}
