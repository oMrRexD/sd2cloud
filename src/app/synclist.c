/* SD2Cloud -- the cards of a sync, as a list: SELECT, while cards are sent, turns the screen of the card on its way
 * into the cards of the whole sync, one per row as the files of a folder are, with what became of each, and back. X
 * on a card still to be sent, or on the one on its way: leave it out of this sync, mark it as resolved (the automatic
 * sync takes it as synced as it is, until it changes again) or turn its sync off. Square leaves it out at once. */
#include "app.h"

#define SY_X     70
#define SY_RIGHT 570
#define SY_Y0    114
#define SY_ROW   26
#define SY_ROWS  9
#define SY_STATE 220   /* the room of what became of a card, on the right of its row */
#define ARROWS   (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)

sync_card_t syncCards[MAX_CARDS];
int nSync;
int syncListed;   /* the sync under way has the list: cards sent one after the other, more than one */
int syncView;     /* the list is the one on screen */
static struct {
    int cursor, top;
    int follow;    /* the cursor goes with the card on its way, until the user moves it */
    u32 down;      /* the buttons held at the last look: a press counts once */
    u64 repeat;    /* an arrow held: when it moves again */
} sy;

/* the cards a sync sends (is_selected), in its order: each still to be sent */
void sync_list_start(int mode)
{
    int i;
    ui_lock();
    for (nSync = i = 0; i < nCards; i++)
        if (is_selected(&cards[i], mode)) {
            syncCards[nSync].c = &cards[i];
            syncCards[nSync].taken = 0;
            syncCards[nSync++].state = SY_WAITING;
        }
    if (mode == 3 && !nSync && singleCard) {   /* (one that isn't a card of the device: the Memory Card, read whole) */
        syncCards[0].c = (card_t *)singleCard;
        syncCards[0].taken = 0;
        syncCards[nSync++].state = SY_WAITING;
    }
    syncListed = mode != 3 && nSync > 1;   /* (a card synced by itself has its own screen only) */
    syncView = 0;
    sy.cursor = sy.top = 0;
    sy.follow = 1;
    sy.down = pad_buttons();   /* (a button already held, as the X that started the sync, counts after it's released) */
    ui_unlock();
}

void sync_list_end(void)
{
    ui_lock();
    syncListed = syncView = 0;
    ui_unlock();
}

void sync_set(int k, int state)
{
    ui_lock();
    syncCards[k].state = state;
    ui_unlock();
}

/* (with ui_lock held) the cursor on card k, the rows scrolled to show it */
static void show(int k)
{
    sy.cursor = k;
    if (k < sy.top)
        sy.top = k;
    else if (k >= sy.top + SY_ROWS)
        sy.top = k - SY_ROWS + 1;
}

static int sending(void)
{
    int k;
    for (k = 0; k < nSync; k++)
        if (syncCards[k].state == SY_SENDING)
            return k;
    return -1;
}

/* X on the selected card: what can be done to it, while it's still to be sent or on its way. Returns what was asked
 * of the one on its way (as confirm_skip: 1 = left out of this sync, 2 = its sync turned off; and 3 = marked as
 * resolved), for the sync to stop sending it; of a card still to be sent, that is done when its turn comes */
static int options(void)
{
    sync_card_t *e = &syncCards[sy.cursor];
    const char *items[3] = {T(T_SKIP), T(T_MARK_RESOLVED), T(T_SYNC_DISABLE)};
    int k;
#ifdef DEBUG_BUILD
    int live = scriptLive;
#endif
    if (e->state != SY_WAITING && e->state != SY_SENDING)
        return 0;
    watchOff++;   /* (asked in the middle of the sync) */
    sound_play(SND_CONFIRM);
#ifdef DEBUG_BUILD
    scriptLive = 0;   /* (the list waits for its buttons as any other) */
#endif
    k = choose(e->c->base, items, 3, 0);
#ifdef DEBUG_BUILD
    scriptLive = live;
#endif
    watchOff--;
    circleDown = 1;   /* the circle that closed it, or X, counts again only after being released */
    sy.down = PAD_CROSS | PAD_CIRCLE;
    ui_scene(scene_sync);
    if (k < 0)
        return 0;
    log_msg("sync: %s %s", e->c->id, k == 0 ? "skipped" : k == 1 ? "marked as resolved" : "with its sync turned off");
    if (e->state == SY_SENDING)
        return k == 0 ? 1 : k == 1 ? 3 : 2;
    sync_set(sy.cursor, k == 0 ? SY_SKIPPED : k == 1 ? SY_RESOLVED : SY_OFF);
    return 0;
}

/* Square on the selected card: left out of this sync at once, without a question, so that skipping several is down,
 * square, down, square. Square again, before its turn comes, takes it back. Returns 1 for the card on its way */
static int skip(void)
{
    sync_card_t *e = &syncCards[sy.cursor];
    if (e->state == SY_SENDING) {
        sound_play(SND_CONFIRM);
        log_msg("sync: %s skipped (square)", e->c->id);
        return 1;
    }
    if (e->state == SY_WAITING || (e->state == SY_SKIPPED && !e->taken)) {
        sound_play(e->state == SY_WAITING ? SND_CONFIRM : SND_BACK);
        log_msg("sync: %s %s (square)", e->c->id, e->state == SY_WAITING ? "skipped" : "to be sent again");
        sync_set(sy.cursor, e->state == SY_WAITING ? SY_SKIPPED : SY_WAITING);
    }
    return 0;
}

/* From the sync's look at the buttons (stop_asked), many times a second while cards are sent: SELECT turns the list
 * on and off; on it, the arrows move, X offers what can be done to the card and square skips it. Returns what was
 * asked of the card on its way (options, skip) */
int sync_list_poll(void)
{
    u32 pad, press, arrow;
    int k, n;
    u64 now = now_ms();
    if (!syncListed)
        return 0;
    pad = pad_buttons();
    press = pad & ~sy.down;
    sy.down = pad;
    if (press & PAD_SELECT) {
        sound_play(SND_MOVE);
        ui_lock();
        syncView = !syncView;
        sy.follow = 1;
        if ((k = sending()) >= 0)
            show(k);
        ui_unlock();
        log_msg("sync: %s on screen", syncView ? "the list of its cards" : "the card on its way");
        ui_scene(syncView ? scene_sync : scene_upload);
        return 0;
    }
    if (!syncView)
        return 0;
    ui_lock();
    if (sy.follow && (k = sending()) >= 0)
        show(k);
    arrow = (press & ARROWS) ? press & ARROWS : (pad & ARROWS) && now >= sy.repeat ? pad & ARROWS : 0;
    if (arrow) {
        sy.repeat = now + ((press & ARROWS) ? 400 : 100);
        k = sy.cursor;
        n = nSync;
        if (arrow & PAD_UP)   /* (left and right: a page at a time) */
            k = k > 0 ? k - 1 : 0;
        else if (arrow & PAD_DOWN)
            k = k < n - 1 ? k + 1 : n - 1;
        else if (arrow & PAD_LEFT)
            k = k > SY_ROWS ? k - SY_ROWS : 0;
        else if (arrow & PAD_RIGHT)
            k = k + SY_ROWS < n ? k + SY_ROWS : n - 1;
        if (k != sy.cursor) {
            sy.follow = 0;
            show(k);
            sound_play(SND_MOVE);
        }
    }
    ui_unlock();
    return (press & PAD_CROSS) ? options() : (press & PAD_SQUARE) ? skip() : 0;
}

static const int stateText[] = {T_SY_WAITING, T_SY_SENDING, T_SY_SENT, T_SY_FAILED, T_SY_SKIPPED, T_SY_RESOLVED, T_SY_OFF};
static const u32 stateColor[] = {COLOR_DIM, COLOR_TEXT, COLOR_OK, COLOR_ERROR, COLOR_DIM, 0x6E9AE0, COLOR_DIM};

/* a card as its row names it: its channel's name, or its game's, or its file's */
static const char *row_name(const card_t *c)
{
    return c->name[0] ? c->name : c->game[0] ? c->game : c->base;
}

void scene_sync(float t)
{
    char s[120];
    int i, y, lh = ui_line_height(FONT_TEXT);
    look_space();   /* (the frame of the screen of the card on its way, which SELECT goes back to) */
    look_frame();
    ui_alpha(look_fade(t));
    look_title(SY_X, 82, T(igrMode ? T_IGR_TITLE : T_SYNC_TITLE), 0);
    snprintf(s, sizeof(s), T(T_SYNC_CARD_N), currentN, totalN);
    ui_text_right(FONT_TEXT, SY_RIGHT, 82, COLOR_TEXT, s);
    /* each card: its name, and on the right what became of it; the one on its way, a bar of how far it went */
    for (i = sy.top, y = SY_Y0; i < nSync && i < sy.top + SY_ROWS; i++, y += SY_ROW) {
        const sync_card_t *e = &syncCards[i];
        float my = y + lh / 2.0f + 1;
        int w = SY_RIGHT - SY_STATE - 16 - (SY_X + 28);
        ui_image(IMG_MINICARD, SY_X + 2, my - 8, 13, 15, 0xFFFFFF, e->state == SY_WAITING || e->state == SY_SENDING ? 0x80 : 0x48);
        snprintf(s, sizeof(s), "%s", row_name(e->c));
        fit(FONT_TEXT, s, sizeof(s), w);
        if (i == sy.cursor)
            look_item(SY_X + 28, y, s, 1, 0);
        else
            ui_text(FONT_TEXT, SY_X + 28, y, e->state == SY_WAITING || e->state == SY_SENDING ? COLOR_ITEM : COLOR_DIM, s);
        if (e->state == SY_SENDING) {
            long long done = shownDone, total = shownTotal;
            look_bar(SY_RIGHT - 120, my - 2, 120, total ? (int)(done * 1000 / total) : 0);
        } else
            ui_text_right(FONT_SMALL, SY_RIGHT, y + 2, stateColor[e->state], T(stateText[e->state]));
    }
    if (sy.top > 0)
        look_arrow(W / 2.0f, SY_Y0 - 12, 0, 0x6E9AE0);
    if (sy.top + SY_ROWS < nSync)
        look_arrow(W / 2.0f, SY_Y0 + SY_ROWS * SY_ROW + 2, 1, 0x6E9AE0);
    if (nSync) {   /* under the list: the selected card's file, and where the cursor is */
        const card_t *c = syncCards[sy.cursor].c;
        if (strcmp(row_name(c), c->base))
            ui_text_fit(FONT_SMALL, SY_X + 28, SY_Y0 + SY_ROWS * SY_ROW + 2, 300, 0x8E98AA, c->base);
        snprintf(s, sizeof(s), "%d/%d", sy.cursor + 1, nSync);
        ui_text_right(FONT_SMALL, SY_RIGHT, SY_Y0 + SY_ROWS * SY_ROW + 2, 0x8E98AA, s);
    }
    {
        /* only what can be done to the selected card is offered */
        const sync_card_t *e = nSync ? &syncCards[sy.cursor] : NULL;
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_CANCEL)}};
        int n = 1;
        if (e && (e->state == SY_WAITING || e->state == SY_SENDING)) {
            l[n].button = BUTTON_CROSS, l[n++].text = T(T_OPTIONS);
            l[n].button = BUTTON_SQUARE, l[n++].text = T(T_SKIP);
        } else if (e && e->state == SY_SKIPPED && !e->taken)
            l[n].button = BUTTON_SQUARE, l[n++].text = T(T_UNDO);
        l[n].button = BUTTON_SELECT, l[n++].text = T(T_VIEW);
        look_legend(l, n, 0);
    }
}
