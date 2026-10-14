/* SD2Cloud -- a card as a whole, from its options (△ on the main screen): deleted, or its whole folder deleted. */
#include "app.h"

/* ------------------------------------------------------------ deleting */

/* After cards went out of cards[], the ones after them moved up: the card in use is found again by its name (gone
 * with them: none), and nothing else holds on to one. The main screen puts its list together again */
static void cards_gone(const char *active)
{
    for (activeCard = nCards - 1; activeCard >= 0 && strcmp(cards[activeCard].id, active); activeCard--)
        ;
    movedOff = NULL;   /* (the device stays where it was moved: the card it was on is no more) */
    transferDest = NULL;
    if (marks.n)
        marks_reset();
    cardsUnsorted = 1;
}

/* what is said under the question of a card that goes: when it was last backed up, or that it never was */
static void backup_line(const card_t *c)
{
    char when[40], t[200];
    last_backup(c, when, sizeof(when));
    if (when[0]) {
        snprintf(t, sizeof(t), T(T_DELETE_BACKUP), when);
        dlg_line(FONT_SMALL, COLOR_DIM, 0, t);
    } else
        dlg_line(FONT_SMALL, COLOR_WARN, 0, T(T_DELETE_NO_BACKUP));
}

/* the question made so far (dlg_new...), with its buttons: 1 = delete */
static int ask_delete(void)
{
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_DELETE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    return 1;
}

/* The card's file off the microSD, asked first. The card the device is using is let go of first (it is moved to
 * another one, which it stays on). When it was its folder's last card, the folder goes too, with its .ini. 1 = it
 * went (cards[] changed: the main screen lists them again) */
int card_delete(card_t *c)
{
    char t[300], active[96], folder[48];
    int i, others = 0, r;
    snprintf(t, sizeof(t), T(T_DELETE_CARD_ASK), c->base);
    dlg_new(COLOR_WARN, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, T(T_DELETE_CARD_TEXT));
    backup_line(c);
    if (!ask_delete() || card_free(c, NULL))
        return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    snprintf(folder, sizeof(folder), "%s", c->folder);
    for (i = 0; i < nCards; i++)
        others += &cards[i] != c && !strcasecmp(cards[i].folder, folder);
    r = cards_delete(c);
    if (r == 0 && !others)   /* (nothing of it is left to keep: its .ini, its pictures, the folder itself) */
        cards_delete_folder(folder);
    cards_gone(active);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_DONE_DELETE_CARD : T_DELETE_CARD_FAILED));
    return 1;
}

/* the whole folder of that card, with every card of it, asked first. 1 = something went (as card_delete) */
int folder_delete(card_t *c)
{
    char t[300], active[96], folder[48];
    card_t *own[MAX_CARDS];
    int i, n = 0, never = 0, r;
    snprintf(folder, sizeof(folder), "%s", c->folder);
    for (i = 0; i < nCards; i++)
        if (!strcasecmp(cards[i].folder, folder)) {
            card_state_t *e = state_card(cards[i].id, 0);
            own[n++] = &cards[i];
            never += !e || !e->sha[0];
        }
    snprintf(t, sizeof(t), T(T_DELETE_FOLDER_ASK), folder);
    dlg_new(COLOR_WARN, t);
    snprintf(t, sizeof(t), T(T_DELETE_FOLDER_TEXT), n);
    dlg_line(FONT_TEXT, COLOR_TEXT, 8, t);
    if (never) {
        snprintf(t, sizeof(t), T(T_DELETE_FOLDER_NO_BACKUP), never);
        dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
    } else
        dlg_line(FONT_SMALL, COLOR_DIM, 0, T(T_DELETE_FOLDER_BACKUPS));
    if (!ask_delete())
        return 0;
    for (i = 0; i < n; i++)   /* (the device is on one of them, at most: that one is let go of) */
        if (card_free(own[i], NULL))
            return 0;
    message(0, NULL, COLOR_TEXT, T(T_DELETING));
    snprintf(active, sizeof(active), "%s", activeCard >= 0 ? cards[activeCard].id : "");
    r = cards_delete_folder(folder);
    cards_gone(active);
    message_wait(0, NULL, r == 0 ? COLOR_OK : COLOR_ERROR, T(r == 0 ? T_DONE_DELETE_FOLDER : T_DELETE_FOLDER_FAILED));
    return 1;
}
