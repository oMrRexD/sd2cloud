/* SD2Cloud -- the Memory Card: a PS2 memory card in the console's other slot (the one the device isn't in), offered in
 * the Files group. Its saves are shown, copied, moved, deleted and sent to Drive as a card's are, and saves go into
 * it from the device's cards or from .psu files: the engine (mcfs) takes it as "mc0:" or "mc1:", read and written
 * through mcman's own calls (helper.c). */
#include <fcntl.h>
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

/* -------- "Upload to Drive" in its options: the whole card, as a backup of its own on Drive, with its history (in the
 * folder its saves are sent to one by one), the way a device's card is sent. What is sent is a card file made for it
 * on the microSD, an empty card with the Memory Card's saves copied into it, deleted once it is sent */

#define IMG_MAKING (1024 * 1024)   /* the empty card's making, on the bar: as that many bytes of saves */
static card_t memImage;
static long long imgDone, imgTotal, imgSave;   /* the bar: what is read, all there is, the save being read */

static void image_made(long long done, long long total) { upload_screen(total ? IMG_MAKING * done / total : 0, imgTotal); }

static int image_step(int phase, long long done, long long total)
{
    static const int start[] = {0, 150, 800}, span[] = {150, 650, 200};
    watch_cancel();
    if (cancelLatched && confirm_cancel(T_WORK_CANCEL_TITLE, T_RESTORE_CANCEL_TEXT))
        return 1;
    if (done > total)
        done = total;
    upload_screen(imgDone + imgSave * (start[phase] + (total ? span[phase] * done / total : 0)) / 1000, imgTotal);
    return 0;
}

void memcard_upload(void)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    static const char regions[] = "AEICK";   /* (the system data, B?DATA-SYSTEM: the console's region) */
    char t[300], system[16];
    long long freeBytes = -1, bytes;
    int n, i, k, files, r = MCFS_OK, fd;
    if (!memcard_there()) {
        message_wait(0, NULL, COLOR_WARN, T(T_MEMCARD_NONE));
        return;
    }
    snprintf(t, sizeof(t), T(T_CONFIRM_CLOUD), slotCard.base);
    if (!confirm(t, T(T_CONFIRM_BACKUP_TEXT), T_TO_CLOUD))
        return;
    googleError[0] = 0;
#ifdef DEBUG_BUILD
    debugDry = debug_take('B');   /* (script "B": it is read, and sent as a dry sync sends: nothing goes) */
    if (!debugDry && (r = ensure_google(1)) != 0) {
#else
    if ((r = ensure_google(1)) != 0) {   /* (asked first: the card is read only to be sent) */
#endif
        if (r > 0) {
            snprintf(t, sizeof(t), "%s %s", T(r), googleError);
            message_wait(0, NULL, COLOR_ERROR, t);
        }
        return;
    }
    message(0, NULL, COLOR_TEXT, T(T_LOADING));
    if ((n = mcfs_list_saves(slotCard.path, list, MCFS_MAX_SAVES, &freeBytes)) < 0)
        r = MCFS_ERR_IO;
    memset(&memImage, 0, sizeof(memImage));
    snprintf(memImage.base, sizeof(memImage.base), "%s", T(T_MEMCARD));
    snprintf(memImage.id, sizeof(memImage.id), "slot/memcard");
    snprintf(memImage.folder, sizeof(memImage.folder), "%s", slotCard.folder);
    snprintf(memImage.path, sizeof(memImage.path), "%smemcard-upload%s", dataDir, dev->ext);
    memImage.type = TYPE_NORMAL;
    memImage.included = 1;
    for (imgTotal = IMG_MAKING, i = 0; i < n; i++) {
        bytes = files = 0;
        mcfs_save_info(slotCard.path, list[i].folder, &bytes, &files);
        imgTotal += bytes;
    }
    if (r == MCFS_OK) {
        card_work(&slotCard, 0, T(T_MEMCARD_READING), 0);
        cancelLatched = 0;
        unlink(memImage.path);   /* (one left by a run that didn't end) */
        if (mcfs_new_card(memImage.path, MCFS_CARD_8MB, image_made) != MCFS_OK)
            r = MCFS_ERR_IO;
        for (imgDone = IMG_MAKING, i = 0; i < n && r == MCFS_OK; i++) {
            bytes = files = 0;
            mcfs_save_info(slotCard.path, list[i].folder, &bytes, &files);
            imgSave = bytes;
            r = mcfs_copy_save(slotCard.path, list[i].folder, memImage.path, image_step);
            log_msg("memory card: %s read for Drive: %d", list[i].folder, r);
            imgDone += bytes;
        }
        for (i = 0; regions[i] && r == MCFS_OK; i++) {   /* (the system data too, the card as it is: there is one) */
            snprintf(system, sizeof(system), "B%cDATA-SYSTEM", regions[i]);
            imgSave = 0;
            if ((k = mcfs_copy_save(slotCard.path, system, memImage.path, image_step)) == MCFS_OK)
                log_msg("memory card: %s read for Drive", system);
            else if (k == MCFS_ERR_CANCELLED)
                r = k;
        }
        card_work_done();
    }
    if (r == MCFS_OK && (fd = open(memImage.path, O_RDONLY)) >= 0) {
        memImage.size = lseek(fd, 0, SEEK_END);
        close(fd);
    }
    log_msg("memory card: %d save(s) in %s (%lld bytes): %d", n, memImage.path, memImage.size, r);
    if (r == MCFS_OK && memImage.size > 0) {
        cards_recheck(&memImage);
        singleCard = &memImage;
        manual_backup(3);
        singleCard = NULL;
    } else if (r != MCFS_ERR_CANCELLED)
        message_wait(0, NULL, COLOR_ERROR, T(T_MEMCARD_READ_FAILED));
    unlink(memImage.path);
#ifdef DEBUG_BUILD
    debugDry = 0;
#endif
}
