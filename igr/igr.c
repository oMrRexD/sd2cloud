/*
 * SD2CLOUD-IGR.ELF -- SD2Cloud's IGR helper.
 *
 * OPL's IGR only runs the "Exit to" ELF (exit_path) from the memory card or USB (it only loads rom0:SIO2MAN and
 * rom0:MCMAN). That's why this small ELF lives in mc?:/BOOT/ of the boot card and does only one thing: loads the
 * mmceman (from the same SDK as the sio2man) and runs SD2Cloud from the sd2psx microSD with -igr. If it can't find
 * it, it goes to the PS2 menu (the browser, or whatever is installed on the memory card: FMCB, PS2BBL, OSDMenu...).
 *
 * Where SD2Cloud is: SD2Cloud writes its own path in its settings every time it is opened (the "app_path" line of
 * SD2Cloud/sd2cloud.ini, always at the root of the microSD), so it can be kept in any folder. Without that line, or
 * if nothing is there any more, it is looked for in APPS/SD2Cloud, and then in every other folder of APPS: a folder
 * that was renamed, or moved, before SD2Cloud was opened from it once (which only this file would do).
 *
 * SD2Cloud doesn't have to be on the microSD: on a USB drive, an MX4SIO or the hard disk, its settings (which are
 * always on the microSD) say so, and SD2Cloud leaves on the microSD the drivers that device needs, which this file
 * doesn't carry (SD2Cloud/drivers/). They are loaded from there and SD2Cloud is started from the device
 * (run_elsewhere).
 *
 * Nor does this file have to be on the memory card: OPL's IGR also runs an ELF from a USB drive ("mass:"), so with the
 * APPS/SD2Cloud folder copied to one, "Exit to" can point at this file right there. SD2Cloud is then next to it, and
 * is started from there.
 *
 * SD2Cloud itself may live on the memory card, in the save folder the Save Application System package makes
 * (mc?:/APP_SD2CLOUD), with this file next to it: "Exit to" points at this file there, and with no SD2Cloud on the
 * microSD the one next to it is started.
 *
 * With no sync to do (the automatic sync turned off in SD2Cloud's settings, or no Google account connected), SD2Cloud
 * isn't started at all: what comes after IGR is started from here (see skip_sync).
 *
 * SD2CLOUD-OPEN.ELF -- the shortcut: this same program built with -DOPEN. SD2Cloud puts both in a save folder of the
 * memory card (mc?:/APP_SD2CLOUD, with an icon and a title.cfg, the Save Application System's way), under the short
 * names IGR.ELF and OPEN.ELF, and the shortcut is what that folder starts from the PS2 browser or a launcher: it finds SD2Cloud on the microSD the same way and
 * opens it as the user would, with no -igr. When it isn't there, it says so on the screen.
 */
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <elf-loader.h>
/* only for fileXioInit and fileXioExit, which don't touch newlib's file descriptors */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <ps2_fileXio_driver.h>
#include <ps2_sio2man_driver.h>
#ifndef FIO_MT_RDONLY
#define FIO_MT_RDONLY 0x01   /* (io_common.h's, which only the old port may include) */
#endif
#ifdef OPEN
#include <debug.h>
#define AS_IGR 0   /* the shortcut: SD2Cloud is opened as the user would open it */
#else
#define AS_IGR 1   /* the IGR helper: SD2Cloud is told that it was IGR that started it */
#endif

extern unsigned char mmceman_irx[];
extern unsigned int size_mmceman_irx;
extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;

#ifndef DEBUG_BUILD
#define ELF "SD2CLOUD.ELF"
#else
#define ELF "SD2CLOUD-DEBUG.ELF"
#endif

/* the roots a microSD can be at: the sd2psx in either slot (and PCSX2's host: when testing) */
static const char *const roots[] = {"mmce0:/", "mmce1:/",
#ifdef DEBUG_BUILD
                                    "host:",
#endif
                                    NULL};

#ifdef DEBUG_BUILD
/* what was tried, written to the microSD once it can be reached (igr-log.txt in the data folder) */
static char tried[600];
static void note(const char *what, const char *path, int r)
{
    char n[16], *p = n + sizeof(n) - 1;
    int v = r < 0 ? -r : r;
    *p = 0;
    do
        *--p = '0' + v % 10;
    while ((v /= 10) != 0);
    if (r < 0)
        *--p = '-';
    if (strlen(tried) + strlen(what) + strlen(path) + 24 < sizeof(tried)) {
        strcat(tried, what);
        strcat(tried, " ");
        strcat(tried, path);
        strcat(tried, " = ");
        strcat(tried, p);
        strcat(tried, "\r\n");
    }
}

static void save_notes(const char *root)
{
    char file[48];
    int fd;
    strcpy(file, root);
    strcat(file, "SD2Cloud/igr-log.txt");
    if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666)) >= 0) {
        write(fd, tried, strlen(tried));
        close(fd);
    }
}
#else
#define note(what, path, r)
#define save_notes(root)
#endif

/* igr = 1: SD2Cloud, told that it was IGR that started it; 0 = any other program */
static void run(const char *path, int igr)
{
    static char *args[] = {"-igr", NULL};
    int fd = open(path, O_RDONLY);
    note("open", path, fd);
    if (fd < 0)
        return;
    close(fd);
    fd = LoadELFFromFile(path, igr, args);
    note("LoadELFFromFile", path, fd);
}

/* Started from somewhere other than a memory card: SD2Cloud may be right next to this file, in the folder that was
 * copied there. Whatever started this file can still read that folder, so SD2Cloud is started as things are, before
 * anything is reset. self = this file's own path */
static void run_next_to(const char *self)
{
    static char path[256];
    const char *name = strrchr(self, '/');
    size_t dir;
    if (!self[0] || !strncmp(self, "mc", 2))
        return;
    if (!name)
        name = strrchr(self, ':');
    if (!name || (dir = name - self + 1) + sizeof(ELF) > sizeof(path))
        return;
    memcpy(path, self, dir);
    strcpy(path + dir, ELF);
    SifLoadFileInit();
    sbv_patch_disable_prefix_check();   /* the ROM's loader only takes a few devices otherwise */
    run(path, AS_IGR);
    SifLoadFileExit();   /* not there: on with the usual way, from scratch */
}

#ifndef OPEN
/* Started from a memory card: SD2Cloud may be in the same save folder (the Save Application System package keeps both
 * there). beside = that one's path when it is there, looked for while whatever started this file can still read the
 * card */
static char beside[256];

static void look_beside(const char *self)
{
    const char *name = strrchr(self, '/');
    size_t dir;
    int fd;
    if (strncmp(self, "mc", 2) != 0 || !name || (dir = name - self + 1) + sizeof(ELF) > sizeof(beside))
        return;
    memcpy(beside, self, dir);
    strcpy(beside + dir, ELF);
    fd = open(beside, O_RDONLY);
    note("open", beside, fd);
    if (fd < 0)
        beside[0] = 0;
    else
        close(fd);
}

/* SD2Cloud from the memory card. What is loaded by now reads the microSD, not the card: the ROM's own drivers do, as
 * when OPL started this file */
static void run_beside(void)
{
    static char *args[] = {"-igr", NULL};
    int k;
    note("from the memory card:", beside, 0);
    for (k = 0; roots[k]; k++)
        save_notes(roots[k]);
    while (!SifIopReset("", 0))
        ;
    while (!SifIopSync())
        ;
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    /* the ELF loader asks for the file through fileXio before it loads it: back too, bound to the new IOP */
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    fileXioExit();
    fileXioInit();
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    LoadELFFromFile(beside, 1, args);
}
#endif

/* SD2Cloud's settings on the microSD being looked at, whole */
static char ini[16 * 1024 + 1];

/* a small text file of SD2Cloud's data folder on that microSD, whole, in text (which ends in a 0); 0 = it can't be
 * read */
static int read_text(const char *root, const char *name, char *text, int size)
{
    char file[48];
    int fd, n;
    text[0] = 0;
    strcpy(file, root);
    strcat(file, "SD2Cloud/");
    strcat(file, name);
    if ((fd = open(file, O_RDONLY)) < 0)
        return 0;
    n = read(fd, text, size - 1);
    close(fd);
    text[n > 0 ? n : 0] = 0;
    return n > 0;
}

/* the value of a key in a section of such a text ("" = the lines before any section), read the way SD2Cloud reads
 * it. In out, which stays "" when the key isn't there or its value doesn't fit; returns the value's length */
static int setting(const char *text, const char *section, const char *key, char *out, int size)
{
    const char *p, *end;
    int in = !section[0], s = strlen(section), k = strlen(key), n, i;
    out[0] = 0;
    if (!strncmp(text, "\xEF\xBB\xBF", 3))   /* the UTF-8 BOM, if the file was saved with Notepad */
        text += 3;
    for (p = text; *p; p = *end ? end + 1 : end) {
        end = p + strcspn(p, "\n");
        p += strspn(p, " \t");
        if (*p == '[') {
            in = !strncasecmp(p + 1, section, s) && p[1 + s] == ']';
            continue;
        }
        if (!in || strncasecmp(p, key, k) != 0)
            continue;
        p += k + strspn(p + k, " \t");
        if (*p != '=')
            continue;
        p++;
        p += strspn(p, " \t");
        n = strcspn(p, "\r\n");
        for (i = 1; i < n; i++)   /* a comment after the value */
            if (p[i] == ';' && (p[i - 1] == ' ' || p[i - 1] == '\t'))
                n = i;
        while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
            n--;
        if (n < size) {
            memcpy(out, p, n);
            out[n] = 0;
        }
        return n;
    }
    return 0;
}

/* a path as the settings keep it ("mmce?:" = whichever slot the sd2psx is in), on that microSD; 0 = it is on another
 * device */
static int on_sd(char *path, const char *root)
{
    if (!strncmp(path, "mmce?:", 6) && !strncmp(root, "mmce", 4))
        path[4] = root[4];
    return !strncmp(path, root, strchr(root, ':') - root + 1);
}

/* -------- SD2Cloud on a device other than the microSD. Where it is, as its settings have it (the notation of
 * SD2Cloud's launch.c): usb:/... (or mass:), mx4sio:/..., ata:/... (the hard disk formatted exFAT),
 * hdd0:PARTITION:pfs:/... (the hard disk formatted APA). The modules each one needs are files of the microSD, put
 * there by SD2Cloud under their own names, and are loaded in the order SD2Cloud itself loads them */
static unsigned char module[400 * 1024];

static int load_driver(const char *root, const char *name, int argLen, const char *args)
{
    char file[64];
    int fd, n = 0, got, ret = 0, id;
    strcpy(file, root);
    strcat(file, "SD2Cloud/drivers/");
    strcat(file, name);
    strcat(file, ".irx");
    if ((fd = open(file, O_RDONLY)) < 0) {
        note("open", file, fd);
        return -1;
    }
    while (n < (int)sizeof(module) && (got = read(fd, module + n, sizeof(module) - n)) > 0)
        n += got;
    close(fd);
    id = SifExecModuleBuffer(module, n, argLen, args, &ret);
    note("module", file, id < 0 ? id : ret);
    return id < 0 || ret == 1 ? -1 : 0;
}

static int has_prefix(const char *s, const char *prefix) { return !strncasecmp(s, prefix, strlen(prefix)); }

/* waits for a file to be there (a USB drive, an MX4SIO and the hard disk take a moment); 1 = it is */
static int wait_for(const char *path, int tenths)
{
    int t, fd;
    for (t = 0; t <= tenths; t++) {
        if ((fd = open(path, O_RDONLY)) >= 0) {
            close(fd);
            return 1;
        }
        usleep(100 * 1000);
    }
    note("not there:", path, 0);
    return 0;
}

static void run_elsewhere(const char *root, const char *where)
{
    static const char hddArgs[] = "-o\0" "4\0" "-n\0" "20";
    static const char pfsArgs[] = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
    static char path[256], part[80], block[80];
    static char *args[] = {"-igr", NULL};
    const char *rest = strchr(where, ':');
    int usb = has_prefix(where, "mass") || has_prefix(where, "usb"), mx = has_prefix(where, "mx4sio"),
        ata = has_prefix(where, "ata"), t, r;
    if (!rest)
        return;
    rest++;
    note("on another device:", where, 0);
    if (usb || mx || ata) {
        r = ata ? load_driver(root, "ps2dev9", 0, NULL) : usb ? load_driver(root, "usbd", 0, NULL) : 0;
        if (r == 0 && (r = load_driver(root, "bdm", 0, NULL)) == 0 && (r = load_driver(root, "bdmfs_fatfs", 0, NULL)) == 0)
            r = load_driver(root, usb ? "usbmass_bd" : mx ? "mx4sio_bd" : "ata_bd", 0, NULL);
        if (r != 0 || strlen(rest) + 8 > sizeof(path))
            return;
        strcpy(path, "mass0:");   /* the only block device loaded: the first one */
        strcat(path, rest);
        if (wait_for(path, 60))
            run(path, AS_IGR);
        return;
    }
    if (has_prefix(where, "hdd")) {   /* hdd0:PARTITION:pfs:/path, or hdd0:PARTITION/path */
        const char *end = rest + strcspn(rest, ":/"), *p;
        size_t n = end - rest;
        if (!*end || n + 8 > sizeof(block))
            return;
        strcpy(block, "hdd0:");
        strncat(block, rest, n);
        strcpy(part, block);
        strcat(part, ":");
        p = (*end == ':' && has_prefix(end, ":pfs:")) ? end + 5 : end;
        if (strlen(p) + 8 > sizeof(path) || load_driver(root, "ps2dev9", 0, NULL) != 0 || load_driver(root, "ps2atad", 0, NULL) != 0
            || load_driver(root, "ps2hdd", sizeof(hddArgs), hddArgs) != 0 || load_driver(root, "ps2fs", sizeof(pfsArgs), pfsArgs) != 0)
            return;
        for (t = 0; t < 30 && (r = fileXioMount("pfs0:", block, FIO_MT_RDONLY)) < 0; t++)   /* the drive takes a moment */
            usleep(100 * 1000);
        note("mount", block, r);
        strcpy(path, "pfs0:");
        if (*p != '/')
            strcat(path, "/");
        strcat(path, p);
        if (!wait_for(path, 0))
            return;
        strcpy(path, "pfs:");   /* how the ELF loader wants it */
        if (*p != '/')
            strcat(path, "/");
        strcat(path, p);
        r = LoadELFFromFileWithPartition(path, part, AS_IGR, args);
        note("LoadELFFromFileWithPartition", path, r);
    }
}

/* SD2Cloud isn't where its settings say, nor in its usual folder: started from any other folder of APPS that has it.
 * The folders' names are read first, and the folder closed, before any of them is looked into (the sd2psx does one
 * thing at a time) */
static void run_found(const char *root)
{
    static char names[24][64], path[256];
    struct dirent *e;
    DIR *d;
    int n = 0, i;
    strcpy(path, root);
    strcat(path, "APPS");
    if (!(d = opendir(path)))
        return;
    while (n < 24 && (e = readdir(d)) != NULL)
        if (e->d_name[0] != '.' && strlen(e->d_name) < sizeof(names[0]))
            strcpy(names[n++], e->d_name);
    closedir(d);
    for (i = 0; i < n; i++) {
        strcpy(path, root);
        strcat(path, "APPS/");
        strcat(path, names[i]);
        strcat(path, "/" ELF);
        run(path, AS_IGR);
    }
}

#ifdef OPEN
/* The shortcut found no SD2Cloud to open: said on the screen (the SDK's text screen has no accented letters), for long
 * enough to be read, before the PS2 menu */
static void not_found(void)
{
    init_scr();
    scr_printf("\n\n\n   SD2Cloud\n\n"
               "   SD2CLOUD.ELF was not found on the MMCE (the sd2psx microSD).\n"
               "   Copy the APPS folder of SD2Cloud's .zip to the MMCE:\n"
               "   the program goes in APPS/SD2Cloud.\n"
               "   github.com/oMrRexD/sd2cloud\n\n"
               "   SD2CLOUD.ELF nao encontrado no MMCE (o microSD do sd2psx).\n"
               "   Copie a pasta APPS do .zip do SD2Cloud para o MMCE:\n"
               "   o programa fica em APPS/SD2Cloud.\n");
    sleep(12);
}
#else
/* With no sync to do, SD2Cloud isn't needed, and loading it would only take time: the automatic sync turned off in
 * its settings ([igr] auto_sync = no), or no Google account connected (no token in token.dat). What comes after IGR
 * is then started from here: the program in [igr] return or, with "auto", the OPL SD2Cloud found the last time it
 * looked ([app] igr_auto). Only a program on this microSD, or the PS2 menu: for anything else (another device, an OPL
 * not looked for yet, a file that isn't there) SD2Cloud is started as usual, which has the drivers and does the same
 * before it shows anything */
static void skip_sync(const char *root)
{
    static char v[256], token[1024];
    if (!setting(ini, "igr", "auto_sync", v, sizeof(v)) || strcasecmp(v, "no") != 0) {
        read_text(root, "token.dat", token, sizeof(token));
        if (setting(token, "", "refresh_token", v, sizeof(v)) > 0)
            return;   /* on, with an account: there is a sync to do */
    }
    setting(ini, "igr", "return", v, sizeof(v));
    if (!v[0] || !strcasecmp(v, "auto"))
        setting(ini, "app", "igr_auto", v, sizeof(v));
    note("nothing to sync, after IGR:", v, 0);
    save_notes(root);
    if (!strcasecmp(v, "osd"))
        ExecOSD(0, NULL);
    if (on_sd(v, root))
        run(v, 0);
}
#endif

int main(int argc, char *argv[])
{
    static char path[256];
    int i, k, elsewhere = 0;
    SifInitRpc(0);
    note("started as", argc > 0 && argv[0] ? argv[0] : "(nothing)", argc);
#ifndef OPEN
    look_beside(argc > 0 && argv[0] ? argv[0] : "");
#endif
    run_next_to(argc > 0 && argv[0] ? argv[0] : "");
    while (!SifIopReset("", 0))
        ;
    while (!SifIopSync())
        ;
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    init_fileXio_driver();
    init_sio2man_driver();
    SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, NULL);
    save_notes("mmce0:/");
    /* the sd2psx may still be switching cards when OPL hands over: a few tries before giving up */
    for (i = 0; i < 6; i++) {
        for (k = 0; roots[k]; k++) {   /* what SD2Cloud's settings on that microSD say */
            if (!read_text(roots[k], "sd2cloud.ini", ini, sizeof(ini)))
                continue;
#ifndef OPEN
            skip_sync(roots[k]);
#endif
            setting(ini, "app", "app_path", path, sizeof(path));   /* where SD2Cloud said it is */
            if (path[0] && !on_sd(path, roots[k]) && !elsewhere++) {   /* another device: its drivers, once */
                run_elsewhere(roots[k], path);
                save_notes(roots[k]);
            } else if (path[0])
                run(path, AS_IGR);
        }
        for (k = 0; roots[k]; k++) {   /* its usual folder */
            strcpy(path, roots[k]);
            strcat(path, "APPS/SD2Cloud/" ELF);
            run(path, AS_IGR);
        }
        for (k = 0; roots[k]; k++)   /* any other folder of APPS */
            run_found(roots[k]);
#ifndef OPEN
        if (beside[0])   /* it is on the memory card, then: no use waiting for the microSD to have it */
            break;
#endif
        usleep(300 * 1000);
    }
#ifdef OPEN
    not_found();
#else
    if (beside[0])
        run_beside();
#endif
    /* not found (or it didn't run): the OSD, as OPL would do without an "Exit to" */
    ExecOSD(0, NULL);
    return 0;
}
