/*
 * SD2Cloud -- starting and leaving: the IOP modules, where the program and the microSD are, and handing the console
 * over clean to whatever comes next.
 * On the sd2psx everything is done one thing at a time: it can't handle two operations at once, and an open that
 * fails while another file is open breaks the other file's descriptor (firmware 1.4.0, sd2psXtd#115).
 */
#include "platform.h"

char appDir[200];
char appPath[260];
int appElsewhere;
int appTookOver;
int appOnCard;
int appCardPort;
char appCardDir[64];
char sdRoot[16];
char dataDir[32];
int igrMode;

/* ------------------------------------------------------------ start and leave */

void iop_reset(void)
{
    SifInitRpc(0);
    while (!SifIopReset("", 0))
        ;
    while (!SifIopSync())
        ;
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

int has_dir(const char *root, const char *dir)
{
    char c[64];
    DIR *d;
    snprintf(c, sizeof(c), "%s%s", root, dir);
    if (!(d = opendir(c)))
        return 0;
    closedir(d);
    return 1;
}

/* the sd2psx microSD: the one that already has the SD2Cloud folder (the data), else the one with MemoryCards/PS2,
 * always starting with the slot the app was started from. The user's data lives in <microSD>SD2Cloud/, apart from
 * the program: updating the APPS/SD2Cloud folder doesn't touch the settings, the sign-in or the history. */
static void find_sd(const char *a0)
{
    const char *options[2] = {"mmce0:/", "mmce1:/"};
    int i;
    if (strncmp(a0, "host:", 5) == 0)
        strcpy(sdRoot, "host:");   /* testing on PCSX2: the root of host: stands in for the microSD */
    else {
        if (!strncmp(a0, "mmce1:", 6))
            options[0] = "mmce1:/", options[1] = "mmce0:/";
        for (i = 0; i < 2 && !has_dir(options[i], "SD2Cloud"); i++)
            ;
        if (i == 2)
            for (i = 0; i < 2 && !has_dir(options[i], "MemoryCards/PS2"); i++)
                ;
        if (i == 2)   /* where a MemCard PRO2 keeps them */
            for (i = 0; i < 2 && !has_dir(options[i], "PS2"); i++)
                ;
        strcpy(sdRoot, options[i < 2 ? i : 0]);
    }
    snprintf(dataDir, sizeof(dataDir), "%sSD2Cloud/", sdRoot);
}

/* Started from a device other than the sd2psx: a USB drive, an MX4SIO or the hard disk (the Apps list of an OPL
 * that doesn't read the sd2psx, or simply where the user keeps it), or a memory card. The program stays where it
 * is: it tells the settings where (main.c), keeps on the microSD the drivers of that device for the IGR helper
 * and the shortcut to start it from there (drivers_store), and is updated there. Only its data is always on the
 * microSD.
 *
 * Which device it is, the name it was started under says ("mx4sio:", "ata:", "hdd0:PARTITION:pfs:"), unless it
 * is the name every block device answers to ("mass0:") or a file system's with no partition in it ("pfs0:").
 * Then each place that name can be is tried: that device's drivers are loaded, alone, and the program is looked
 * for under the same path. The settings remember the answer, so it is found out once (at IGR what matters is being
 * quick) */
static void locate_self(const char *a0)
{
    static const char *const block[] = {"usb:", "mx4sio:", "ata:", NULL};
    static const char *const parts[] = {"hdd0:+OPL:pfs:", "hdd0:__common:pfs:", NULL};
    const char *const *tries = !strncasecmp(a0, "mass", 4) ? block : !strncasecmp(a0, "pfs", 3) ? parts : NULL;
    const char *rest = strchr(a0, ':');
    char c[260];
    int i, found = 0;
    rest = rest ? rest + 1 : a0;
    if (!tries)
        return;   /* the name says the device: appPath is right as it is */
    for (i = 0; tries[i]; i++) {   /* what the settings remember, when it is one of them */
        snprintf(c, sizeof(c), "%s%s", tries[i], rest);
        if (!strcasecmp(c, cfg.app_path)) {
            snprintf(appPath, sizeof(appPath), "%s", c);
            return;
        }
    }
    for (i = 0; tries[i] && !found; i++) {
        snprintf(c, sizeof(c), "%s%s", tries[i], rest);
        found = device_has(c);
    }
    if (!found)   /* (not likely: it was started from there. The first one, then) */
        snprintf(c, sizeof(c), "%s%s", tries[0], rest);
    snprintf(appPath, sizeof(appPath), "%s", c);
    /* back to what reads the sd2psx, as when the program started */
    iop_reset();
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    fileXioExit();
    fileXioInit();
    SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
    SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, NULL);
}

static void started_elsewhere(const char *a0)
{
    const char *slash;
    /* started from a save folder of a memory card (mc0:/APP_SD2CLOUD/SD2CLOUD.ELF, the Save Application System
     * package): that folder is the program's place, with its IGR helper, and what an update is written to */
    if (!strncmp(appPath, "mc", 2) && (appPath[2] == '0' || appPath[2] == '1') && appPath[3] == ':') {
        const char *folder = appPath + 4 + (appPath[4] == '/'), *end = strchr(folder, '/');
        if (end && end > folder && !strchr(end + 1, '/') && (size_t)(end - folder) < sizeof(appCardDir) - 1) {
            appOnCard = 1;
            appCardPort = appPath[2] - '0';
            snprintf(appCardDir, sizeof(appCardDir), "/%.*s", (int)(end - folder), folder);
            return;
        }
    }
    appElsewhere = 1;
    if (strncmp(a0, "host:", 5) != 0)   /* (PCSX2 has none of those devices to try) */
        locate_self(appPath);
    slash = strrchr(appPath, '/');
    snprintf(appDir, sizeof(appDir), "%.*s", slash ? (int)(slash - appPath + 1) : 0, appPath);
}

void system_init(int argc, char *argv[])
{
    const char *a0 = (argc > 0 && argv[0]) ? argv[0] : "", *started = a0;
    const char *slash;
    int i;

    {
        ee_sema_t m;
        memset(&m, 0, sizeof(m));
        m.init_count = 1;
        m.max_count = 1;
        logSema = CreateSema(&m);
    }
    iop_reset();
    init_fileXio_driver();
    init_sio2man_driver();
    /* the mmceman must come from the same SDK as the sio2man (mixing them hangs the sd2psx bus) */
    SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, NULL);

    for (i = 1; i < argc; i++) {
        if (argv[i] && strcmp(argv[i], "-igr") == 0)
            igrMode = 1;
        if (argv[i] && strcmp(argv[i], "-handover") == 0)
            appTookOver = 1;
    }
#ifdef DEBUG_BUILD
    {   /* PCSX2 only starts programs from host: a started-from.txt in the data folder says which path to pretend it was
         * started from (not for the copy that takes over, which comes with one more argument) */
        static char pretend[200];
        buffer_t b = {0};
        if (!appTookOver && !strncmp(a0, "host:", 5) && file_read("host:SD2Cloud/started-from.txt", &b) == 0 && b.len) {
            snprintf(pretend, sizeof(pretend), "%s", (char *)b.data);
            trim(pretend);
            started = pretend;
        }
        buf_free(&b);
    }
#endif
    /* the app's folder is the ELF's (argv[0]); started without a path, the default one on the sd2psx */
    slash = strrchr(a0, '/');
    if (!slash)
        slash = strrchr(a0, ':');
    if (strncmp(a0, "host:", 5) == 0)
        strcpy(appDir, "host:APPS/SD2Cloud/");   /* testing on PCSX2: the root of host: stands in for the microSD */
    else if (slash && (size_t)(slash - a0 + 1) < sizeof(appDir))
        snprintf(appDir, sizeof(appDir), "%.*s", (int)(slash - a0 + 1), a0);
    else
        strcpy(appDir, "mmce0:/APPS/SD2Cloud/");
    /* the program itself, for the IGR helper to start it later: as it was started now, in whichever slot the sd2psx
     * is by then */
    snprintf(appPath, sizeof(appPath), "%s", started[0] ? started : "mmce?:/APPS/SD2Cloud/SD2CLOUD.ELF");
    if (!strncmp(appPath, "mmce", 4) && appPath[4] && appPath[5] == ':')
        appPath[4] = '?';
    if (strncmp(a0, "host:", 5) == 0)
        logFd = open("host:log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    find_sd(a0);
    find_device();
    config_read();
    if (strncmp(appPath, "mmce", 4) != 0 && strncmp(appPath, "host:", 5) != 0)
        started_elsewhere(a0);
    if (init_joystick_driver(false) == JOYSTICK_INIT_STATUS_OK)
        padOpen = padPortOpen(0, 0, padArea);
#ifdef DEBUG_BUILD
    script_read();
#endif
    log_msg(APP_NAME " " APP_VERSION " -- program in %s (%s), data in %s, %s%s%s%s", appDir, appPath, dataDir, igrMode ? "IGR" : "manual",
            appTookOver ? ", took over from a copy on another device" : "", appElsewhere ? ", on another device" : "",
            appOnCard ? ", on a memory card" : "");
#ifdef DEBUG_BUILD
    rescue_init();
#endif
}

/* Resets the IOP before handing over: the controller and the network write to EE memory by DMA and, if they stayed
 * on, they would write over the next program. With card = 1, only what reads the sd2psx comes back. */
void iop_cleanup(int card)
{
#ifdef DEBUG_BUILD
    rescue_stop();
    if (logRam.len) {
        char c[260];
        snprintf(c, sizeof(c), "%sdebug-log.txt", dataDir);
        file_write(c, logRam.data, logRam.len);
    }
#endif
    if (logFd >= 0) {
        close(logFd);
        logFd = -1;
    }
    if (padOpen) {
        padPortClose(0, 0);
        padEnd();
        padOpen = 0;
    }
    ui_end();
    sound_end();
    iop_reset();
    if (card) {
        SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
        SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
        SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
        SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, NULL);
    }
}

void go_osd(void)
{
    /* without arguments the OSDSYS goes through the memory card's "System Update" (FMCB/PS2BBL/OSDMenu) */
    log_msg("(end: OSD)");
    iop_cleanup(0);
    ExecOSD(0, NULL);
    for (;;)
        ;
}
