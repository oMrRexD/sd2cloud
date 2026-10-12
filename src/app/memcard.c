/* SD2Cloud -- the Memory Card: a PS2 memory card in the console's other slot (the one the device isn't in), offered in
 * the Files group. Its saves are shown, copied, moved, deleted and sent to Drive as a card's are, and saves go into
 * it from the device's cards or from .psu files: the engine (mcfs) takes it as "mc0:" or "mc1:", read and written
 * through mcman's own calls (helper.c). */
#include "app.h"

card_t slotCard;

/* Is there one (a formatted PS2 memory card in the other slot)? 1 = yes, and slotCard is it. Asked each time it may be
 * offered: a card is put in and taken out while the program is open */
int memcard_there(void)
{
    int port = mc_other_slot();
    if (port < 0)
        return 0;
    memset(&slotCard, 0, sizeof(slotCard));
    snprintf(slotCard.base, sizeof(slotCard.base), "%s", T(T_MEMCARD));
    snprintf(slotCard.id, sizeof(slotCard.id), "slot/mc%d", port);
    snprintf(slotCard.path, sizeof(slotCard.path), "mc%d:", port);
    snprintf(slotCard.folder, sizeof(slotCard.folder), "Memory Card");   /* (on Drive, its saves sent one by one) */
    slotCard.type = TYPE_NORMAL;
    return 1;
}

/* X on it in the Files group: its saves */
void memcard_screen(void)
{
    if (!memcard_there()) {   /* (taken out since the list was shown) */
        message_wait(0, NULL, COLOR_WARN, T(T_MEMCARD_NONE));
        return;
    }
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    card_screen(&slotCard);
}

/* what the Files group says of it, on the right (the main screen's and the list of cards a copy marks saves on) */
void memcard_hint(void)
{
    ui_text_center(FONT_TEXT, CARD_CX, 82, 0x7E8AA0, T(T_MEMCARD));
    ui_paragraph_center(FONT_SMALL, CARD_CX, CARD_Y + 70, LOOK_CARD_W + 32, COLOR_DIM, T(T_MEMCARD_HINT));
}
