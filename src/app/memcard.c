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
    snprintf(memImage.id, sizeof(memImage.id), "%s", MEMCARD_ID);
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

/* -------- "Restore backup" in its options: its backups on Drive, and one of them put back. The backup is a card
 * file: it is downloaded into the data folder, and its saves are copied into the Memory Card, each one replacing a
 * save of the same folder there; the Memory Card's other saves stay. The file goes once they are copied */

static card_t memRestore;
#ifdef DEBUG_BUILD
static int debugLocalRestore;   /* script "Y": the backup is the data folder's restore-test.mcd (PCSX2 has no account) */
#endif

void memcard_history(void)
{
    if (!memcard_there()) {
        message_wait(0, NULL, COLOR_WARN, T(T_MEMCARD_NONE));
        return;
    }
    memset(&memRestore, 0, sizeof(memRestore));
    snprintf(memRestore.base, sizeof(memRestore.base), "%s", T(T_MEMCARD));
    snprintf(memRestore.id, sizeof(memRestore.id), "%s", MEMCARD_ID);
    snprintf(memRestore.folder, sizeof(memRestore.folder), "%s", slotCard.folder);
    snprintf(memRestore.path, sizeof(memRestore.path), "%smemcard-restore%s", dataDir, dev->ext);
    memRestore.type = TYPE_NORMAL;
#ifdef DEBUG_BUILD
    if (debug_take('Y')) {
        drive_file_t f;
        memset(&f, 0, sizeof(f));
        snprintf(f.name, sizeof(f.name), "restore-test.mcd");
        debugLocalRestore = 1;
        memcard_restore(&memRestore, &f);
        debugLocalRestore = 0;
        return;
    }
#endif
    history_screen(&memRestore, NULL);
}

int memcard_restore(card_t *c, const drive_file_t *f)
{
    static mcfs_save_t list[MCFS_MAX_SAVES];
    static const char regions[] = "AEICK";
    char when[40], t[300], system[16];
    int n = 0, i, k, r, ok = 0, failed = 0;
    backup_when(c, f, when, sizeof(when));
    dlg_new(COLOR_TITLE, T(T_MEMCARD_RESTORE_ASK));
    snprintf(t, sizeof(t), T(T_RESTORE_FROM), when);
    dlg_line(FONT_TEXT, COLOR_ACCENT, 8, t);
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(T_MEMCARD_RESTORE_TEXT));
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, T_RESTORE);
    next.wide = 1;
    dlg_show();
    if (!(wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS)) {
        sound_play(SND_BACK);
        return 0;
    }
    sound_play(SND_CONFIRM);
    if (!memcard_there()) {   /* (taken out meanwhile) */
        message_wait(0, NULL, COLOR_WARN, T(T_MEMCARD_NONE));
        return 0;
    }
    unlink(c->path);   /* (one left by a run that didn't end) */
    restoreNew = 1;   /* (nothing is written over until the saves are copied: it can be given up on till then) */
    lastRestoreDraw = 0;
    lastRestorePhase = -1;
    card_work(&slotCard, 1, T(T_RESTORE_STEP_DOWNLOAD), 0);
#ifdef DEBUG_BUILD
    if (debugLocalRestore) {
        char from[200];
        snprintf(from, sizeof(from), "%srestore-test.mcd", dataDir);
        r = file_copy(from, c->path) == 0 ? 0 : -1;
    } else
#endif
        r = restore_card(c, f, restore_progress);
    if (r == 0 && (n = mcfs_list_saves(c->path, list, MCFS_MAX_SAVES, NULL)) < 0)
        r = -1;
    for (i = 0; r == 0 && i < n; i++) {   /* each save into the Memory Card, in place of one of the same folder */
        ui_lock();
        workText = T(T_RESTORE_STEP_WRITE);
        workFixed = 1;
        ui_unlock();
        upload_screen(i, n);
        k = mcfs_copy_save(c->path, list[i].folder, slotCard.path, NULL);
        if (k == MCFS_ERR_EXISTS && mcfs_delete_save(slotCard.path, list[i].folder) == MCFS_OK)
            k = mcfs_copy_save(c->path, list[i].folder, slotCard.path, NULL);
        log_msg("memory card restore: %s: %d", list[i].folder, k);
        if (k == MCFS_OK)
            ok++;
        else
            failed++;
    }
    for (i = 0; r == 0 && regions[i]; i++) {   /* (and its system data, when the backup has it) */
        snprintf(system, sizeof(system), "B%cDATA-SYSTEM", regions[i]);
        if (mcfs_copy_save(c->path, system, slotCard.path, NULL) == MCFS_ERR_EXISTS && mcfs_delete_save(slotCard.path, system) == MCFS_OK)
            mcfs_copy_save(c->path, system, slotCard.path, NULL);
    }
    card_work_done();
    restoreNew = 0;
    unlink(c->path);
    log_msg("memory card restored from %s: %d, %d save(s) in, %d not", f->name, r, ok, failed);
    if (r == -2)
        return 0;   /* (given up while downloading: the Memory Card didn't change) */
    dlg_new(r == 0 && !failed ? COLOR_OK : r == 0 ? COLOR_WARN : COLOR_ERROR, T(r == 0 ? T_MEMCARD_RESTORED : T_RESTORE_FAILED));
    if (r == 0) {
        snprintf(t, sizeof(t), T(T_RESTORE_FROM), when);
        dlg_line(FONT_TEXT, COLOR_ACCENT, 4, t);
        snprintf(t, sizeof(t), T(T_TPL_COPIED_N), ok);
        dlg_line(FONT_TEXT, COLOR_TEXT, 2, t);
        if (failed) {
            snprintf(t, sizeof(t), T(T_TPL_NOT_COPIED), failed);
            dlg_line(FONT_SMALL, COLOR_WARN, 0, t);
        }
    } else
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, googleError);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    sound_play(r == 0 ? SND_EXIT : SND_BACK);
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
    return r == 0;
}
