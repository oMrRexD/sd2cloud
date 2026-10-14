/* SD2Cloud -- a card's saves sent each to its game's card, as the sd2psx's Game ID keeps them: what a card of many
 * games (one used before Game ID, the Memory Card, a card file) becomes. The saves are read and grouped by game first,
 * and the list shows where each game's go: its card, or a new one of the size recommended for them. Nothing is
 * copied until START; the card they come from is only changed if, at the end, the user says so. */
#include "app.h"

#define MG_GAMES 128
#define MG_X     70
#define MG_RIGHT 570
#define MG_Y0    114
#define MG_ROW   26
#define MG_ROWS  8

typedef struct {
    char id[12];        /* the game's ID, as its saves' folders have it */
    char folder[48];    /* where the device keeps that game's cards (Game2Folder.ini's, or the ID) */
    char name[64];      /* how the list names it: the game's name, or its ID */
    int first, n;       /* its saves in mg.order */
    long long room;     /* what they take of a card, at most */
    card_t *card;       /* its card on the microSD (channel 1); NULL = a new one, of mb */
    int mb;
    int on;             /* its saves go (X takes it out, and back in) */
} mg_game_t;

static struct {
    card_t *src;
    mcfs_save_t saves[MCFS_MAX_SAVES];
    int n;
    int order[MCFS_MAX_SAVES], of[MCFS_MAX_SAVES];   /* the saves that go somewhere, by game; and each one's game */
    int nOrder;
    mg_game_t games[MG_GAMES];
    int nGames;
    int stay;           /* saves without a game's ID in their folder's name: homebrew, exploits, the system's data */
    int cursor, top;
    /* what became of each save (MG_*), once START was pressed */
    unsigned char done[MCFS_MAX_SAVES];
} mg;
enum { MG_WAITING, MG_COPIED, MG_THERE, MG_KEPT, MG_FAILED };

/* the saves of the card, grouped by the game their folder names (0 = it could be read) */
static int mg_read(void)
{
    char id[12], folder[48];
    long long bytes, freeBytes;
    int i, k, files;
    mg.n = mcfs_list_saves(mg.src->path, mg.saves, MCFS_MAX_SAVES, &freeBytes);
    if (mg.n < 0)
        return -1;
    mg.nGames = mg.nOrder = mg.stay = 0;
    for (i = 0; i < mg.n; i++) {
        mg.of[i] = -1;
        mg.done[i] = MG_WAITING;
        if (!save_game_id(mg.saves[i].folder, id)) {
            mg.stay++;
            continue;
        }
        game_folder(id, folder, sizeof(folder));
        if (!strcasecmp(folder, mg.src->folder) && mg.src >= cards && mg.src < cards + nCards)
            continue;   /* (on its game's card already: this is one) */
        for (k = 0; k < mg.nGames && strcasecmp(mg.games[k].folder, folder); k++)
            ;
        if (k == mg.nGames) {
            mg_game_t *g;
            if (mg.nGames == MG_GAMES)
                continue;
            g = &mg.games[mg.nGames++];
            memset(g, 0, sizeof(*g));
            snprintf(g->id, sizeof(g->id), "%s", id);
            snprintf(g->folder, sizeof(g->folder), "%s", folder);
            if (!game_title(id, g->name, sizeof(g->name)))
                snprintf(g->name, sizeof(g->name), "%s", id);
            g->on = 1;
        }
        bytes = 0, files = 0;
        mcfs_save_info(mg.src->path, mg.saves[i].folder, &bytes, &files);
        mg.games[k].room += mcfs_save_room(bytes, files);
        mg.games[k].n++;
        mg.of[i] = k;
    }
    /* each game's saves together, the games by name; each one's card, or the size of a new one */
    for (k = 0; k < mg.nGames; k++) {
        mg_game_t *g = &mg.games[k];
        for (i = 0; i < nCards && !(cards[i].channel == 1 && !strcasecmp(cards[i].folder, g->folder)); i++)
            ;
        g->card = i < nCards ? &cards[i] : NULL;
        g->mb = mcfs_recommended_mb(g->room, dev->minMb, dev->maxMb);
        if (!g->mb)
            g->mb = dev->maxMb;
    }
    for (i = 0; i < mg.nGames; i++)   /* (by name: a few dozen, at most) */
        for (k = i + 1; k < mg.nGames; k++)
            if (strcasecmp(mg.games[k].name, mg.games[i].name) < 0) {
                mg_game_t t = mg.games[i];
                int j;
                mg.games[i] = mg.games[k];
                mg.games[k] = t;
                for (j = 0; j < mg.n; j++)
                    mg.of[j] = mg.of[j] == i ? k : mg.of[j] == k ? i : mg.of[j];
            }
    for (k = 0; k < mg.nGames; k++) {
        mg.games[k].first = mg.nOrder;
        for (i = 0; i < mg.n; i++)
            if (mg.of[i] == k)
                mg.order[mg.nOrder++] = i;
    }
    log_msg("by game, %s: %d saves, %d game(s), %d to stay", mg.src->id, mg.n, mg.nGames, mg.stay);
    return 0;
}

static void scene_migrate(float t)
{
    char s[160];
    int i, y, lh = ui_line_height(FONT_TEXT), going = 0, games = 0;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_title(MG_X, 82, T(T_MIGRATE_TITLE), 0);
    ui_text_right(FONT_TEXT, MG_RIGHT, 82, COLOR_DIM, mg.src->base);   /* (the card they are on) */
    for (i = mg.top, y = MG_Y0; i < mg.nGames && i < mg.top + MG_ROWS; i++, y += MG_ROW) {
        const mg_game_t *g = &mg.games[i];
        float my = y + lh / 2.0f + 1;
        int w = MG_RIGHT - 190 - (MG_X + 28);
        if (g->on) {   /* a green tick, as a marked save has: its saves go */
            int k;
            for (k = 0; k < 2; k++) {
                ui_line(MG_X + 3, my - 1 + k, MG_X + 7, my + 3 + k, COLOR_OK, 0x80);
                ui_line(MG_X + 7, my + 3 + k, MG_X + 15, my - 6 + k, COLOR_OK, 0x80);
            }
        }
        snprintf(s, sizeof(s), "%s (%d)", g->name, g->n);   /* (and how many of its saves) */
        if (ui_measure(FONT_TEXT, s) > w) {
            char n[16];
            snprintf(n, sizeof(n), " (%d)", g->n);
            snprintf(s, sizeof(s), "%s", g->name);
            fit(FONT_TEXT, s, sizeof(s), w - ui_measure(FONT_TEXT, n));
            snprintf(s + strlen(s), sizeof(s) - strlen(s), "%s", n);
        }
        if (i == mg.cursor)
            look_item(MG_X + 28, y, s, 1, 0);
        else
            ui_text(FONT_TEXT, MG_X + 28, y, g->on ? COLOR_ITEM : COLOR_DIM, s);
        if (!g->on)
            snprintf(s, sizeof(s), "%s", T(T_MIGRATE_OFF));
        else if (g->card)
            snprintf(s, sizeof(s), "%s", g->card->base);
        else
            snprintf(s, sizeof(s), T(T_MIGRATE_NEW), g->mb);
        ui_text_right(FONT_SMALL, MG_RIGHT, y + 2, g->on ? (g->card ? COLOR_TEXT : COLOR_OK) : COLOR_DIM, s);
    }
    if (mg.top > 0)
        look_arrow(W / 2.0f, MG_Y0 - 12, 0, 0x6E9AE0);
    if (mg.top + MG_ROWS < mg.nGames)
        look_arrow(W / 2.0f, MG_Y0 + MG_ROWS * MG_ROW + 2, 1, 0x6E9AE0);
    for (i = 0; i < mg.nGames; i++)
        if (mg.games[i].on)
            going += mg.games[i].n, games++;
    if (!mg.nGames)
        ui_paragraph_center(FONT_TEXT, W / 2.0f, 180, 420, COLOR_DIM, T(T_MIGRATE_NONE));
    /* under the list, past its arrow: how many go, how many stay */
    snprintf(s, sizeof(s), T(T_MIGRATE_SUMMARY), going, games, mg.stay);
    ui_text_fit(FONT_SMALL, MG_X + 4, MG_Y0 + MG_ROWS * MG_ROW + 20, MG_RIGHT - MG_X - 70, 0x8E98AA, s);
    if (mg.nGames) {
        snprintf(s, sizeof(s), "%d/%d", mg.cursor + 1, mg.nGames);
        ui_text_right(FONT_SMALL, MG_RIGHT, MG_Y0 + MG_ROWS * MG_ROW + 20, 0x8E98AA, s);
    }
    {
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_BACK)}};
        int n = 1;
        if (mg.nGames) {
            l[n].button = BUTTON_CROSS, l[n++].text = T(T_MARK);
            if (!mg.games[mg.cursor].card && mg.games[mg.cursor].on)
                l[n].button = BUTTON_TRIANGLE, l[n++].text = T(T_MIGRATE_SIZE);
            if (going)
                l[n].button = BUTTON_START, l[n++].text = T(T_MIGRATE_START);
        }
        look_legend(l, n, 0);
    }
}

/* what to do with a save the game's card has already, saved differently: asked once, for all of them. 0 = keep the
 * game card's, 1 = put this card's in its place, 2 = keep the newer; -1 = back. Saves that are the same on both are
 * left out, said or not */
static int mg_conflicts(int *same)
{
    static mcfs_save_t there[MCFS_MAX_SAVES];
    unsigned long long a, b;
    int k, i, j, n, differ = 0;
    *same = 0;
    for (k = 0; k < mg.nGames; k++) {
        mg_game_t *g = &mg.games[k];
        if (!g->on || !g->card || (n = mcfs_list_saves(g->card->path, there, MCFS_MAX_SAVES, NULL)) <= 0)
            continue;
        for (i = g->first; i < g->first + g->n; i++) {
            const mcfs_save_t *s = &mg.saves[mg.order[i]];
            for (j = 0; j < n && strcmp(there[j].folder, s->folder); j++)
                ;
            if (j == n)
                continue;
            /* the same files with the same bytes on both: nothing to copy */
            if (mcfs_save_signatures(mg.src->path, s, 1, &a) == 0 && mcfs_save_signatures(g->card->path, &there[j], 1, &b) == 0 &&
                a && a == b) {
                mg.done[mg.order[i]] = MG_THERE;
                (*same)++;
            } else {
                mg.done[mg.order[i]] = there[j].when >= s->when ? MG_KEPT + 0x10 : MG_KEPT + 0x20;   /* (which is newer) */
                differ++;
            }
        }
    }
    if (!differ)
        return 0;
    {
        static const char *items[3];
        char t[200];
        items[0] = T(T_MIGRATE_KEEP_GAME);
        items[1] = T(T_MIGRATE_REPLACE);
        items[2] = T(T_MIGRATE_NEWER);
        snprintf(t, sizeof(t), T(T_MIGRATE_DIFFER), differ);
        return choose(t, items, 3, 0);
    }
}

/* START: asked, and then the games' saves copied, a game at a time (its new card made first). 1 = done (the list is
 * over), 0 = back to it */
static int mg_go(void)
{
    char t[300], name[100];
    int k, i, policy, same, copied = 0, there = 0, failed = 0, going = 0, games = 0, made = 0, step = 0;
    for (k = 0; k < mg.nGames; k++)
        if (mg.games[k].on)
            going += mg.games[k].n, games++, made += !mg.games[k].card;
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    for (i = 0; i < mg.n; i++)
        mg.done[i] = MG_WAITING;
    if ((policy = mg_conflicts(&same)) < 0)
        return 0;
    snprintf(t, sizeof(t), T(T_MIGRATE_ASK), going, games);
    dlg_new(COLOR_TITLE, t);
    if (made) {
        snprintf(t, sizeof(t), T(T_MIGRATE_ASK_NEW), made);
        dlg_line(FONT_TEXT, COLOR_TEXT, 6, t);
    }
    dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_MIGRATE_ASK_KEEP));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_MIGRATE_START);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    for (k = 0; k < mg.nGames; k++) {
        mg_game_t *g = &mg.games[k];
        int fresh = !g->card;
        if (!g->on)
            continue;
        if (fresh && !(g->card = dest_card_sized(g->folder, 1, (long long)g->mb << 20))) {
            for (i = g->first; i < g->first + g->n; i++)   /* (couldn't be made: said, and its saves didn't go) */
                mg.done[mg.order[i]] = MG_FAILED, failed++;
            continue;
        }
        if (card_free(g->card, mg.src)) {   /* (the device is on it, and couldn't be moved off: its saves don't go) */
            for (i = g->first; i < g->first + g->n; i++)
                mg.done[mg.order[i]] = MG_FAILED, failed++;
            continue;
        }
        card_work(mg.src, 0, T(T_RESIZE_COPYING), 1);
        for (i = g->first; i < g->first + g->n; i++) {
            int s = mg.order[i], r, newer = mg.done[s] & 0x20;
            upload_screen(step++, going);
            if (mg.done[s] == MG_THERE) {
                there++;
                continue;
            }
            if ((mg.done[s] & 0xF) == MG_KEPT) {   /* a save of that folder, saved differently, is on the game's card */
                if (policy == 0 || (policy == 2 && !newer)) {
                    mg.done[s] = MG_KEPT;
                    continue;
                }
                mcfs_delete_save(g->card->path, mg.saves[s].folder);
            }
            r = mcfs_copy_save(mg.src->path, mg.saves[s].folder, g->card->path, NULL);
            mg.done[s] = r == MCFS_OK ? MG_COPIED : MG_FAILED;
            if (r == MCFS_OK)
                copied++;
            else
                failed++;
            log_msg("by game: %s into %s: %d", mg.saves[s].folder, g->card->id, r);
        }
        card_work_done();
        cards_recheck(g->card);
        card_back();
        if (fresh)   /* (a new game card: its templates too, as the device's new cards get them) */
            templates_new_card(g->card);
    }
    /* how it went */
    snprintf(t, sizeof(t), T(T_TPL_COPIED_N), copied);
    dlg_new(failed ? COLOR_WARN : COLOR_OK, t);
    if (there) {
        snprintf(t, sizeof(t), T(T_MIGRATE_THERE_N), there);
        dlg_line(FONT_TEXT, COLOR_TEXT, 2, t);
    }
    for (i = 0, k = 0; i < mg.n; i++)
        k += mg.done[i] == MG_KEPT;
    if (k) {
        snprintf(t, sizeof(t), T(T_MIGRATE_KEPT_N), k);
        dlg_line(FONT_TEXT, COLOR_TEXT, 2, t);
    }
    if (failed) {
        snprintf(t, sizeof(t), T(T_TPL_NOT_COPIED), failed);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
    }
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
    /* the card they came from: left as it is, unless the user says so (the saves that are on their game's card
     * now, copied or there already, can go) */
    if (copied + there) {
        snprintf(name, sizeof(name), "%s", mg.src->base);
        snprintf(t, sizeof(t), T(T_MIGRATE_DELETE_ASK), name);
        dlg_new(COLOR_TITLE, t);
        snprintf(t, sizeof(t), T(T_MIGRATE_DELETE_TEXT), copied + there);
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
        dlg_buttons(BUTTON_CIRCLE, T_MIGRATE_KEEP, BUTTON_CROSS, T_DELETE);
        next.wide = 1;
        dlg_show();
        if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) {
            int gone = 0;
            sound_play(SND_CONFIRM);
            if (!card_free(mg.src, NULL)) {
                message(0, NULL, COLOR_TEXT, T(T_DELETING));
                for (i = 0; i < mg.n; i++)
                    if ((mg.done[i] == MG_COPIED || mg.done[i] == MG_THERE) &&
                        mcfs_delete_save(mg.src->path, mg.saves[i].folder) == MCFS_OK)
                        gone++;
                log_msg("by game: %d save(s) deleted from %s", gone, mg.src->id);
                if (mg.src >= cards && mg.src < cards + nCards)
                    cards_recheck(mg.src);
                card_back();
                op_result(MCFS_OK, T_DONE_DELETE, NULL);
            }
        } else
            sound_play(SND_BACK);
    }
    return 1;
}

void migrate_screen(card_t *src)
{
    mg.src = src;
    mg.cursor = mg.top = 0;
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    if (mg_read() != 0) {
        message_wait(0, NULL, COLOR_ERROR, T(T_CARD_UNREADABLE));
        return;
    }
    ui_scene(scene_migrate);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_START);
        mg_game_t *g = mg.nGames ? &mg.games[mg.cursor] : NULL;
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return;
        }
        if ((b & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) && mg.nGames) {
            int k = mg.cursor;
            if (b & PAD_UP)
                k = (k + mg.nGames - 1) % mg.nGames;
            else if (b & PAD_DOWN)
                k = (k + 1) % mg.nGames;
            else if (b & PAD_LEFT)
                k = k >= MG_ROWS ? k - MG_ROWS : 0;
            else
                k = k + MG_ROWS < mg.nGames ? k + MG_ROWS : mg.nGames - 1;
            ui_lock();
            mg.cursor = k;
            if (k < mg.top)
                mg.top = k;
            if (k >= mg.top + MG_ROWS)
                mg.top = k - MG_ROWS + 1;
            ui_unlock();
            sound_play(SND_MOVE);
        } else if ((b & PAD_CROSS) && g) {
            ui_lock();
            g->on = !g->on;
            ui_unlock();
            sound_play(g->on ? SND_CONFIRM : SND_BACK);
        } else if ((b & PAD_TRIANGLE) && g && g->on && !g->card) {   /* a new card's size */
            int mb;
            sound_play(SND_CONFIRM);
            if ((mb = card_size_pick(T_SIZE_TITLE, g->room, 0, 0)) != 0)
                g->mb = mb;
            ui_scene(scene_migrate);
        } else if (b & PAD_START) {
            int k, going = 0;
            for (k = 0; k < mg.nGames; k++)
                going += mg.games[k].on;
            sound_play(going ? SND_CONFIRM : SND_BACK);
            if (going && mg_go())
                return;
            ui_scene(scene_migrate);
        }
    }
}
