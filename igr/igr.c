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
 * if nothing is there any more, it is looked for in APPS/SD2Cloud.
 *
 * It doesn't have to be on the memory card: OPL's IGR also runs an ELF from a USB drive ("mass:"), so with the
 * APPS/SD2Cloud folder copied to one, "Exit to" can point at this file right there. SD2Cloud is then next to it, and
 * is started from there.
 */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <elf-loader.h>
#include <ps2_fileXio_driver.h>
#include <ps2_sio2man_driver.h>

extern unsigned char mmceman_irx[];
extern unsigned int size_mmceman_irx;

#ifndef TEST
#define ELF "SD2CLOUD.ELF"
#else
#define ELF "SD2CLOUD-TEST.ELF"
#endif

/* the roots a microSD can be at: the sd2psx in either slot (and PCSX2's host: when testing) */
static const char *const roots[] = {"mmce0:/", "mmce1:/",
#ifdef TEST
                                    "host:",
#endif
                                    NULL};

#ifdef TEST
/* what was tried before the reset, written to the microSD once it can be reached (igr-log.txt in the data folder) */
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
#else
#define note(what, path, r)
#endif

static void run(const char *path)
{
    static char *args[] = {"-igr", NULL};
    int fd = open(path, O_RDONLY);
    note("open", path, fd);
    if (fd < 0)
        return;
    close(fd);
    fd = LoadELFFromFile(path, 1, args);
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
    run(path);
    SifLoadFileExit();   /* not there: on with the usual way, from scratch */
}

/* the "app_path" line of the settings on that microSD: where SD2Cloud said it is ("" = it didn't) */
static void told_path(const char *root, char *out, int size)
{
    static char ini[16 * 1024 + 1];
    char file[48], *p, *end;
    int fd, n;
    out[0] = 0;
    strcpy(file, root);
    strcat(file, "SD2Cloud/sd2cloud.ini");
    if ((fd = open(file, O_RDONLY)) < 0)
        return;
    n = read(fd, ini, sizeof(ini) - 1);
    close(fd);
    ini[n > 0 ? n : 0] = 0;
    for (p = ini; *p; p = *end ? end + 1 : end) {
        end = p + strcspn(p, "\n");
        p += strspn(p, " \t");
        if (strncmp(p, "app_path", 8) != 0)
            continue;
        p += 8 + strspn(p + 8, " \t");
        if (*p != '=')
            continue;
        p++;
        p += strspn(p, " \t");
        n = strcspn(p, "\r\n");
        while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
            n--;
        if (n > 0 && n < size) {
            memcpy(out, p, n);
            out[n] = 0;
        }
        return;
    }
}

int main(int argc, char *argv[])
{
    static char path[256];
    int i, k;
    SifInitRpc(0);
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
#ifdef TEST
    {
        int fd = open("mmce0:/SD2Cloud/igr-log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            write(fd, "started as ", 11);
            write(fd, argc > 0 && argv[0] ? argv[0] : "(nothing)", strlen(argc > 0 && argv[0] ? argv[0] : "(nothing)"));
            write(fd, "\r\n", 2);
            write(fd, tried, strlen(tried));
            close(fd);
        }
    }
#endif
    /* the sd2psx may still be switching cards when OPL hands over: a few tries before giving up */
    for (i = 0; i < 6; i++) {
        for (k = 0; roots[k]; k++) {   /* where SD2Cloud said it is */
            told_path(roots[k], path, sizeof(path));
            if (!strncmp(path, "mmce?:", 6) && !strncmp(roots[k], "mmce", 4))
                path[4] = roots[k][4];   /* the microSD those settings are on */
            if (path[0])
                run(path);
        }
        for (k = 0; roots[k]; k++) {   /* its usual folder */
            strcpy(path, roots[k]);
            strcat(path, "APPS/SD2Cloud/" ELF);
            run(path);
        }
        usleep(300 * 1000);
    }
    /* not found (or it didn't run): the OSD, as OPL would do without an "Exit to" */
    ExecOSD(0, NULL);
    return 0;
}
