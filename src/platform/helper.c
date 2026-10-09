/*
 * SD2Cloud -- what it keeps on the memory card: the APP_SD2CLOUD save folder, with the IGR helper and the shortcut.
 *
 * OPL's IGR only runs the "Exit to" ELF (exit_path) from the memory card or USB: it only loads rom0:SIO2MAN and
 * rom0:MCMAN. With "MMCE IGR slot" on, OPL switches to the BootCard first, so the helper has to be on the boot card.
 * The helper only loads the mmceman and runs SD2Cloud from the microSD with -igr.
 *
 * The helper finds SD2Cloud by the path SD2Cloud keeps in its own settings (sd2cloud.ini, "app_path"), so the
 * program itself never goes to the memory card.
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
static void remove_partial(int port, const char *path)
{
    mcDelete(port, 0, path);
    log_msg("helper: incomplete %s deleted (%d)", path, mc_wait());
}

/* told of each piece written, and of each one read back (NULL = nobody) */
static void (*mcStep)(int bytes);

static int mc_write_file(int port, const char *path, const unsigned char *d, int n)
{
    int fd, done = 0, r;
    static unsigned char back[64 * 1024] __attribute__((aligned(64)));
    mcDelete(port, 0, path);   /* mcman doesn't truncate: delete the old one first */
    log_msg("helper: delete %s %d (fine if it didn't exist)", path, mc_wait());
    mcOpen(port, 0, path, MC_WRONLY | MC_CREAT);
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
            remove_partial(port, path);
            return -1;
        }
        done += k;
        if (mcStep)
            mcStep(k);
    }
    mcClose(fd);
    if ((r = mc_wait()) < 0) {
        log_msg("helper: close %d", r);
        remove_partial(port, path);
        return -1;
    }
    /* read it back and compare */
    mcOpen(port, 0, path, MC_RDONLY);
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
            remove_partial(port, path);
            return -1;
        }
        done += k;
        if (mcStep)
            mcStep(k);
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

/* ------------------------------------------------------------ what goes to the memory card

   Turning the automatic sync on puts a save folder on the memory card in use, the Save Application System's way:
   APP_SD2CLOUD, with its icon and its title.cfg, the IGR helper (IGR.ELF, what OPL's "IGR Path" points at: a short
   name, as that path is typed with the controller) and the shortcut the folder starts from the PS2 browser or from
   a launcher (OPEN.ELF), which opens SD2Cloud from the microSD. All of it is
   embedded in the program (see the Makefile), so what is on the card can always be told from what this version
   would write, and written again.

   On the screens this folder is the "SAS package".

   Up to version 1.5 only the helper went to the card, as mc0:/BOOT/SD2CLOUD-IGR.ELF, and OPL was given that path.
   One that is there stays there (helperLegacy says so) and is written again whenever the folder is, so that path
   keeps working; without the folder, the card counts as having an outdated package. */

extern unsigned char card_igr_elf[], card_open_elf[], card_icon_sys[], card_icon_icn[], card_title_cfg[];
extern unsigned int size_card_igr_elf, size_card_open_elf, size_card_icon_sys, size_card_icon_icn, size_card_title_cfg;

#define CARD_DIR    "/APP_SD2CLOUD"
#define CARD_HELPER CARD_DIR "/IGR.ELF"
#define CARD_OPEN   CARD_DIR "/OPEN.ELF"
/* the helper in the folder as it was named before (in the package of version 1.5, next to the whole program): a
 * folder that has it is SD2Cloud's, and outdated */
#define CARD_HELPER_BEFORE CARD_DIR "/SD2CLOUD-IGR.ELF"
#define LEGACY_DIR  "/BOOT"
#define LEGACY      LEGACY_DIR "/SD2CLOUD-IGR.ELF"

int helperLegacy;

const char *helper_path(void) { return CARD_HELPER; }
const char *helper_place(void) { return CARD_DIR; }

typedef struct {
    const char *name;
    const unsigned char *data;
    unsigned int len;
} folder_file_t;
#define FOLDER_FILES 5

/* title.cfg as the package has it (tools/make_release.py): this version in place of @VERSION@, Windows line endings */
static void folder_title(buffer_t *b)
{
    const char *p = (const char *)card_title_cfg, *end = p + size_card_title_cfg;
    for (; p < end; p++) {
        if (*p == '\r')
            continue;
        if (*p == '\n')
            buf_append(b, "\r\n", 2);
        else if (end - p >= 9 && !memcmp(p, "@VERSION@", 9)) {
            buf_append(b, APP_VERSION, strlen(APP_VERSION));
            p += 8;
        } else
            buf_append(b, p, 1);
    }
}

/* the folder's files, in the order the package has them (title.cfg is made in title, which the caller frees) */
static void folder_list(folder_file_t f[FOLDER_FILES], buffer_t *title)
{
    folder_title(title);
    f[0] = (folder_file_t){"icon.sys", card_icon_sys, size_card_icon_sys};
    f[1] = (folder_file_t){"sd2cloud.icn", card_icon_icn, size_card_icon_icn};
    f[2] = (folder_file_t){"title.cfg", title->data, (unsigned int)title->len};
    f[3] = (folder_file_t){"OPEN.ELF", card_open_elf, size_card_open_elf};
    f[4] = (folder_file_t){"IGR.ELF", card_igr_elf, size_card_igr_elf};
}

/* a file or a folder of the memory card in use: its size (a folder's: how many entries it has), -1 = it isn't there */
static int mc_size(const char *path)
{
    static sceMcTblGetDir e[1];
    mcGetDir(0, 0, path, 0, 1, e);
    return mc_wait() == 1 ? (int)e[0].FileSizeByte : -1;
}

/* is that file of the memory card in use these very bytes? 1 = yes, 0 = it is another, -1 = it isn't there */
static int mc_same(const char *path, const unsigned char *d, int n)
{
    static unsigned char block[16 * 1024] __attribute__((aligned(64)));
    int fd, done = 0, k, same = 1;
    mcOpen(0, 0, path, MC_RDONLY);
    if ((fd = mc_wait()) < 0)
        return -1;
    for (;;) {
        mcRead(fd, block, sizeof(block));
        if ((k = mc_wait()) <= 0)
            break;
        if (done + k > n || memcmp(block, d + done, k) != 0) {
            same = 0;
            break;
        }
        done += k;
    }
    if (done != n)
        same = 0;
    mcClose(fd);
    mc_wait();
    return same;
}

/* What the memory card in use has of this version's folder. The helper is compared byte by byte: it is what runs with
 * nobody watching. The shortcut, the icon and a helper in BOOT only by their sizes: this is asked every time SD2Cloud
 * opens, and reading from a memory card is slow. title.cfg isn't compared at all, or every new version would call the
 * folder outdated for the number in it */
int helper_status(void)
{
    int same, old;
    helperLegacy = 0;
    if (appOnCard)   /* the helper OPL runs is the one next to the program: there, it is the right one */
        return card_app_helper() ? HELPER_SAME : HELPER_NO_FILE;
    if (memcard_ready(0) != 0)
        return HELPER_NOT_INSTALLED;
    same = mc_same(CARD_HELPER, card_igr_elf, (int)size_card_igr_elf);
    if (same < 0 && mc_size(CARD_HELPER_BEFORE) >= 0)
        same = 0;
    old = mc_size(LEGACY);
    helperLegacy = old >= 0;
    if (same < 0 && old < 0)
        return HELPER_NOT_INSTALLED;
    if (same == 1 && (mc_size(CARD_OPEN) != (int)size_card_open_elf
                      || mc_size(CARD_DIR "/sd2cloud.icn") != (int)size_card_icon_icn
                      || mc_size(CARD_DIR "/icon.sys") != (int)size_card_icon_sys || mc_size(CARD_DIR "/title.cfg") < 0
                      || (old >= 0 && old != (int)size_card_igr_elf)))
        same = 0;
    log_msg("SAS package on the memory card%s%s: %s", same < 0 ? " (none: only the helper in BOOT)" : "",
            same >= 0 && old >= 0 ? " (and a helper in BOOT)" : "", same == 1 ? "this version's" : "DIFFERENT from this version's");
    return same == 1 ? HELPER_SAME : HELPER_DIFFERENT;
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

/* a file in 1 KB clusters, and a few more for its folder and the entries in it */
static int need_kb(size_t len) { return (int)((len + 1023) / 1024) + 4; }

/* what installing takes: the whole folder */
static int install_kb(void)
{
    folder_file_t f[FOLDER_FILES];
    buffer_t title = {0};
    int i, kb = 6;
    folder_list(f, &title);
    for (i = 0; i < FOLDER_FILES; i++)
        kb += (int)((f[i].len + 1023) / 1024);
    buf_free(&title);
    return kb;
}

/* the memory card's free space plus what the folder has now, which gives its room back when it is written again, in
 * KB; -1 = unknown */
static int room_kb(void)
{
    static sceMcTblGetDir list[32];
    int type = 0, freeKb = 0, format = 0, n, i;
    mcGetInfo(0, 0, &type, &freeKb, &format);
    if (mc_wait() < -1)   /* 0 and -1 (a card was changed) both come with the information */
        return -1;
    mcGetDir(0, 0, CARD_DIR "/*", 0, 32, list);
    n = mc_wait();
    for (i = 0; i < n; i++)
        if (list[i].AttrFile & MC_ATTR_FILE)
            freeKb += (list[i].FileSizeByte + 1023) / 1024;
    return freeKb;
}

int helper_space(void)
{
    helperNeedKb = install_kb();
    helperFreeKb = memcard_ready(0) == 0 ? room_kb() : -1;
    return 0;
}

/* ------------------------------------------------------------ a save, into the card in a slot */

int mc_slot(void) { return strncmp(sdRoot, "mmce", 4) ? 0 : sdRoot[4] - '0'; }

int mc_has_folder(int port, const char *folder)
{
    static sceMcTblGetDir e[1];
    char path[48];
    int r;
    if (memcard_ready(port) != 0)
        return -1;
    snprintf(path, sizeof(path), "/%.32s", folder);
    mcGetDir(port, 0, path, 0, 1, e);
    r = mc_wait();
    return r == 1 ? 1 : r == 0 || r == -4 ? 0 : -1;   /* (-4: no such entry) */
}

int mc_delete_save(int port, const char *folder)
{
    static sceMcTblGetDir list[64];
    char path[100];
    int n, i, pass;
    for (pass = 0; pass < 8; pass++) {   /* (a folder with more files than the list holds takes more than one pass) */
        int files = 0;
        snprintf(path, sizeof(path), "/%.32s/*", folder);
        mcGetDir(port, 0, path, 0, 64, list);
        n = mc_wait();
        for (i = 0; i < n; i++) {
            const char *name = (const char *)list[i].EntryName;
            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;
            snprintf(path, sizeof(path), "/%.32s/%.32s", folder, name);
            mcDelete(port, 0, path);
            mc_wait();
            files++;
        }
        if (!files)
            break;
    }
    snprintf(path, sizeof(path), "/%.32s", folder);
    mcDelete(port, 0, path);
    log_msg("slot: %s deleted (%d)", path, mc_wait());
    return mc_has_folder(port, folder) == 0 ? 0 : -1;
}

typedef struct {
    int port, files;
    char folder[40];
    unsigned char root[64];   /* the folder's own entry: its mode and its dates */
} put_t;

static void entry_info(const unsigned char *entry, sceMcTblGetDir *info)
{
    memset(info, 0, sizeof(*info));
    memcpy(&info->_Create, entry + 8, 8);    /* (the dates are kept on a card the way mcman takes them) */
    memcpy(&info->_Modify, entry + 24, 8);
    info->AttrFile = entry[0] | entry[1] << 8;
}

/* (mcfs_psu_files) the save's folder is made, then each file is written, read back and given its dates */
static int put_piece(const unsigned char *entry, const unsigned char *data, unsigned int len, void *u)
{
    static sceMcTblGetDir info;
    put_t *p = u;
    char path[80];
    int r;
    if (!data && ((entry[0] | entry[1] << 8) & 0x0020)) {
        memcpy(p->root, entry, sizeof(p->root));
        snprintf(p->folder, sizeof(p->folder), "/%.32s", (const char *)entry + 64);
        mcMkDir(p->port, 0, p->folder);
        r = mc_wait();
        log_msg("slot: folder %s made (%d)", p->folder, r);
        return r == 0 ? 0 : MCFS_ERR_IO;
    }
    snprintf(path, sizeof(path), "%s/%.32s", p->folder, (const char *)entry + 64);
    if (mc_write_file(p->port, path, data, (int)len) != 0)
        return MCFS_ERR_IO;
    entry_info(entry, &info);
    mcSetFileInfo(p->port, 0, path, &info, sceMcFileInfoCreate | sceMcFileInfoModify | sceMcFileInfoAttr);
    mc_wait();
    p->files++;
    return 0;
}

int mc_put_save(int port, const char *psu)
{
    static sceMcTblGetDir info;
    put_t p;
    mcfs_psu_t what;
    buffer_t a = {0}, b = {0};
    int r, type = 0, freeKb = 0, format = 0;
    if (memcard_ready(port) != 0)
        return MCFS_ERR_IO;
    r = mcfs_psu_info(psu, &what, &a, &b);
    buf_free(&a);
    buf_free(&b);
    if (r != MCFS_OK)
        return r;
    if ((r = mc_has_folder(port, what.folder)) != 0)
        return r < 0 ? MCFS_ERR_IO : MCFS_ERR_EXISTS;
    mcGetInfo(port, 0, &type, &freeKb, &format);
    mc_wait();
    if (freeKb < (int)((what.bytes + 1023) / 1024) + what.files + 3)   /* (each file's last cluster, and the folder's own) */
        return MCFS_ERR_FULL;
    memset(&p, 0, sizeof(p));
    p.port = port;
    if ((r = mcfs_psu_files(psu, put_piece, &p)) != 0) {
        if (p.folder[0])
            mc_delete_save(port, p.folder + 1);   /* half a save is no save */
        return r;
    }
    /* the folder's own dates last: writing its files changed them */
    entry_info(p.root, &info);
    mcSetFileInfo(port, 0, p.folder, &info, sceMcFileInfoCreate | sceMcFileInfoModify | sceMcFileInfoAttr);
    mc_wait();
    log_msg("slot: %s put into the card in slot %d, %d file(s)", p.folder, port + 1, p.files);
    return MCFS_OK;
}

/* everything the folder has, deleted: its own files from an earlier version, or the whole program the package of
 * version 1.5 kept there */
static void folder_clear(void)
{
    static sceMcTblGetDir list[32];
    char path[100];
    int n, i;
    mcGetDir(0, 0, CARD_DIR "/*", 0, 32, list);
    n = mc_wait();
    for (i = 0; i < n; i++) {
        const char *name = (const char *)list[i].EntryName;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        snprintf(path, sizeof(path), CARD_DIR "/%.32s", name);
        mcDelete(0, 0, path);
        mc_wait();
    }
}

/* The folder's date is the one the Save Application System gives it by its name (tools/make_release.py, sas_date:
 * 2098-12-31 13:46:24 for APP_SD2CLOUD): the menus that list a card's programs sort them by it */
static void folder_date(void)
{
    static sceMcTblGetDir e;
    memset(&e, 0, sizeof(e));
    e._Create.Sec = 24;
    e._Create.Min = 46;
    e._Create.Hour = 13;
    e._Create.Day = 31;
    e._Create.Month = 12;
    e._Create.Year = 2098;
    e._Modify = e._Create;
    mcSetFileInfo(0, 0, CARD_DIR, &e, sceMcFileInfoCreate | sceMcFileInfoModify);
    log_msg("helper: the folder's date set (%d)", mc_wait());
}

int helper_install(void)
{
    folder_file_t f[FOLDER_FILES];
    buffer_t title = {0};
    char path[100];
    int i, r = 0;
    if (appOnCard || memcard_ready(0) != 0)
        return -1;
    /* a helper in BOOT, from a version up to 1.5: this version's, in its place (it takes no more room there), so the
     * path OPL was given keeps working whatever happens to the folder */
    if (mc_size(LEGACY) >= 0 && mc_same(LEGACY, card_igr_elf, (int)size_card_igr_elf) != 1)
        log_msg("helper: the one in BOOT written again (%d)", mc_write_file(0, LEGACY, card_igr_elf, (int)size_card_igr_elf));
    helperNeedKb = install_kb();
    helperFreeKb = room_kb();
    log_msg("helper: %d KB needed, %d KB available", helperNeedKb, helperFreeKb);
    if (helperFreeKb >= 0 && helperFreeKb < helperNeedKb)
        return -3;   /* not enough room: the folder wasn't touched */
    mcMkDir(0, 0, CARD_DIR);
    log_msg("helper: mkdir %d (fine if it exists)", mc_wait());
    folder_clear();
    folder_list(f, &title);
    for (i = 0; i < FOLDER_FILES && r == 0; i++) {
        snprintf(path, sizeof(path), CARD_DIR "/%s", f[i].name);
        r = mc_write_file(0, path, f[i].data, (int)f[i].len);
    }
    buf_free(&title);
    folder_date();
    log_msg("helper: %s", r == 0 ? "installed and verified" : "FAILED");
    return r;
}

/* the folder and the helper an earlier version left in BOOT, whichever are there */
int helper_uninstall(void)
{
    int r, ok = 1;
    if (appOnCard || memcard_ready(0) != 0)
        return -1;
    if (mc_size(CARD_DIR) >= 0) {
        folder_clear();
        mcDelete(0, 0, CARD_DIR);
        r = mc_wait();
        log_msg("helper: folder removed (%d)", r);
        ok &= r == 0 || r == -4;   /* -4: it wasn't there */
    }
    mcDelete(0, 0, LEGACY);
    r = mc_wait();
    log_msg("helper: the one in BOOT removed (%d)", r);
    ok &= r == 0 || r == -4;
    return ok ? 0 : -1;
}

int helper_present(void)
{
    if (appOnCard)
        return card_app_helper();
    /* the helper only starts SD2Cloud from the microSD: with this copy started from another device and none there,
     * installing it would do nothing */
    return 1;
}

/* ------------------------------------------------------------ the program on a memory card

   The Save Application System package (APP_SD2CLOUD.psu) puts the program, its IGR helper and title.cfg in a save
   folder of a memory card, and the program is started from there. With no SD2Cloud on the microSD to take over, that
   folder is the program's place (appOnCard): OPL's IGR runs the helper that is in it, with nothing to install, and an
   update is written to it. */

#define OWN_APP    "SD2CLOUD.ELF"
#define OWN_HELPER "SD2CLOUD-IGR.ELF"

static void own_path(const char *name, char *out, size_t size)
{
    snprintf(out, size, "%s%s%s", appCardDir, name[0] ? "/" : "", name);
}

/* a file of that folder ("" = the folder itself): 1 = it is there, with what the card says of it in e */
static int own_entry(const char *name, sceMcTblGetDir *e)
{
    static sceMcTblGetDir table[1];
    char path[120];
    int n;
    own_path(name, path, sizeof(path));
    mcGetDir(appCardPort, 0, path, 0, 1, table);
    n = mc_wait();
    if (n == 1 && e)
        *e = table[0];
    return n == 1;
}

static int own_read(const char *name, buffer_t *b)
{
    static unsigned char block[16 * 1024] __attribute__((aligned(64)));
    char path[120];
    int fd, n;
    own_path(name, path, sizeof(path));
    mcOpen(appCardPort, 0, path, MC_RDONLY);
    if ((fd = mc_wait()) < 0)
        return -1;
    for (;;) {
        mcRead(fd, block, sizeof(block));
        if ((n = mc_wait()) <= 0 || buf_append(b, block, n) != 0)
            break;
    }
    mcClose(fd);
    mc_wait();
    return n == 0 ? 0 : -1;
}

static void own_delete(const char *name)
{
    char path[120];
    own_path(name, path, sizeof(path));
    mcDelete(appCardPort, 0, path);
    mc_wait();
}

int card_app_helper(void) { return memcard_ready(appCardPort) == 0 && own_entry(OWN_HELPER, NULL); }

int card_app_read(buffer_t *b) { return memcard_ready(appCardPort) == 0 ? own_read(OWN_APP, b) : -1; }

/* One file of the folder gives way to a new one: mcman can't replace a file in one step, so the new one is written
 * next to it under another name, read back, and takes its name only then (the old one is deleted at that moment) */
static int own_replace(const char *name, const unsigned char *d, int n)
{
    char path[120], fresh[120];
    int r;
    own_path(name, path, sizeof(path));
    snprintf(fresh, sizeof(fresh), "%s.new", path);
    if (mc_write_file(appCardPort, fresh, d, n) != 0)
        return -1;
    mcDelete(appCardPort, 0, path);
    mc_wait();
    mcRename(appCardPort, 0, fresh, name);
    if ((r = mc_wait()) == 0)
        return 0;
    /* it couldn't take the name: written once more, under it, in the room the old one left */
    log_msg("helper: rename of %s returned %d, writing it again", fresh, r);
    if (mc_write_file(appCardPort, path, d, n) != 0)
        return -1;
    mcDelete(appCardPort, 0, fresh);
    mc_wait();
    return 0;
}

/* title.cfg says which version the folder has and when it came out: the new ones (each one when it is known: a beta
 * has a date and no number of its own), every other line as it was */
static void own_title(const char *version, const char *released)
{
    buffer_t in = {0}, out = {0};
    const char *p, *end;
    char path[120], line[120];
    int changed = 0;
    if (own_read("title.cfg", &in) != 0 || !in.len) {
        buf_free(&in);
        return;
    }
    for (p = (const char *)in.data, end = p + in.len; p < end;) {
        const char *nl = memchr(p, '\n', end - p);
        size_t len = nl ? (size_t)(nl - p + 1) : (size_t)(end - p);
        const char *eol = !nl ? "" : (len > 1 && nl[-1] == '\r') ? "\r\n" : "\n", *value = NULL;
        if (len > 8 && !strncmp(p, "Version=", 8) && version[0])
            value = version;
        else if (len > 8 && !strncmp(p, "Release=", 8) && released[0])
            value = released;
        if (value) {
            snprintf(line, sizeof(line), "%.8s%s%s", p, value, eol);
            buf_append(&out, line, strlen(line));
            changed |= strlen(line) != len || memcmp(line, p, len) != 0;
        } else
            buf_append(&out, p, len);
        p += len;
    }
    if (changed && out.len) {
        own_path("title.cfg", path, sizeof(path));
        log_msg("helper: title.cfg with version %s: %d", version, mc_write_file(appCardPort, path, out.data, (int)out.len));
    }
    buf_free(&in);
    buf_free(&out);
}

static long long ownDone, ownTotal;
static void (*ownProgress)(long long done, long long total);

static void own_step(int bytes)
{
    ownDone += bytes;
    if (ownProgress)
        ownProgress(ownDone, ownTotal);
}

int card_app_update(const buffer_t *app, const buffer_t *helper, const char *version, const char *released,
                    void (*progress)(long long done, long long total))
{
    static sceMcTblGetDir folder;
    buffer_t old = {0};
    int type = 0, freeKb = 0, format = 0, sameHelper, r;
    if (memcard_ready(appCardPort) != 0 || !own_entry("", &folder))
        return -1;
    /* what an update that was cut short left behind */
    own_delete(OWN_APP ".new");
    own_delete(OWN_HELPER ".new");
    /* the helper seldom changes from a version to the next: the same one stays as it is */
    sameHelper = !helper->len || (own_read(OWN_HELPER, &old) == 0 && old.len == helper->len && !memcmp(old.data, helper->data, old.len));
    buf_free(&old);
    /* room for the largest of the new files next to the one it replaces, which goes only when the new one is whole */
    mcGetInfo(appCardPort, 0, &type, &freeKb, &format);
    r = mc_wait();
    helperNeedKb = need_kb(app->len);
    helperFreeKb = r < -1 ? -1 : freeKb;
    log_msg("helper: update of %s: %d KB needed, %d KB free, helper %s", appCardDir, helperNeedKb, helperFreeKb, sameHelper ? "the same" : "new");
    if (helperFreeKb >= 0 && helperFreeKb < helperNeedKb)
        return -3;   /* nothing was touched */
    ownDone = 0;
    ownTotal = 2 * ((long long)app->len + (sameHelper ? 0 : (long long)helper->len));   /* written, then read back */
    ownProgress = progress;
    mcStep = own_step;
    /* the helper first: if it can't be written the program doesn't change */
    r = (sameHelper || own_replace(OWN_HELPER, helper->data, (int)helper->len) == 0) && own_replace(OWN_APP, app->data, (int)app->len) == 0 ? 0 : -1;
    mcStep = NULL;
    if (r == 0)
        own_title(version, released);
    /* the folder keeps its dates: the menus that list the programs of a memory card sort them by it */
    mcSetFileInfo(appCardPort, 0, appCardDir, &folder, sceMcFileInfoCreate | sceMcFileInfoModify);
    log_msg("helper: update of %s %s; the folder's dates put back (%d)", appCardDir, r == 0 ? "written and verified" : "FAILED", mc_wait());
    return r;
}
