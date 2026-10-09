/* SD2Cloud -- the card the sd2psx is using right now: which one it is, telling the sd2psx to take another, moving
 * it off a card before that card's file is changed, and noticing a change made on the device itself, or the device
 * taken out. */
#include "app.h"

/* is this card the one the sd2psx is emulating right now? 1 = yes, 0 = no, -1 = couldn't tell.
 * A CardN comes with its number, which the slot is checked against. Card number 0 is the BootCard, a game card or a
 * named folder, which the MMCE commands don't tell apart: then it compares the root folder the PS2 sees in that slot
 * with the one inside the .mcd (the same saves with the same modification times = the same card) */
int card_in_use(const card_t *c, int active, int channel)
{
    char seen[65], file[65];
    if (active == -2)
        return 0;   /* not on an MMCE device (testing on PCSX2) */
    if (!cardTold) {   /* a device that isn't asked which card it is on: only what the PS2 sees in the slot tells */
        if (mc_root_signature(sdRoot[4] - '0', seen) != 0 || mcfs_root_signature(c->path, file) != 0)
            return -1;
        return strcmp(seen, file) == 0;
    }
    if (active >= 1) {
        const card_t *told = NULL;
        int i;
        if (c->type == TYPE_NORMAL && card_number(c) == active && c->channel == channel)
            return 1;
        /* Another card than the one the device says it is on. That isn't taken on its word alone, as it may be the
         * number of the card it was on before (see find_active): unless the root folder the PS2 sees in the slot is
         * that numbered card's, this card is in use when that root folder is its own */
        if (mc_root_signature(sdRoot[4] - '0', seen) != 0)
            return 0;
        for (i = 0; i < nCards && !told; i++)
            if (cards[i].type == TYPE_NORMAL && card_number(&cards[i]) == active && cards[i].channel == channel)
                told = &cards[i];
        if (told && mcfs_root_signature(told->path, file) == 0 && !strcmp(seen, file))
            return 0;
        if (mcfs_root_signature(c->path, file) != 0 || strcmp(seen, file) != 0)
            return 0;
        log_msg("%s is in the slot, though the device says card %d, channel %d", c->id, active, channel);
        return 1;
    }
    if (active == 0 && (c->type == TYPE_NORMAL || c->channel != channel))
        return 0;
    if (mc_root_signature(sdRoot[4] - '0', seen) != 0 || mcfs_root_signature(c->path, file) != 0)
        return active == 0 ? 1 : -1;   /* couldn't compare: on a special card, assume the worst */
    log_msg("restore: root of the card in the slot %.16s, of %s %.16s", seen, c->id, file);
    return strcmp(seen, file) == 0;
}

/* -------- the card in the sd2psx: which one it is, and telling the sd2psx to take another.
 *
 * The sd2psx changes cards by itself, a moment after it is asked, and says nothing of when it is done: it only gives
 * its card's number and channel, which change as soon as it is asked. So a change is only taken as made when the PS2
 * itself sees it in the slot: mcman notices the card was changed, the sd2psx gives the number and channel asked for,
 * and the root folder in the slot is the one inside that card's .mcd. (Whether the sd2psx is still reading the card
 * into its memory doesn't matter, and isn't waited for: an 8 MB card takes it a long while, and it answers for the
 * card meanwhile) */

/* Which card that is (activeCard). A numbered card is found by its number. The BootCard, a game's card and a named folder all come as number 0: the
 * root folder the PS2 sees in the slot is compared with each candidate's, the boot cards first (the likeliest when
 * SD2Cloud was opened from OPL), since reading a card's root takes a moment */
/* the card the device says it is on. Only the sd2psx is asked: what the numbers mean to another device isn't known,
 * and there the slot tells (-1) */
int active_card(int *channel)
{
    return cardTold || strncmp(sdRoot, "mmce", 4) != 0 ? mmce_active_card(channel) : -1;
}

void find_active(void)
{
    static const int order[3] = {TYPE_BOOT, TYPE_GAMEID, TYPE_NAMED};
    char seen[65], file[65];
    int channel = 0, active = active_card(&channel), i, pass, found = -1;
    if (!cardTold) {   /* against each card's root folder as it was read when the cards were checked */
        if (active == -1 && mc_root_signature(sdRoot[4] - '0', seen) == 0)
            for (i = 0; i < nCards && found < 0; i++)
                if (cards[i].rootSig[0] && !strcmp(seen, cards[i].rootSig))
                    found = i;
    } else if (active >= 1) {
        for (i = 0; i < nCards && found < 0; i++)
            if (cards[i].type == TYPE_NORMAL && card_number(&cards[i]) == active && cards[i].channel == channel)
                found = i;
        /* What the device says is checked against what the PS2 sees in the slot: changed to a game's card with its
         * own buttons, an sd2psx went on giving the number of the card it had been on (seen on a console). When the
         * root folder in the slot isn't that card's and is another card's, that other one is the card in use */
        if (mc_root_signature(sdRoot[4] - '0', seen) == 0 &&
            (found < 0 || mcfs_root_signature(cards[found].path, file) != 0 || strcmp(seen, file) != 0))
            for (pass = 0; pass < 3; pass++)
                for (i = 0; i < nCards; i++)
                    if (cards[i].type == order[pass] && mcfs_root_signature(cards[i].path, file) == 0 && !strcmp(seen, file)) {
                        found = i;
                        pass = 3;
                        break;
                    }
    } else if (active == 0 && mc_root_signature(sdRoot[4] - '0', seen) == 0) {
        for (pass = 0; pass < 3 && found < 0; pass++)
            for (i = 0; i < nCards && found < 0; i++)
                if (cards[i].type == order[pass] && cards[i].channel == channel && mcfs_root_signature(cards[i].path, file) == 0 &&
                    !strcmp(seen, file))
                    found = i;
    }
#ifdef DEBUG_BUILD
    {   /* PCSX2 has no sd2psx: active.txt in the data folder names the card to show as the one in it */
        buffer_t b = {0};
        char c[260];
        snprintf(c, sizeof(c), "%sactive.txt", dataDir);
        if (active == -2 && file_read(c, &b) == 0 && b.len) {
            trim((char *)b.data);
            for (i = 0; i < nCards; i++)
                if (!strcasecmp(cards[i].id, (char *)b.data))
                    found = i;
        }
        buf_free(&b);
    }
#endif
    ui_lock();
    activeCard = found;
    ui_unlock();
    log_msg("%s: the card in it is %s", dev->name, found >= 0 ? cards[found].id : "none of the cards here, or unknown");
}

/* Is it sure which card the device is on? It is when find_active found one and the root folder the PS2 sees in the
 * slot (seen) is the one in that card's file: the device has nothing of it left to write, and a card whose file has
 * another root folder is not the one in the device. For what changes cards' files with nobody watching */
int active_sure(char seen[65])
{
    char file[65];
    if (activeCard < 0)
        return 0;
#ifdef DEBUG_BUILD
    if (strncmp(sdRoot, "mmce", 4) != 0)   /* PCSX2 has no device: the card active.txt names stands for the one in it */
        return mcfs_root_signature(cards[activeCard].path, seen) == 0;
#endif
    return mc_root_signature(mc_slot(), seen) == 0 && mcfs_root_signature(cards[activeCard].path, file) == 0 && !strcmp(seen, file);
}

/* can the device be told to take this card? Not a folder with a name of its own: it has no way to be asked for one.
 * The BootCard and a game's card the sd2psx only takes with Autoboot or Game ID on in its settings, which shows by
 * trying. On a device that isn't the sd2psx this is a preview: it takes the same requests, as far as is known */
int can_insert(const card_t *c) { return !strncmp(sdRoot, "mmce", 4) && c->type != TYPE_NAMED; }

/* and can it be moved off that card and back, for the card to be changed? Only the sd2psx: there it is known that
 * the card it left was written and closed by the time the next one shows in the slot */
static int can_move(const card_t *c) { return dev->sd2psx && can_insert(c); }

/* from here on, a change of card that mcman notices in the slot is the one about to be asked for */
static void slot_settle(void)
{
    int i;
    for (i = 0; i < 6 && mc_card_state(sdRoot[4] - '0') != 0; i++)
        sleep_ms(200);
}

/* Waits for a card to be in the slot (see above). c = that card; NULL = a BootCard, whichever channel the sd2psx
 * keeps for it. Returns the card's index in cards[], or -1 when the time is up; with no change seen in the slot a
 * few seconds after asking, the sd2psx isn't going to make one */
static int wait_for_card(const card_t *c, int ms)
{
    char seen[65], file[65];
    int port = sdRoot[4] - '0', changed = 0, channel = 0, active = 0, i;
    u64 start = now_ms(), end = start + ms;
    while (now_ms() < end && (changed || now_ms() < start + 6000)) {
        sleep_ms(300);
        if (mc_card_state(port) != 0) {
            changed = 1;
            continue;
        }
        if (!changed)
            continue;
        if (cardTold || !c) {   /* (a device whose numbers aren't known: the root folder in the slot alone tells) */
            channel = 0;
            active = mmce_active_card(&channel);
            if (active < 0 || (c && (active != (c->type == TYPE_NORMAL ? card_number(c) : 0) || channel != c->channel)))
                continue;
        }
        if (mc_root_signature(port, seen) != 0)
            continue;
        for (i = 0; i < nCards; i++) {
            const card_t *k = &cards[i];
            if (c ? k != c : (active != 0 || k->type != TYPE_BOOT || k->channel != channel))
                continue;
            if (mcfs_root_signature(k->path, file) == 0 && !strcmp(seen, file)) {
                log_msg("sd2psx: %s is in the slot", k->id);
                return i;
            }
        }
    }
    log_msg("sd2psx: %s didn't show in the slot (a change was %sseen)", c ? c->id : "the BootCard", changed ? "" : "not ");
    return -1;
}

/* another channel of the card the sd2psx is on. It may be in the middle of changing cards and not answer: asked again
 * for a while. 0 = it took the request */
static int ask_channel(int channel)
{
    int i;
    for (i = 0; i < 40; i++) {
        if (mmce_set_channel(channel) == 0)
            return 0;
        sleep_ms(300);
    }
    return -1;
}

/* the first channel of a card's folder, when it is one of the cards here */
static const card_t *first_channel(const card_t *c)
{
    int i;
    for (i = 0; i < nCards; i++)
        if (cards[i].type == c->type && cards[i].channel == 1 && !strcmp(cards[i].folder, c->folder))
            return &cards[i];
    return NULL;
}

/* Tells the sd2psx to take that card and waits for it to be in the slot. 0 = it is. The marker of the card in use
 * follows whatever the sd2psx ended on.
 * The sd2psx is asked for a card (which comes in a channel of its choosing: the first one, or for the BootCard the
 * one it keeps for it) and, apart from that, for a channel of the card it is on. So: a card of the folder it is on
 * already, only the channel (asked for its own card's number again, it would say the first channel without changing
 * to it); else the card, and the channel only once that card is in the slot (a sd2psx with Autoboot off never goes
 * to the BootCard: the channel would be taken of whatever card it is on). The card's first channel has to be one
 * of the cards here, as the sd2psx creates a card it is asked for and doesn't find */
int insert_card(const card_t *c)
{
    int number = c->type == TYPE_NORMAL ? card_number(c) : 0, channel = 0, active, now = -1, r = -1;
    const card_t *first;
    if (!can_insert(c))
        return -1;
    active = mmce_active_card(&channel);
    find_active();
    if (activeCard >= 0 && &cards[activeCard] == c)
        return 0;
    slot_settle();
    log_msg("sd2psx: asking for %s", c->id);
    /* (the folder it is on: the one of the card found in the slot, when one was, as the device's own number may be
     * of the card it was on before; else that number) */
    if (activeCard >= 0 ? cards[activeCard].type == c->type && !strcmp(cards[activeCard].folder, c->folder)
                        : cardTold && c->type == TYPE_NORMAL && active == number)
        now = activeCard >= 0 ? activeCard : nCards;   /* on that folder already (nCards: on a channel that isn't here) */
    else if (c->type == TYPE_BOOT) {
        if (mmce_set_card(1, 0) == 0)
            now = wait_for_card(NULL, 25000);
    } else if ((first = first_channel(c)) != NULL) {
        if ((c->type == TYPE_NORMAL ? mmce_set_card(0, number) : mmce_set_gameid(c->folder)) == 0)
            now = wait_for_card(first, 25000);
    }
    if (now >= 0 && now < nCards && &cards[now] == c)
        r = 0;
    else if (now >= 0) {
        slot_settle();
        if (ask_channel(c->channel) == 0 && wait_for_card(c, 25000) >= 0)
            r = 0;
    }
    if (!cardTold)
        mmce_active_card(&channel);   /* (for the log: what that device calls the card it ended on) */
    find_active();
    watchNext = now_ms() + 10000;   /* it may still be busy with the change: not asked whether it is there for a while */
    return r;
}

/* -------- changing a card the sd2psx is using. The sd2psx keeps that card in its own memory and writes it back by
 * itself, so its .mcd is never changed under it: the sd2psx is moved to another card first (which makes it write
 * this one and close it), the change is made, and it is moved back. The user is told before any of it. */

const card_t *movedOff;   /* the card the sd2psx was moved off of, to go back to */

/* Waits for the card in the slot to be another one than it was: mcman has noticed a change of card, a card is there
 * again and its root folder isn't the one from before. Which card it is doesn't matter here, only that the one from
 * before was let go of. 0 = it is another card */
static int wait_for_change(const char *before, int ms)
{
    char seen[65];
    int port = sdRoot[4] - '0', changed = 0;
    u64 start = now_ms(), end = start + ms;
    while (now_ms() < end && (changed || now_ms() < start + 6000)) {
        sleep_ms(300);
        if (mc_card_state(port) != 0)
            changed = 1;
        else if (changed && mc_root_signature(port, seen) == 0 && strcmp(seen, before) != 0)
            return 0;
    }
    log_msg("sd2psx: the card in the slot is still the same (a change was %sseen)", changed ? "" : "not ");
    return -1;
}

/* Moves the sd2psx off a card: to the BootCard when it goes to it (nothing in its settings changes with that), else
 * to a numbered card that is no part of what is being done, the lowest first. 0 = it is on another card now */
static int leave_card(const card_t *c, const card_t *other)
{
    char before[65];
    int i, k, n = 0, order[MAX_CARDS];
    if (c->type != TYPE_BOOT && !(other && other->type == TYPE_BOOT) && mc_root_signature(sdRoot[4] - '0', before) == 0) {
        slot_settle();
        i = mmce_set_card(1, 0) == 0 ? wait_for_change(before, 40000) : -1;
        find_active();
        if (i == 0)
            return 0;
    }
    for (i = 0; i < nCards; i++)   /* the numbered cards, by number and channel */
        if (cards[i].type == TYPE_NORMAL && &cards[i] != c && &cards[i] != other) {
            for (k = n++; k > 0 && (card_number(&cards[order[k - 1]]) > card_number(&cards[i]) ||
                                    (card_number(&cards[order[k - 1]]) == card_number(&cards[i]) &&
                                     cards[order[k - 1]].channel > cards[i].channel)); k--)
                order[k] = order[k - 1];
            order[k] = i;
        }
    for (k = 0; k < n && k < 2; k++)   /* (one that fails takes its time: when two do, the others would too) */
        if (insert_card(&cards[order[k]]) == 0)
            return 0;
    return -1;
}

/* 0 = the card's .mcd can be changed now: the sd2psx isn't using it, or was moved off it (card_back when done).
 * 1 = it can't, and the user was told why, or was asked and didn't want the sd2psx moved. other = the other card
 * of what is being done, if there is one */
int card_free(const card_t *c, const card_t *other)
{
    char t[400];
    int channel = 0, active = active_card(&channel), r = card_in_use(c, active, channel);
    if (r == 0)
        return 0;
    if (r < 0 || !can_move(c)) {   /* which card it has isn't known, or it's one it can't be told to come back to */
        snprintf(t, sizeof(t), T(r > 0 ? T_CARD_IN_USE : T_RESTORE_UNKNOWN), c->base);
        message_wait(0, NULL, COLOR_WARN, t);
        return 1;
    }
    snprintf(t, sizeof(t), T(T_SWITCH_ASK), c->base);
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, t);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_CONTINUE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 1;
    }
    sound_play(SND_CONFIRM);
    message(0, NULL, COLOR_TEXT, T(T_SWITCHING));
    r = leave_card(c, other);
    channel = 0;
    active = mmce_active_card(&channel);
    if (r != 0 || card_in_use(c, active, channel) != 0) {   /* not moved, or not for sure: nothing is changed */
        log_msg("sd2psx: couldn't be moved off %s", c->id);
        insert_card(c);
        message_wait(0, NULL, COLOR_WARN, T(T_SWITCH_FAILED));
        return 1;
    }
    movedOff = c;
    return 0;
}

/* moves the sd2psx back to the card it was moved off of (nothing to do when it wasn't) */
void card_back(void)
{
    char t[300];
    const card_t *c = movedOff;
    if (!c)
        return;
    movedOff = NULL;
    message(0, NULL, COLOR_TEXT, T(T_SWITCHING));
    if (insert_card(c) != 0) {
        snprintf(t, sizeof(t), T(T_SWITCH_BACK_FAILED), c->base);
        message_wait(0, NULL, COLOR_WARN, t);
    }
}

/* the device didn't take that card: said, with what the sd2psx needs for that kind of card */
void insert_failed(const card_t *c)
{
    char t[300];
    snprintf(t, sizeof(t), T(T_INSERT_FAILED), c->base);
    dlg_new(0, NULL);
    dlg_line(FONT_TEXT, COLOR_WARN, 8, t);
    if (c->type == TYPE_BOOT || c->type == TYPE_GAMEID)
        dlg_line(FONT_SMALL, COLOR_DIM, 0, T(c->type == TYPE_BOOT ? T_INSERT_NEEDS_BOOT : T_INSERT_NEEDS_GAMEID));
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* a card's "Insert into sd2psx": the sd2psx takes that card, as if picked with its own buttons */
void insert_option(const card_t *c)
{
    char t[300];
    /* the sd2psx starts from the BootCard it used last */
    if (c->type == TYPE_BOOT && !confirm(c->base, T(T_INSERT_BOOT_ASK), T_INSERT_YES))
        return;
    message(0, NULL, COLOR_TEXT, T(T_SWITCHING));
    if (insert_card(c) == 0) {
        snprintf(t, sizeof(t), T(T_INSERT_DONE), c->base);
        message_wait(0, NULL, COLOR_OK, t);
        return;
    }
    insert_failed(c);
}

/* -------- the device taken out of the console while SD2Cloud is open (to change its microSD, say). While a screen
 * waits for the controller the device is asked, every couple of seconds, whether it is there. When it stops answering
 * that is said on screen until it answers again; then everything that came from its microSD is read again from the
 * start, as it may be another microSD: the settings, the account, the history and the cards. */

jmp_buf reloadPoint;   /* in main(), right before the main screen is set up */

#ifdef DEBUG_BUILD
u64 debugGone;   /* script letter G: the device "doesn't answer" until then (PCSX2 has none to take out) */
#endif

static int device_answers(void)
{
#ifdef DEBUG_BUILD
    if (debugGone)
        return now_ms() >= debugGone;
#endif
    return mmce_ping() >= 0;
}

/* The card in the device may be changed with the device's own buttons while SD2Cloud is open. Each time the device
 * is asked whether it is there, it is also asked which card it is on (what the sd2psx tells: a number and a
 * channel), and mcman whether the card in the slot is another. A change is only looked into the next time around,
 * with nothing changing in between: the device takes a moment to have the new card ready. Then the card in use is
 * found again, and what the settings say of the SAS package, which is of the card in use, is read again when they
 * are next opened (helperStale) */
static void active_watch(void)
{
    static int seenCard = -3, seenChannel, seenSlot, pending;
    /* mcman: -1 = another card since it was last asked (said once); 0 = the same one; anything else is how the slot
     * is for now (no card that can be used), which only counts when it becomes so */
    int channel = 0, card = cardTold ? mmce_card_now(&channel) : -1, state = mc_card_state(sdRoot[4] - '0'),
        slot = state == -1 ? 0 : state;
    if (seenCard == -3) {   /* the first time: what was found when SD2Cloud opened stands */
        seenCard = card;
        seenChannel = channel;
        seenSlot = slot;
        return;
    }
    if (card != seenCard || channel != seenChannel || slot != seenSlot || state == -1) {
        seenCard = card;
        seenChannel = channel;
        seenSlot = slot;
        pending = 1;
        return;
    }
    if (pending) {
        pending = 0;
        log_msg("%s: another card in it (%d, channel %d)", dev->name, card, channel);
        find_active();
        helperStale = 1;
    }
}

void device_watch(void)
{
    int i;
    /* (a device that never answered the ping, as a MemCard PRO2 may not, would look taken out all the time) */
    if (watchOff || !devicePings || !dev->sd2psx || strncmp(sdRoot, "mmce", 4) != 0 || now_ms() < watchNext)
        return;
    watchNext = now_ms() + 2000;
    for (i = 0; i < 3; i++) {   /* three times in a row: it may only be busy (changing cards) */
        static int said;
        int r = mmce_ping();
        if (r >= 0) {
            if (!said++)
                log_msg("%s: watched while the screens wait (it answers %d)", dev->name, r);
            active_watch();
            return;
        }
        sleep_ms(500);
    }
    device_lost();
}

void device_lost(void)
{
    DIR *d = NULL;
    int i;
    watchOff++;
    log_msg("%s: it doesn't answer (taken out?)", dev->name);
    sound_play(SND_BACK);
    dlg_new(COLOR_WARN, T(T_DEVICE_GONE));
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_DEVICE_GONE_TEXT));
    dlg_buttons(BUTTON_CIRCLE, T_EXIT_BROWSER, 0, 0);
    dlg_show();
    while (!device_answers()) {
        if (pad_buttons() & PAD_CIRCLE)   /* without the microSD the PS2 browser is the only place left to go */
            leave("osd");
#ifdef DEBUG_BUILD
        debug_capture_if('g');
#endif
        sleep_ms(400);
    }
#ifdef DEBUG_BUILD
    debugGone = 0;
#endif
    log_msg("%s: it answers again", dev->name);
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    for (i = 0; i < 60 && !(d = opendir(sdRoot)); i++)   /* its microSD takes a moment */
        sleep_ms(500);
    if (d)
        closedir(d);
    log_msg("the microSD %s after %d ms", d ? "answers" : "still doesn't answer", i * 500);
    /* what was open of the microSD that was there */
    browser_close();
    psu_close();
    card_file_close(&cardFile);
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    fbCard = NULL;
    fbGive.c = NULL;
    fbExit = 0;
    fbPick = 0;
    movedOff = NULL;
    transferDest = NULL;
    appsListed = 0;
    activeCard = -1;
    cancelLatched = 0;
    watchOff = 0;
    longjmp(reloadPoint, 1);
}
