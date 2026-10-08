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

/* Started from a device other than the sd2psx: the Apps list of an OPL that doesn't read the sd2psx, with the SD2Cloud
 * folder copied to the USB drive or the MX4SIO card (or anywhere else that OPL lists). The SD2Cloud installed on the
 * microSD takes over, so there is a single place for the program, its IGR helper and its updates. Without one there,
 * this copy goes on by itself (it needs nothing from its own folder to work), and what it installs later, an update,
 * goes to the microSD; unless it was started from a memory card, where it stays */
static void hand_over(int argc, char *argv[])
{
    static char *args[8];
    char c[260], *q;
    const char *name;
    int n = 0, i;
    /* at IGR what matters is being quick: when the one on the microSD is this same version (it says so in the
     * settings), loading it would only take time */
    if (igrMode && !strcmp(cfg.app_version, APP_VERSION)) {
        appElsewhere = 1;
        snprintf(appDir, sizeof(appDir), "%sAPPS/SD2Cloud/", sdRoot);
        return;
    }
    snprintf(c, sizeof(c), "%s", cfg.app_path);   /* where the one on the microSD said it is */
    if ((q = strstr(c, "mmce?:")) != NULL)
        q[4] = strncmp(sdRoot, "mmce", 4) == 0 ? sdRoot[4] : '0';
    if (!file_exists(c)) {   /* its usual folder, under the name this copy has */
        name = strrchr(appPath, '/') ? strrchr(appPath, '/') + 1 : strrchr(appPath, ':') ? strrchr(appPath, ':') + 1 : appPath;
        snprintf(c, sizeof(c), "%sAPPS/SD2Cloud/%s", sdRoot, name);
    }
    if (file_exists(c)) {
        for (i = 1; i < argc && n < 6; i++)
            args[n++] = argv[i];
        args[n++] = "-handover";
        log_msg("started from %s: handing over to %s", appPath, c);
        if (logFd >= 0) {
            close(logFd);
            logFd = -1;
        }
        LoadELFFromFile(c, n, args);   /* only comes back if it couldn't run it */
    }
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
    snprintf(appDir, sizeof(appDir), "%sAPPS/SD2Cloud/", sdRoot);
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
        hand_over(argc, argv);
    if (init_joystick_driver(false) == JOYSTICK_INIT_STATUS_OK)
        padOpen = padPortOpen(0, 0, padArea);
#ifdef DEBUG_BUILD
    script_read();
#endif
    log_msg(APP_NAME " " APP_VERSION " -- program in %s (%s), data in %s, %s%s%s%s", appDir, appPath, dataDir, igrMode ? "IGR" : "manual",
            appTookOver ? ", took over from a copy on another device" : "", appElsewhere ? ", started from another device" : "",
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
