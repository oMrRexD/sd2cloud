/* SD2Cloud -- the main screen: the cards on the left, in their groups, the selected one big on the right. */
#include "app.h"

/* -------- △ on a card: what can be done with it as a whole */

static void card_options(card_t *c)
{
    enum { OPT_SYNC, OPT_INCLUDED, OPT_RESTORE, OPT_DEVICE, OPT_MOVE, OPT_INSERT, OPT_RENAME, OPT_RESIZE, OPT_BY_GAME,
           OPT_CHECK, OPT_DELETE, OPTS };
    static const char *devices[FDEVS];
    static char move[96];
    const char *items[OPTS];
    int id[OPTS], n, k = 0, d;
    for (;;) {   /* circle in what comes next comes back here; circle here goes back to the main screen */
        n = 0;
        if (!viewOther) {   /* (the other device's card, not in the slot: never synced nor restored) */
            items[n] = T(T_SYNC_NOW), id[n++] = OPT_SYNC;
            items[n] = T(c->included ? T_SYNC_DISABLE : T_SYNC_ENABLE), id[n++] = OPT_INCLUDED;
            items[n] = T(T_RESTORE_BACKUP), id[n++] = OPT_RESTORE;
        }
        items[n] = T(T_COPY_DEVICE), id[n++] = OPT_DEVICE;
        if (c->moved[0] && !viewOther) {   /* the device opens another folder for its game now: its saves can go there */
            char to[64];
            snprintf(to, sizeof(to), "%.44s-%d", c->moved, c->channel);
            snprintf(move, sizeof(move), T(T_MOVE_SAVES), to);
            items[n] = move, id[n++] = OPT_MOVE;
        } else if (can_insert(c) && c != (activeCard >= 0 ? &cards[activeCard] : NULL))
            /* only a card the device can be told to take, and isn't on already */
            items[n] = T(dev->sd2psx ? T_INSERT : T_INSERT_PREVIEW), id[n++] = OPT_INSERT;
        items[n] = T(T_RENAME), id[n++] = OPT_RENAME;
        items[n] = T(T_RESIZE), id[n++] = OPT_RESIZE;
        items[n] = T(T_MIGRATE), id[n++] = OPT_BY_GAME;
        items[n] = T(T_CHECK_CARD), id[n++] = OPT_CHECK;
        /* (the card alone: its folder, whole, from the folder's own options, triangle on its row in the Games group.
         * Offered here too, right under this one, it was taken for it, the list going round from its top) */
        items[n] = T(T_DELETE_CARD), id[n++] = OPT_DELETE;
        if ((k = choose(c->base, items, n, k < n ? k : 0)) < 0)
            return;
        switch (id[k]) {
        case OPT_RENAME:
            if (card_rename_screen(c))
                return;   /* (its folder renamed: back to the main screen, which lists the cards again) */
            break;
        case OPT_BY_GAME:
            migrate_screen(c);
            return;   /* (its saves gone to their games' cards: back to the main screen, which has those cards) */
        case OPT_RESIZE:
            card_resize_screen(c);
            return;   /* (back to the main screen, where its size and status show) */
        case OPT_CHECK:
            card_check_screen(c);
            break;
        case OPT_DELETE:
            if (card_delete(c))
                return;   /* (gone: back to the main screen, which lists the cards again) */
            break;
        case OPT_SYNC:
            if (sync_card(c))
                return;   /* synced: back to the main screen, where its new status shows */
            break;
        case OPT_INCLUDED:   /* its sync off, or on again: back to the main screen, where its status says so */
            message(0, NULL, COLOR_TEXT, T(T_LOADING));
            if (card_set_included(c, !c->included) != 0)
                message_wait(0, NULL, COLOR_ERROR, T(T_ERR_EXPORT));
            return;
        case OPT_INSERT:
            insert_option(c);
            return;   /* back to the main screen, where the card in the sd2psx is marked */
        case OPT_MOVE:
            card_move_saves(c);
            return;
        case OPT_DEVICE: {   /* the whole card, as a file, to a folder of one of the devices */
            int list[FDEVS], n = fdev_list(list);
            for (d = 0; d < n; d++)
                devices[d] = T(deviceText[list[d]]);
            if ((d = choose(T(T_COPY_DEVICE), devices, n, 0)) >= 0) {
                fbCard = c;
                files_screen(list[d]);
                fbCard = NULL;
            }
            break;
        }
        default: {
            icon_t *ic = newest_icon(c);
            history_screen(c, ic);
            ui_scene(scene_frame);
            ui_lock();
            icon_free(ic);
            ui_unlock();
        }
        }
    }
}

/* -------- △ on a folder of the Games group: what can be done with the folder as a whole */

static void folder_options(card_t *c)
{
    const char *items[2];
    char title[100];
    int i, n = 0, k = 0, rename = folder_renamable(c->folder);
    for (i = 0; i < nCards; i++)
        n += !strcasecmp(cards[i].folder, c->folder);
    if (rename)   /* (a folder of the user's own: it can be called something else) */
        items[k++] = T(T_RENAME);
    items[k++] = T(T_DELETE_FOLDER);
    snprintf(title, sizeof(title), "%s", game_of(c));   /* (the game's name, cut to fit the box) */
    fit(FONT_TEXT, title, sizeof(title), 320);
    if ((i = choose(title, items, k, 0)) < 0)
        return;
    if (rename && i == 0)
        card_rename_screen(c);
    else if (n > 1)
        folder_delete(c);
    else   /* (a folder of one card: that card is asked about, and its folder goes with it) */
        card_delete(c);
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
    view_draw();
    tabs_draw(&menu.g, 1);
    if (menu.g.tab == TAB_LIBRARY) {   /* a game of the Library: its newest save on a game's card, how many it has */
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_MENU_EXIT)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_SELECT, T(T_TOOLS)},
                         {BUTTON_START, T(T_SETTINGS)}};
        int k = ROW_LIB(0) - menu.g.idx[menu.g.cursor];
        look_legend(l, 4, 2);
        if (!menu.g.n)
            return;
        panel_title(library_row_name(k));
        library_picture(k, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
        return;
    }
    if (menu.g.tab == TAB_FILES) {   /* the devices: what the group is for, where a card's picture would be */
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_MENU_EXIT)}, {BUTTON_CROSS, T(T_OPEN)}, {BUTTON_SELECT, T(T_TOOLS)},
                         {BUTTON_START, T(T_SETTINGS)}};
        look_legend(l, 4, 2);
        if (tabs_on_cloud(&menu.g)) {   /* (Google Drive: what it has there) */
            panel_title(T(T_SET_G_DRIVE));
            ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + 70, LOOK_CARD_W + 32, COLOR_DIM, T(T_CLOUD_HINT));
            return;
        }
        if (tabs_device(&menu.g) < 0) {   /* (the Memory Card) */
            memcard_hint(tabs_memcard_port(&menu.g));
            return;
        }
        panel_title(T(deviceText[tabs_device(&menu.g)]));
        ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + 70, LOOK_CARD_W + 32, COLOR_DIM, T(T_FILES_HINT));
        return;
    }
    {
        /* inside a folder circle goes back to the folders ("Back" and the other four don't fit the line: the tools
         * and the settings stay on SELECT and START, unsaid) */
        int inside = menu.g.open >= 0, made = menu.g.n && menu.g.idx[menu.g.cursor] == ROW_NEW, n = 5, apart = 2;
        legend_t l[5] = {{BUTTON_CIRCLE, T(inside ? T_BACK : T_MENU_EXIT)}, {BUTTON_CROSS, T(made ? T_TPL_CREATE : T_OPEN)},
                         {BUTTON_TRIANGLE, T(T_OPTIONS)}, {BUTTON_SELECT, T(T_TOOLS)}, {BUTTON_START, T(T_SETTINGS)}};
        /* "All saves", "New card": not a card, no options of one; nor a place of the VMCs (its folder is the OPL's) */
        if ((menu.g.n && menu.g.idx[menu.g.cursor] < 0) || (menu.g.tab == TAB_VMC && tabs_on_folders(&menu.g)))
            l[2] = l[3], l[3] = l[4], n = 4;
        else if (inside)
            n = 3, apart = 0;
        look_legend(l, n, apart);
    }
    if (menu.g.n && menu.g.idx[menu.g.cursor] == ROW_NEW && menu.g.tab == TAB_VMC) {   /* "New VMC" */
        panel_title(T(T_NEW_VMC));
        look_card(CARD_X, CARD_Y, "+", NULL);
        ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 4, LOOK_CARD_W + 32, COLOR_DIM, T(T_VMC_CREATE_HINT));
        return;
    }
    if (menu.g.n && menu.g.idx[menu.g.cursor] == ROW_NEW) {   /* "New card": what it makes */
        panel_title(T(T_NEW_CARD));
        look_card(CARD_X, CARD_Y, "+", NULL);
        ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 4, LOOK_CARD_W + 32, COLOR_DIM,
                            T(standalone ? T_CREATE_HINT_FILE : T_CREATE_HINT));
        return;
    }
    if (menu.g.n && menu.g.idx[menu.g.cursor] < 0) {   /* "All saves": the game cards, and how many they are */
        snprintf(s, sizeof(s), T(T_GAME_CARDS_N), menu.g.nGames);
        panel_title(T(T_ALL_SAVES));
        look_card(CARD_X, CARD_Y, NULL, T(T_TAB_GAMES));
        ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, s);
        return;
    }
    if (!(c = tabs_card(&menu.g))) {
        ui_paragraph(FONT_TEXT, LIST_X + 30, ROW_Y0, 150, COLOR_WARN, T(T_NO_CARDS));
        return;
    }
    if (tabs_on_folders(&menu.g)) {   /* a folder: the game's name whole, and the picture of its first card */
        const char *name = c->type == TYPE_VMC ? vmc_place_name(c->folder) : game_of(c);
        panel_title(name);
        draw_card_picture(c, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
        if (c->moved[0])   /* (the device opens another folder for this game now) */
            ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 4, COLOR_WARN, T(T_CARD_UNUSED));
        return;
    }
    panel_title(c->name[0] ? c->name : c->base);
    if (c->type == TYPE_VMC && vmc_games(c, NULL, 0) >= 0) {   /* under its name: the OPL's games that use it */
        snprintf(s, sizeof(s), T(T_VMC_GAMES), vmc_games(c, NULL, 0));
        ui_text_center(FONT_SMALL, CARD_CX, 104, COLOR_DIM, s);
    }
    draw_card_picture(c, menu.iconCard == menu.g.idx[menu.g.cursor] ? menu.icon : NULL, ui_clock());
    if (viewOther) {   /* (the other device's: no sync status, it isn't the slot's) */
        ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, COLOR_DIM, T(T_VIEW_NOTE));
        return;
    }
    ui_text_center(FONT_TEXT, CARD_CX, CARD_Y + LOOK_CARD_H + 2, status_color(c), status_text(c));
    last_backup(c, s, sizeof(s));
    if (c->moved[0])
        ui_text_center(FONT_SMALL, CARD_CX, CARD_Y + LOOK_CARD_H + 24, COLOR_WARN, T(T_CARD_UNUSED));
    else if (s[0]) {
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
    if (menu.g.tab == TAB_LIBRARY && menu.g.n && menu.iconCard != menu.g.idx[menu.g.cursor]) {   /* (a game's newest save) */
        ic = library_icon(ROW_LIB(0) - menu.g.idx[menu.g.cursor]);
        ui_lock();
        icon_free(menu.icon);
        menu.icon = ic;
        menu.iconCard = menu.g.idx[menu.g.cursor];
        ui_unlock();
        return;
    }
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
    int i, inside = menu.g.open >= 0;
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
    /* a microSD with the cards of both devices: which one it is in is asked once. And what the settings say gives way
     * to a microSD that has only one device's cards, or to a device that tells which it is */
    if (deviceBoth && !cfg.device[0])
        pick_device(1);
    else if (!deviceBoth && cfg.device[0] && strcasecmp(cfg.device, dev->sd2psx ? "sd2psx" : "pro2") != 0) {
        snprintf(cfg.device, sizeof(cfg.device), "%s", dev->sd2psx ? "sd2psx" : "pro2");
        config_set("general", "device", cfg.device);
    }
    message(0, NULL, COLOR_TEXT, T(T_SEARCHING));
    n = cards_scan();
    if (n < 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_NO_SD));
        leave(cfg.manual_return);
    }
    cards_check(checking_progress);
    cards_retry();
#ifdef DEBUG_BUILD
    if (debug_take('V'))
        debug_preview(debug_digit());
#endif
    helperState = helper_status();
    helper_follow();
    if (helperState == HELPER_SAME || helperState == HELPER_DIFFERENT)   /* (no sd2psx: where the program is, for it) */
        helper_point();
    find_active();
#ifdef DEBUG_BUILD
    {   /* checks the in-use detection on PCSX2: the root signature of the card in slot 1 against each .mcd's */
        char seen[65], file[65];
        int same = -1;
        if (mc_root_signature(0, seen) == 0)
            for (i = 0; i < nCards; i++)
                if (mcfs_root_signature(cards[i].path, file) == 0) {
                    log_msg("root signature: slot 1 %.16s, %s %.16s%s%s", seen, cards[i].id, file, strcmp(seen, file) ? "" : "  <- same card",
                            cards[i].rootSig[0] && strcmp(cards[i].rootSig, file) ? "  (NOT the one read with its index)" : "");
                    if (!strcmp(seen, file))
                        same = i;
                }
        /* script "a": that card is taken as the one the device is using (PCSX2 has no device to say so): what is
         * done to it then goes through the slot, as on a console */
        if (same >= 0 && debug_take('a')) {
            activeCard = same;
            log_msg("[script] a: %s is taken as the card in use", cards[same].id);
        }
        /* script "t": the automatic sync's step that puts the templates into the game cards, as after a game (the
         * sync itself needs a Google account, which PCSX2 has none of) */
        /* script "j": the question square brings up while cards are sent (the sync needs a Google account) */
        if (nCards && debug_take('j'))
            log_msg("[script] j: skip? %d", confirm_skip(&cards[0]));
        if (debug_take('t')) {
            log_msg("[script] t: the templates, as after a game");
            templates_after_game(0);
        }
        /* script "B": a sync of every card, with nothing sent (PCSX2 has no Google account): its list of cards */
        if (debug_take('B')) {
            for (i = 0; i < nCards; i++)   /* (and, first, what the automatic sync would send now) */
                if (is_selected(&cards[i], 0))
                    log_msg("[script] B: the automatic sync sends %s", cards[i].id);
            log_msg("[script] B: a sync with nothing sent");
            debugDry = 1;
            manual_backup(2);
            debugDry = 0;
        }
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
    if (debug_take('r'))   /* the first sync's question, without an account (X on it fails at the sign-in) */
        first_run();
    if (debug_take('d'))   /* Google Drive in the Files group, with made-up folders (PCSX2 has no Google account) */
        cloudFake = 1;
#endif
    if (google_has_access() && state_empty())
        first_run();
    drive_offer_startup();   /* (backups kept in the first layout on Drive: the move to the second, offered once) */
    remember_unseen();
    templates_startup();   /* a game card that lacks the main template (the card of a new game): said now */
    /* the cursor starts on the first card that needs a backup, in its group */
    for (i = 0; i < nCards && !is_selected(&cards[i], 1); i++)
        ;
    ui_lock();
    tabs_init(&menu.g, NULL, 1);
    menu.g.library = 1;
    menu.g.cloud = 1;
    menu.g.all = 1;
    menu.g.create = 1;   /* (the Cards group starts with "New card": with no sd2psx, no Cards group, nor the row) */
    tabs_show(&menu.g, menu.g.tab);
    if (i < nCards) {
        tabs_show(&menu.g, tab_of(&cards[i]));
        tabs_find(&menu.g, i);
    }
    ui_unlock();
    for (;;) {
        u32 b, keys = PAD_UP | PAD_DOWN | TAB_KEYS | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_START | PAD_SELECT |
                         (device_other() ? VIEW_KEYS : 0);
        ui_scene(scene_menu);
#ifdef DEBUG_BUILD
        if (debug_take('G') || (debugUnplugged && debug_take('p'))) {   /* (the sd2psx taken out, and put back: PCSX2
                                                                           has none, the watch is skipped) */
            debugUnplugged = !debugUnplugged;
            device_changed();
        }
#endif
        if (cardsUnsorted)   /* a card made for a save: into its place, now that nothing holds on to the others */
            cards_resort();
        /* the selected card's icon is read when the cursor rests a moment; a card not read yet, every few seconds */
        b = wait_nav_ms(keys, 250);
        if (!b) {
            menu_icon();
            /* the OPL's settings, for the games that use each VMC (under its name): read a little at a time while
             * nothing is pressed, whichever group is shown, so that they are there when the VMCs' is opened (all at
             * once, they held the screen for seconds on an sd2psx) */
            while (nVmcPlaces && !vmc_links_step(20) && !(b = wait_nav_ms(keys, 10)))
                ;
            while (!b && cards_unread() && !(b = wait_nav_ms(keys, 3000)))
                card_reread();
            if (!b)
                b = wait_nav(keys);
        }
        if (b & VIEW_KEYS) {   /* the other device's cards, or the slot's again */
            if (!view_key(b))
                continue;
            cards_resort();
            if (!menu.g.count[menu.g.tab]) {   /* (a group that device has none of: the Boot cards) */
                int k;
                ui_lock();
                for (k = 0; k < TABS && !menu.g.count[k]; k++)
                    ;
                tabs_show(&menu.g, k < TABS ? k : TAB_CARDS);
                ui_unlock();
            }
            continue;
        }
        /* (X opens it. A place of the VMCs has none: its folder is the OPL's, not to be renamed or deleted here) */
        if ((b & PAD_TRIANGLE) && tabs_on_folders(&menu.g) && tabs_card(&menu.g) && menu.g.tab != TAB_VMC) {
            sound_play(SND_CONFIRM);
            folder_options(tabs_card(&menu.g));
            continue;
        }
        if (tabs_nav(&menu.g, b) || tabs_folder_nav(&menu.g, b))
            continue;
        if ((b & PAD_CROSS) && menu.g.tab != TAB_FILES && menu.g.n && menu.g.idx[menu.g.cursor] == ROW_NEW) {   /* "New card" */
            sound_play(SND_CONFIRM);
            if (menu.g.tab == TAB_VMC)
                vmc_create(NULL, 0);
            else
                card_create(NULL, 0);
        } else if ((b & PAD_CROSS) && menu.g.tab == TAB_GAMES && menu.g.n && menu.g.idx[menu.g.cursor] < 0) {
            sound_play(SND_CONFIRM);
            card_screen(&allGames);   /* "All saves" */
        } else if ((b & PAD_CROSS) && menu.g.tab == TAB_LIBRARY && menu.g.n) {   /* a game's saves in the Library */
            int k;
            sound_play(SND_CONFIRM);
            library_open(ROW_LIB(0) - menu.g.idx[menu.g.cursor]);
            ui_lock();   /* (saves moved out or in: its games as they are now; none left, the group goes) */
            icon_free(menu.icon);
            menu.icon = NULL;
            menu.iconCard = -1;
            tabs_show(&menu.g, TAB_LIBRARY);
            for (k = 0; k < TABS && !menu.g.count[menu.g.tab]; k++)
                tabs_show(&menu.g, k);
            ui_unlock();
        } else if ((b & PAD_CROSS) && menu.g.tab == TAB_FILES) {
            sound_play(SND_CONFIRM);
            if (tabs_on_cloud(&menu.g))
                cloud_screen();
            else if (tabs_device(&menu.g) < 0) {
                memcard_pick(tabs_memcard_port(&menu.g));
                memcard_screen();
            }
            else
                files_screen(tabs_device(&menu.g));
            ui_lock();
            tabs_show(&menu.g, TAB_FILES);   /* (the Memory Card may have been taken out meanwhile) */
            ui_unlock();
        } else if ((b & PAD_CROSS) && tabs_card(&menu.g)) {
            card_t *c = tabs_card(&menu.g);
            sound_play(SND_CONFIRM);
            if (c->status == ST_ERROR) {   /* not read when the list was made: read again first */
                message(0, NULL, COLOR_TEXT, T(T_LOADING));
                cards_recheck(c);
            }
            card_screen(c);
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
