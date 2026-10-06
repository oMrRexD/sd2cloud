/*
 * SD2Cloud -- installs the IGR helper (SD2CLOUD-IGR.ELF) on the memory card in use, and removes it.
 *
 * OPL's IGR only runs the "Exit to" ELF (exit_path) from the memory card or USB: it only loads rom0:SIO2MAN and
 * rom0:MCMAN. With "MMCE IGR slot" on, OPL switches to the BootCard first, so the helper has to be in BOOT/ of the boot
 * card. The helper only loads the mmceman and runs SD2Cloud from the microSD with -igr.
 *
 * The helper finds SD2Cloud by the path SD2Cloud keeps in its own settings (sd2cloud.ini, "app_path"), so nothing
 * but the helper itself goes to the memory card.
 *
 * It writes through mcman (the same path games use to save), only after the user confirms, and reads it back.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libmc.h>
#include <libpad.h>
#include <ps2_memcard_driver.h>
#include "common.h"

#define TARGET_DIR "/BOOT"
#define TARGET     HELPER_TARGET

static int mc_wait(void)
{
    int r;
    mcSync(0, NULL, &r);
    return r;
}

/* mcman's open modes (Sony's; newlib's O_* don't match) */
#define MC_RDONLY 0x0001
#define MC_WRONLY 0x0002
#define MC_CREAT  0x0200

/* a file that couldn't be written whole is deleted: OPL would try to run a broken ELF at IGR */
static void remove_partial(const char *path)
{
    mcDelete(0, 0, path);
    log_msg("helper: incomplete %s deleted (%d)", path, mc_wait());
}

static int mc_write_file(const char *path, const unsigned char *d, int n)
{
    int fd, done = 0, r;
    static unsigned char back[64 * 1024] __attribute__((aligned(64)));
    mcMkDir(0, 0, TARGET_DIR);
    log_msg("helper: mkdir %d (fine if it exists)", mc_wait());
    mcDelete(0, 0, path);   /* mcman doesn't truncate: delete the old one first */
    log_msg("helper: delete %s %d (fine if it didn't exist)", path, mc_wait());
    mcOpen(0, 0, path, MC_WRONLY | MC_CREAT);
    fd = mc_wait();
    log_msg("helper: open for writing %d", fd);
    if (fd < 0)
        return -1;
    while (done < n) {
        int k = n - done > 16384 ? 16384 : n - done;
        mcWrite(fd, d + done, k);
        if ((r = mc_wait()) != k) {
            log_msg("helper: write at %d returned %d", done, r);
            mcClose(fd);
            mc_wait();
            remove_partial(path);
            return -1;
        }
        done += k;
    }
    mcClose(fd);
    if ((r = mc_wait()) < 0) {
        log_msg("helper: close %d", r);
        remove_partial(path);
        return -1;
    }
    /* read it back and compare */
    mcOpen(0, 0, path, MC_RDONLY);
    fd = mc_wait();
    log_msg("helper: open for verifying %d", fd);
    if (fd < 0)
        return -1;
    for (done = 0; done < n;) {
        int k = n - done > (int)sizeof(back) ? (int)sizeof(back) : n - done;
        mcRead(fd, back, k);
        if (mc_wait() != k || memcmp(back, d + done, k) != 0) {
            mcClose(fd);
            mc_wait();
            remove_partial(path);
            return -1;
        }
        done += k;
    }
    mcClose(fd);
    mc_wait();
    return 0;
}

/* loads mcman/mcserv, once. 0 = they are there */
static int memcard_start(void)
{
    static int started;
    int r;
    if (!started) {
        /* ps2_drivers only loads mcman and mcserv (returns 1); mcInit is up to us */
        r = init_memcard_driver(false);
        log_msg("helper: init_memcard_driver %d", r);
        if (r < MEMCARD_INIT_STATUS_OK || (r = mcInit(MC_TYPE_XMC)) < 0) {
            log_msg("helper: mcInit %d", r);
            return -1;
        }
        started = 1;
    }
    return 0;
}

/* checks there is a formatted PS2 card in that slot (0 = slot 1). 0 = ready */
static int memcard_ready(int port)
{
    int r, type = 0, freeKb = 0, format = 0;
    if (memcard_start() != 0)
        return -1;
    mcGetInfo(port, 0, &type, &freeKb, &format);
    r = mc_wait();
    log_msg("helper: getinfo %d (type %d, %d KB free, formatted %d)", r, type, freeKb, format);
    return (type == 2 && format) ? 0 : -1;   /* 2 = PS2 card */
}

/* what mcman has to say of the card in that slot since the last time it was asked: 0 = it is the same card; anything
 * else = it was changed for another, or there is none to use right now (the sd2psx, while it changes cards) */
int mc_card_state(int port)
{
    int type = 0, freeKb = 0, format = 0;
    if (memcard_start() != 0)
        return -100;
    mcGetInfo(port, 0, &type, &freeKb, &format);
    return mc_wait();
}

/* is the helper on the memory card in use the same as the one in the app's folder? (after SD2Cloud is updated, the
 * one on the memory card stays old until someone reinstalls it: the settings show it) */
int helper_status(void)
{
    static unsigned char block[16 * 1024] __attribute__((aligned(64)));
    char source[260];
    buffer_t elf = {0};
    int fd, done = 0, n, r = HELPER_SAME;
    snprintf(source, sizeof(source), "%sSD2CLOUD-IGR.ELF", appDir);
    if (appElsewhere || file_read(source, &elf) != 0 || elf.len < 1024) {
        buf_free(&elf);
        return HELPER_NO_FILE;
    }
    if (memcard_ready(0) != 0) {
        buf_free(&elf);
        return HELPER_NOT_INSTALLED;
    }
    mcOpen(0, 0, TARGET, MC_RDONLY);
    if ((fd = mc_wait()) < 0) {
        buf_free(&elf);
        return HELPER_NOT_INSTALLED;
    }
    for (;;) {
        mcRead(fd, block, sizeof(block));
        if ((n = mc_wait()) <= 0)
            break;
        if (done + n > (int)elf.len || memcmp(block, elf.data + done, n) != 0) {
            r = HELPER_DIFFERENT;
            break;
        }
        done += n;
    }
    if (r == HELPER_SAME && done != (int)elf.len)
        r = HELPER_DIFFERENT;
    mcClose(fd);
    mc_wait();
    buf_free(&elf);
    log_msg("helper on the memory card: %s", r == HELPER_SAME ? "same as in APPS" : "DIFFERENT from the one in APPS");
    return r;
}

/* the root folder of the card the PS2 sees in that slot (the one the sd2psx is emulating), as mcfs_root_signature
 * reads it from a .mcd */
int mc_root_signature(int port, char hex[65])
{
    static sceMcTblGetDir table[512];
    static unsigned char rec[512][ROOT_REC];
    int n, i, k = 0;
    if (memcard_ready(port) != 0)
        return -1;
    mcGetDir(port, 0, "/*", 0, 512, table);
    n = mc_wait();
    log_msg("helper: root of mc%d has %d entries", port, n);
    if (n < 0)
        return -1;
    for (i = 0; i < n; i++) {
        const char *name = (const char *)table[i].EntryName;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        memset(rec[k], 0, ROOT_REC);
        memcpy(rec[k], name, strnlen(name, 32));
        memcpy(rec[k] + 32, (const unsigned char *)&table[i]._Modify + 1, 7);
        k++;
    }
    mcfs_sign_records(rec, k, hex);
    return 0;
}

int helperNeedKb, helperFreeKb;

/* the memory card's free space plus what the helper already there will give back when it's replaced, in KB; -1 =
 * unknown */
static int room_kb(void)
{
    static sceMcTblGetDir entry[1];
    int type = 0, freeKb = 0, format = 0, n;
    mcGetInfo(0, 0, &type, &freeKb, &format);
    if (mc_wait() < -1)   /* 0 and -1 (a card was changed) both come with the information */
        return -1;
    mcGetDir(0, 0, TARGET, 0, 1, entry);
    n = mc_wait();
    if (n == 1)
        freeKb += (entry[0].FileSizeByte + 1023) / 1024;
    return freeKb;
}

/* the helper in 1 KB clusters, and a few more for the BOOT folder and its entries */
static int need_kb(size_t elfLen) { return (int)((elfLen + 1023) / 1024) + 4; }

/* the helper's ELF, from the app's folder. 0 = read */
static int read_helper(buffer_t *elf)
{
    char source[260];
    snprintf(source, sizeof(source), "%sSD2CLOUD-IGR.ELF", appDir);
    if (file_read(source, elf) != 0 || elf->len < 1024) {
        buf_free(elf);
        return -1;
    }
    return 0;
}

int helper_space(void)
{
    buffer_t elf = {0};
    helperNeedKb = 0;
    helperFreeKb = -1;
    if (read_helper(&elf) != 0)
        return -1;
    helperNeedKb = need_kb(elf.len);
    buf_free(&elf);
    if (memcard_ready(0) == 0)
        helperFreeKb = room_kb();
    return 0;
}

int helper_install(void)
{
    buffer_t elf = {0};
    int r = -1;
    if (read_helper(&elf) != 0)
        return -2;
    if (memcard_ready(0) == 0) {
        helperNeedKb = need_kb(elf.len);
        helperFreeKb = room_kb();
        log_msg("helper: %d KB needed, %d KB available", helperNeedKb, helperFreeKb);
        if (helperFreeKb >= 0 && helperFreeKb < helperNeedKb) {
            buf_free(&elf);
            return -3;   /* not enough room: nothing was touched */
        }
        r = mc_write_file(TARGET, elf.data, (int)elf.len);
    }
    log_msg("helper: %s (%u bytes)", r == 0 ? "installed and verified" : "FAILED", (unsigned)elf.len);
    buf_free(&elf);
    return r;
}

int helper_uninstall(void)
{
    int r;
    if (memcard_ready(0) != 0)
        return -1;
    mcDelete(0, 0, TARGET);
    r = mc_wait();
    log_msg("helper: removed (%d)", r);
    return (r == 0 || r == -4) ? 0 : -1;   /* -4: it wasn't there */
}

int helper_present(void)
{
    char source[260];
    /* the helper only starts SD2Cloud from the microSD: with this copy started from another device and none there,
     * installing it would do nothing */
    snprintf(source, sizeof(source), "%sSD2CLOUD-IGR.ELF", appDir);
    return !appElsewhere && file_exists(source);
}
