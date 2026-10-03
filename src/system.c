/*
 * SD2Cloud base: IOP modules, controller, clock, network, log and launching another ELF.
 * On the sd2psx everything is done one thing at a time: it can't handle two operations at once, and an open that
 * fails while another file is open breaks the other file's descriptor (firmware 1.4.0, sd2psXtd#115).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <kernel.h>
#include <timer.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <elf-loader.h>
#include <strings.h>
#include <libcdvd.h>
#include <osd_config.h>
/* only for fileXioDevctl (the MMCE commands), which doesn't touch newlib's file descriptors */
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <ps2_fileXio_driver.h>
#include <ps2_sio2man_driver.h>
#include <ps2_joystick_driver.h>
#include <ps2_network_driver.h>
#include <netman.h>
#include <ps2ip.h>
#include "common.h"
#include "ui.h"
#include "sound.h"

extern unsigned char mcman_irx[];
extern unsigned int size_mcman_irx;
extern unsigned char mcserv_irx[];
extern unsigned int size_mcserv_irx;
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbmass_bd_irx[];
extern unsigned int size_usbmass_bd_irx;
extern unsigned char bdm_irx[];
extern unsigned int size_bdm_irx;
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char mx4sio_bd_irx[];
extern unsigned int size_mx4sio_bd_irx;
extern unsigned char ata_bd_irx[];
extern unsigned int size_ata_bd_irx;
extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;
extern unsigned char ps2atad_irx[];
extern unsigned int size_ps2atad_irx;
extern unsigned char ps2hdd_irx[];
extern unsigned int size_ps2hdd_irx;
extern unsigned char ps2fs_irx[];
extern unsigned int size_ps2fs_irx;
extern unsigned char mmceman_irx[];
extern unsigned int size_mmceman_irx;
extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;

char appDir[200];
char appPath[260];
int appElsewhere;
int appTookOver;
char sdRoot[16];
char dataDir[32];
int igrMode;

static char padArea[256] __attribute__((aligned(64)));
static int padOpen;
static int logFd = -1;          /* host:log.txt, only when testing on PCSX2 */
#ifdef TEST
static buffer_t logRam;         /* on the console the log goes to the card only at the end, in a single file */
#endif

/* ------------------------------------------------------------ log */

static int logSema = -1;          /* the log is written from more than one thread */

/* is anyone keeping the log? On the console only the test build does: the release doesn't even format the lines */
static int log_kept(void)
{
#ifdef TEST
    return 1;
#else
    return logFd >= 0;
#endif
}

void log_raw(const char *d, size_t n)
{
    if (!log_kept())
        return;
    if (logSema >= 0)
        WaitSema(logSema);
    if (logFd >= 0)
        write(logFd, d, n);
#ifdef TEST
    if (logRam.len < 4 * 1024 * 1024)
        buf_append(&logRam, d, n);
#endif
    if (logSema >= 0)
        SignalSema(logSema);
}

void log_msg(const char *fmt, ...)
{
    va_list ap;
    char t[600];
    if (!log_kept())
        return;
    va_start(ap, fmt);
    vsnprintf(t, sizeof(t), fmt, ap);
    va_end(ap);
    log_raw(t, strlen(t));
    log_raw("\r\n", 2);
}

/* ------------------------------------------------------------ time and controller */

u64 now_ms(void) { return GetTimerSystemTime() / (kBUSCLK / 1000); }   /* the SDK's clock() wraps around in ~14 s */

void sleep_ms(int ms) { usleep(ms * 1000); }

u32 pad_buttons(void)
{
    struct padButtonStatus b;
    int st;
    if (!padOpen)
        return 0;
    st = padGetState(0, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1)
        return 0;
    if (padRead(0, 0, &b) == 0)
        return 0;
    return 0xffff ^ b.btns;
}

#ifdef TEST
/* Test build: a script.txt in the data folder ("X O T Q R S U D < >", T = triangle, Q = square, R = R1, S = START,
 * U/D = up/down,
 * < > = left/right)
 * presses the buttons instead of someone holding the controller; "H" hands over to the real controller. "K" presses circle in the middle of an upload or
 * download (to test cancelling). "C" captures the screen that is waiting for a button to host:screen.tga
 * and STOPS there: the SDK's capture leaves PATH3 stuck on PCSX2 and gsKit draws nothing after it. "P" and "E" capture
 * the next progress screen (P = checking the cards, E = uploading or downloading, W = writing a restored card, halfway
 * through); an "I" at the start runs as IGR; an "A" right after the cards are checked shows the questions that
 * follow the first sign-in (the automatic sync).
 * Without a script, the test also runs as IGR (main.c). */
static char script[256];
static int scriptPos, hasScript;

int test_has_script(void) { return hasScript; }

static void script_read(void)
{
    char c[260];
    buffer_t b = {0};
    snprintf(c, sizeof(c), "%sscript.txt", dataDir);
    if (file_read(c, &b) == 0 && b.len) {
        snprintf(script, sizeof(script), "%s", (char *)b.data);
        hasScript = 1;
    }
    buf_free(&b);
}

void test_capture_and_stop(void)
{
    if (logFd >= 0)
        ui_capture("host:screen.tga");
    log_msg("(end: capture)");
    for (;;)
        sleep_ms(1000);
}

/* is the next letter of the script this one? (consumes it) */
int test_take(char mark)
{
    while (script[scriptPos] == ' ' || script[scriptPos] == '\r' || script[scriptPos] == '\n')
        scriptPos++;
    if (!hasScript || script[scriptPos] != mark)
        return 0;
    scriptPos++;
    return 1;
}

/* the next letter is a digit: consume it and return its value (-1 = not a digit) */
int test_digit(void)
{
    char c = script[scriptPos];
    if (c < '0' || c > '9')
        return -1;
    scriptPos++;
    return c - '0';
}

void test_capture_if(char mark)
{
    if (test_take(mark))
        test_capture_and_stop();
}

static u32 script_next_button(u32 mask)
{
    sleep_ms(400);
    while (script[scriptPos]) {
        char ch = script[scriptPos++];
        u32 b = ch == 'X' ? PAD_CROSS : ch == 'O' ? PAD_CIRCLE : ch == 'T' ? PAD_TRIANGLE : ch == 'Q' ? PAD_SQUARE : ch == 'R' ? PAD_R1
              : ch == 'S' ? PAD_START : ch == 'U' ? PAD_UP : ch == 'D' ? PAD_DOWN : ch == '<' ? PAD_LEFT : ch == '>' ? PAD_RIGHT : 0;
        if (ch == 'C')
            test_capture_and_stop();
        if (ch == 'H') {   /* hand over: from here on the real controller (to watch it on PCSX2) */
            log_msg("[script] H: the controller from now on");
            hasScript = 0;
            return 0;
        }
        if (b) {
            log_msg("[script] %c%s", ch, (b & mask) ? "" : " (not expected here)");
            return b;
        }
    }
    log_msg("[script] finished: O");
    return PAD_CIRCLE;
}
#endif

u32 wait_button(u32 mask, int seconds)
{
    u64 end = now_ms() + (u64)seconds * 1000, release = now_ms() + 2000;
#ifdef TEST
    if (hasScript && seconds == 0) {
        u32 b = script_next_button(mask);
        if (b)
            return b;
    }
#endif
    while ((pad_buttons() & mask) && now_ms() < release)
        sleep_ms(20);
    for (;;) {
        u32 b = pad_buttons() & mask;
        if (b)
            return b;
        if (seconds > 0 && now_ms() >= end)
            return 0;
        sleep_ms(20);
    }
}

/* for lists: a new press returns at once; the arrows held repeat (after 400 ms, then every 100 ms). A button that is
 * already held when the screen opens (the X that confirmed the previous screen) only counts after it's released.
 * ms >= 0: gives up after that long and returns 0 (the screen has something to do in the background meanwhile) */
#define ARROWS (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)
u32 wait_nav_ms(u32 mask, int ms)
{
    static u32 held;
    static u64 next;
    u32 now = pad_buttons() & mask;
    u64 end = now_ms() + (ms > 0 ? ms : 0);
#ifdef TEST
    if (hasScript) {
        u32 b;
        if (ms >= 0)
            return 0;   /* with a script, the background work goes first: the presses come when the screen just waits */
        if ((b = script_next_button(mask)) != 0)
            return b;
    }
#endif
    held = (held & now) | (now & ~ARROWS);
    for (;;) {
        u32 b = pad_buttons() & mask, fresh = b & ~held;
        u64 t = now_ms();
        if (fresh) {
            held = b;
            next = t + 400;
            return fresh;
        }
        if ((b & held & ARROWS) && t >= next) {
            held = b;
            next = t + 100;
            return b & ARROWS;
        }
        held = b;
        if (ms >= 0 && t >= end)
            return 0;
        sleep_ms(16);
    }
}

u32 wait_nav(u32 mask) { return wait_nav_ms(mask, -1); }

/* the PS2 clock keeps Japan time; convert it with the time zone set on the console */
void local_time(datetime_t *t)
{
    sceCdCLOCK c;
    t->year = 2000;
    t->month = t->day = 1;
    t->hour = t->minute = t->second = 0;
    if (sceCdInit(SCECdINoD) && sceCdReadClock(&c)) {
#define BCD(x) (((x) >> 4) * 10 + ((x) & 0xF))
        configConvertToLocalTime(&c);
        t->year = 2000 + BCD(c.year);
        t->month = BCD(c.month & 0x1F);
        t->day = BCD(c.day);
        t->hour = BCD(c.hour);
        t->minute = BCD(c.minute);
        t->second = BCD(c.second);
#undef BCD
    }
}

void card_time_local(int year, int month, int day, int hour, int minute, int second, datetime_t *t)
{
    sceCdCLOCK c;
#define TOBCD(x) ((((x) / 10) << 4) | ((x) % 10))
#define BCD(x) (((x) >> 4) * 10 + ((x) & 0xF))
    memset(&c, 0, sizeof(c));
    c.second = TOBCD(second), c.minute = TOBCD(minute), c.hour = TOBCD(hour);
    c.day = TOBCD(day), c.month = TOBCD(month), c.year = TOBCD(year % 100);
    configConvertToLocalTime(&c);
    t->year = 2000 + BCD(c.year);
    t->month = BCD(c.month & 0x1F);
    t->day = BCD(c.day);
    t->hour = BCD(c.hour);
    t->minute = BCD(c.minute);
    t->second = BCD(c.second);
#undef TOBCD
#undef BCD
}

/* ------------------------------------------------------------ network */

/* The drivers and the IP stack start once: starting lwIP a second time hangs the PS2 (that's what happened when the
 * first try failed for lack of a cable and a backup tried again). Every later call only waits for the cable and the
 * router again. A plugged cable has its link in 1-3 s, so 5 s without it = no cable; the router gets its own time. */
static int netStarted;
#ifdef TEST
int testNoLinkOnce;   /* script "N": the first try finds no cable (PCSX2 can't unplug one) */
#endif

static int link_up(void)
{
#ifdef TEST
    if (testNoLinkOnce)
        return 0;
#endif
    return NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP;
}

static int dhcp_bound(void)
{
    t_ip_info i;
    if (ps2ip_getconfig("sm0", &i) < 0 || !i.dhcp_enabled)
        return 0;
    return i.dhcp_status == DHCP_STATE_BOUND;
}

static int wait_for(int (*check)(void), int ms)
{
    u64 end = now_ms() + ms;
    while (!check()) {
        if (now_ms() >= end)
            return -1;
        sleep_ms(200);
    }
    return 0;
}

/* asks the router for an address (again: after a cable comes back, lwIP's DHCP may be waiting a long while between
 * its tries, so it's restarted) */
static int start_dhcp(int restart)
{
    t_ip_info i;
    if (ps2ip_getconfig("sm0", &i) < 0)
        return -1;
    if (restart && i.dhcp_enabled) {
        i.dhcp_enabled = 0;
        ps2ip_setconfig(&i);
    }
    i.dhcp_enabled = 1;
    return ps2ip_setconfig(&i) < 0 ? -1 : 0;
}

int network_up(void)
{
    struct ip4_addr ip, nm, gw;
    int fresh = !netStarted;
    u64 t0 = now_ms();
    if (!netStarted) {
        if (init_network_driver(true) != EEIP_INIT_STATUS_OK)
            return T_NET_ERR_DRIVERS;
        ip4_addr_set_zero(&ip);
        ip4_addr_set_zero(&nm);
        ip4_addr_set_zero(&gw);
        ps2ipInit(&ip, &nm, &gw);
        if (start_dhcp(0) != 0)
            return T_NET_ERR_DRIVERS;
        netStarted = 1;
    }
    if (wait_for(link_up, 5000) != 0) {
        log_msg("network: no link (cable?) after %d ms", (int)(now_ms() - t0));
#ifdef TEST
        testNoLinkOnce = 0;
#endif
        return T_NET_ERR_LINK;
    }
    if (!dhcp_bound()) {
        if (!fresh)
            start_dhcp(1);
        if (wait_for(dhcp_bound, igrMode ? 12000 : 20000) != 0) {
            log_msg("network: link up, but no address from the router");
            return T_NET_ERR_DHCP;
        }
    }
    eeip_get_current_config(&ip, &nm, &gw);
    log_msg("network ok: %d.%d.%d.%d (%d ms%s)", ip4_addr1(&ip), ip4_addr2(&ip), ip4_addr3(&ip), ip4_addr4(&ip), (int)(now_ms() - t0),
            fresh ? "" : ", tried again");
    return 0;
}

/* ------------------------------------------------------------ sd2psx (MMCE commands, mmceman's devctl) */

#define MMCE_CMD_GET_CARD    0x3
#define MMCE_CMD_GET_CHANNEL 0x5

int mmce_active_card(int *channel)
{
    char dev[8];
    int card, ch;
    if (strncmp(sdRoot, "mmce", 4) != 0)
        return -2;
    snprintf(dev, sizeof(dev), "%.6s", sdRoot);   /* "mmce0:" */
    card = fileXioDevctl(dev, MMCE_CMD_GET_CARD, NULL, 0, NULL, 0);
    ch = fileXioDevctl(dev, MMCE_CMD_GET_CHANNEL, NULL, 0, NULL, 0);
    log_msg("sd2psx: card %d, channel %d", card, ch);
    if (card < 0 || ch < 0)
        return -1;
    *channel = ch;
    return card;
}

/* ------------------------------------------------------------ start and leave */

static void iop_reset(void)
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

static int has_dir(const char *root, const char *dir)
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
        strcpy(sdRoot, options[i < 2 ? i : 0]);
    }
    snprintf(dataDir, sizeof(dataDir), "%sSD2Cloud/", sdRoot);
}

/* Started from a device other than the sd2psx: the Apps list of an OPL that doesn't read the sd2psx, with the SD2Cloud
 * folder copied to the USB drive or the MX4SIO card (or anywhere else that OPL lists). The SD2Cloud installed on the
 * microSD takes over, so there is a single place for the program, its IGR helper and its updates. Without one there,
 * this copy goes on by itself (it needs nothing from its own folder to work), and what it installs later, an update,
 * goes to the microSD */
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
#ifdef TEST
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
    config_read();
    if (strncmp(appPath, "mmce", 4) != 0 && strncmp(appPath, "host:", 5) != 0)
        hand_over(argc, argv);
    if (init_joystick_driver(false) == JOYSTICK_INIT_STATUS_OK)
        padOpen = padPortOpen(0, 0, padArea);
#ifdef TEST
    script_read();
#endif
    log_msg(APP_NAME " " APP_VERSION " -- program in %s (%s), data in %s, %s%s%s", appDir, appPath, dataDir, igrMode ? "IGR" : "manual",
            appTookOver ? ", took over from a copy on another device" : "", appElsewhere ? ", started from another device" : "");
}

/* Resets the IOP before handing over: the controller and the network write to EE memory by DMA and, if they stayed
 * on, they would write over the next program. With card = 1, only what reads the sd2psx comes back. */
static void iop_cleanup(int card)
{
#ifdef TEST
    if (logRam.len) {
        char c[260];
        snprintf(c, sizeof(c), "%stest-log.txt", dataDir);
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

/* ------------------------------------------------------------ the program opened after SD2Cloud

   Where it is, the device first (the notation PS2 homebrew launchers use):
     mmce?:/... mmce0:/... mmce1:/...   the sd2psx (the drivers SD2Cloud already uses)
     mc?:/... mc0:/... mc1:/...         a memory card
     mass:/... usb:/...                 a USB drive
     mx4sio:/...                        an MX4SIO
     ata:/...                           the internal HDD formatted exFAT
     hdd0:PARTITION:pfs:/...            the internal HDD formatted APA (also hdd0:PARTITION/...)
   Only the drivers of that device are loaded, after the IOP reset and only at that moment: the app itself, and IGR,
   never pay for the others. */

static int starts(const char *s, const char *prefix) { return strncasecmp(s, prefix, strlen(prefix)) == 0; }

int device_of(const char *path)
{
    if (starts(path, "mc?:") || starts(path, "mc0:") || starts(path, "mc1:"))
        return DEV_MC;
    if (starts(path, "mass") || starts(path, "usb"))
        return DEV_USB;
    if (starts(path, "mx4sio"))
        return DEV_MX4SIO;
    if (starts(path, "ata"))
        return DEV_ATA;
    if (starts(path, "hdd"))
        return DEV_HDD;
    return DEV_SD;   /* mmce, host: on PCSX2, and anything else */
}

static void load_irx(const void *irx, unsigned int size, int argLen, const char *args)
{
    int ret = 0, id = SifExecModuleBuffer((void *)irx, size, argLen, args, &ret);
    if (id < 0 || ret == 1)   /* the log is closed by now: only for whoever runs it on PCSX2 with a debugger */
        printf("SD2Cloud: module not loaded (%d, %d)\n", id, ret);
}

/* waits for a file to appear (USB, MX4SIO and the HDD take a moment to be ready); 0 = it's there */
static int wait_for_file(const char *path, int ms)
{
    int t;
    for (t = 0; t <= ms; t += 100) {
        if (file_exists(path))
            return 0;
        sleep_ms(100);
    }
    return -1;
}

/* loads the drivers of the program's device and turns its path into one the IOP can read (in path); for the APA HDD
 * also the partition, as the next program expects to see it in argv[0] ("hdd0:__common:"). 0 = the file is there */
static int open_device(int dev, char *path, size_t size, char *part, size_t partSize)
{
    static const char hddArgs[] = "-o\0" "4\0" "-n\0" "20";
    static const char pfsArgs[] = "-m\0" "4\0" "-o\0" "10\0" "-n\0" "40";
    char rest[200], *colon = strchr(path, ':');
    snprintf(rest, sizeof(rest), "%s", colon ? colon + 1 : path);
    part[0] = 0;
    switch (dev) {
    case DEV_MC:
        load_irx(sio2man_irx, size_sio2man_irx, 0, NULL);
        load_irx(mcman_irx, size_mcman_irx, 0, NULL);
        load_irx(mcserv_irx, size_mcserv_irx, 0, NULL);
        if (path[2] == '?') {   /* mc?: = the first slot that has it */
            path[2] = '0';
            if (wait_for_file(path, 0) == 0)
                return 0;
            path[2] = '1';
        }
        return wait_for_file(path, 1000);
    case DEV_USB:
    case DEV_MX4SIO:
    case DEV_ATA:
        if (dev == DEV_ATA)
            load_irx(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
        if (dev == DEV_MX4SIO)
            load_irx(sio2man_irx, size_sio2man_irx, 0, NULL);
        if (dev == DEV_USB)
            load_irx(usbd_irx, size_usbd_irx, 0, NULL);
        load_irx(bdm_irx, size_bdm_irx, 0, NULL);
        load_irx(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL);
        if (dev == DEV_USB)
            load_irx(usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL);
        else if (dev == DEV_MX4SIO)
            load_irx(mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL);
        else
            load_irx(ata_bd_irx, size_ata_bd_irx, 0, NULL);
        snprintf(path, size, "mass0:%s", rest);   /* the only block device loaded: the first one */
        return wait_for_file(path, 6000);
    default: {   /* DEV_HDD: hdd0:PARTITION:pfs:/path or hdd0:PARTITION/path */
        char *p = rest, *end = p + strcspn(p, ":/");
        int t;
        if (!*end)
            return -1;
        snprintf(part, partSize, "hdd0:%.*s:", (int)(end - p), p);
        p = (*end == ':' && starts(end, ":pfs:")) ? end + 5 : end;
        load_irx(ps2dev9_irx, size_ps2dev9_irx, 0, NULL);
        load_irx(ps2atad_irx, size_ps2atad_irx, 0, NULL);
        load_irx(ps2hdd_irx, size_ps2hdd_irx, sizeof(hddArgs), hddArgs);
        load_irx(ps2fs_irx, size_ps2fs_irx, sizeof(pfsArgs), pfsArgs);
        for (t = 0; t < 30; t++) {   /* the drive takes a moment to answer */
            char block[80];
            snprintf(block, sizeof(block), "%.*s", (int)strlen(part) - 1, part);   /* without the last ':' */
            if (fileXioMount("pfs0:", block, FIO_MT_RDONLY) >= 0)
                break;
            sleep_ms(100);
        }
        snprintf(path, size, "pfs0:%s%s", *p == '/' ? "" : "/", p);
        if (wait_for_file(path, 0) != 0)
            return -1;
        snprintf(path, size, "pfs:%s%s", *p == '/' ? "" : "/", p);   /* how the ELF loader wants it */
        return 0;
    }
    }
}

void run_elf(const char *path)
{
    static char c[260], part[80];
    char *q;
    int dev = device_of(path);
    snprintf(c, sizeof(c), "%s", path);
    /* mmce?: becomes the microSD's slot */
    if ((q = strstr(c, "mmce?:")) != NULL)
        q[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
    log_msg("(end: running %s)", c);
    if (dev == DEV_SD) {
        iop_cleanup(1);
        LoadELFFromFile(c, 0, NULL);
    } else {
        iop_cleanup(0);
        SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
        SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
        fileXioExit();   /* the RPC binding is from before the reset: made again */
        fileXioInit();
        if (open_device(dev, c, sizeof(c), part, sizeof(part)) == 0) {
            if (part[0])
                LoadELFFromFileWithPartition(c, part, 0, NULL);
            else
                LoadELFFromFile(c, 0, NULL);
        }
    }
    /* only comes back here if it couldn't run it: the PS2 menu */
    ExecOSD(0, NULL);
    for (;;)
        ;
}
