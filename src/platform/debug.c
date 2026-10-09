/* SD2Cloud -- what only the debug build has: the script that presses the buttons instead of a controller, the
 * screen captures, what it says of the memory, and the rescue that takes a console back when a test hangs. */
#include "platform.h"

#ifdef DEBUG_BUILD
/* Debug build: a script.txt in the data folder ("X O T Q R S L U D < >", T = triangle, Q = square, R = R1, S = START,
 * L = SELECT, U/D = up/down,
 * < > = left/right)
 * presses the buttons instead of someone holding the controller; "H" hands over to the real controller. "K" presses circle in the middle of an upload or
 * download (to test cancelling). "C" captures the screen that is waiting for a button to host:screen.tga
 * and STOPS there: the SDK's capture leaves PATH3 stuck on PCSX2 and gsKit draws nothing after it. "P" and "E" capture
 * the next progress screen (P = checking the cards, E = uploading or downloading, W = writing a restored card, halfway
 * through); an "I" at the start runs as IGR; an "A" right after the cards are checked shows the questions that
 * follow the first sign-in (the automatic sync).
 * Without a script it is the program as it is, with the real controller: only the log tells it apart.
 * "." waits a second on the screen it is on. Right after the cards are checked: "N" = the first try at the network
 * finds no cable, "n" = it gets no address from the router, "u" = the network starts as it used to, without the heap
 * it takes being zeroed (see __wrap_malloc). While a card file is installed: "z" = the file can't be read, halfway
 * through, once; "k" = circle halfway through the writing. "M" and "e" capture a new card being made and a card being
 * copied to a device, halfway through. "a" right after the cards are checked: the card that is the same as the one
 * in slot 1 is taken as the card in use (PCSX2 has no device to say so), and is changed through the slot. For the rescue (at the end of this file): "@<seconds>" at the very
 * start is the test's time limit, "Y" hangs and "Z" crashes, to try it. */
char script[256];
int scriptPos, hasScript;
int nRescue, rescueSeconds = 300, pastEnd;

int debug_has_script(void) { return hasScript; }

void script_read(void)
{
    char c[260];
    buffer_t b = {0};
    snprintf(c, sizeof(c), "%sscript.txt", dataDir);
    if (file_read(c, &b) == 0 && b.len) {
        const char *s = (char *)b.data;
        if (*s == '@') {
            rescueSeconds = atoi(s + 1);
            for (s++; *s >= '0' && *s <= '9'; s++)
                ;
        }
        snprintf(script, sizeof(script), "%s", s);
        hasScript = 1;
    }
    buf_free(&b);
}

void debug_capture_and_stop(void)
{
    if (logFd >= 0)
        ui_capture("host:screen.tga");
    log_msg("(end: capture)");
    for (;;)
        sleep_ms(1000);
}

/* is the next letter of the script this one? (consumes it) */
int debug_take(char mark)
{
    while (script[scriptPos] == ' ' || script[scriptPos] == '\r' || script[scriptPos] == '\n')
        scriptPos++;
    if (!hasScript || script[scriptPos] != mark)
        return 0;
    scriptPos++;
    return 1;
}

/* the next letter is a digit: consume it and return its value (-1 = not a digit) */
int debug_digit(void)
{
    char c = script[scriptPos];
    if (c < '0' || c > '9')
        return -1;
    scriptPos++;
    return c - '0';
}

void debug_capture_if(char mark)
{
    if (debug_take(mark))
        debug_capture_and_stop();
}

u32 script_next_button(u32 mask)
{
    sleep_ms(400);
    while (script[scriptPos]) {
        char ch = script[scriptPos++];
        u32 b = ch == 'X' ? PAD_CROSS : ch == 'O' ? PAD_CIRCLE : ch == 'T' ? PAD_TRIANGLE : ch == 'Q' ? PAD_SQUARE : ch == 'R' ? PAD_R1
              : ch == 'S' ? PAD_START : ch == 'L' ? PAD_SELECT : ch == 'U' ? PAD_UP : ch == 'D' ? PAD_DOWN : ch == '<' ? PAD_LEFT
              : ch == '>' ? PAD_RIGHT : 0;
        if (ch == 'C')
            debug_capture_and_stop();
        if (ch == 'H') {   /* hand over: from here on the real controller (to watch it on PCSX2) */
            log_msg("[script] H: the controller from now on");
            hasScript = 0;
            return 0;
        }
        if (ch == '.') {   /* a second on this screen, for whoever is watching */
            sleep_ms(1000);
            continue;
        }
        if (ch == 'Y') {
            log_msg("[script] Y: hanging");
            for (;;)
                ;
        }
        if (ch == 'Z') {
            log_msg("[script] Z: crashing");
            *(volatile int *)0x40000000 = 1;   /* nothing is mapped there */
        }
        if (b) {
            log_msg("[script] %c%s", ch, (b & mask) ? "" : " (not expected here)");
            return b;
        }
    }
    log_msg("[script] finished: O");
    if (nRescue && ++pastEnd > 8)   /* a few circles may still close what is open; after that nobody is coming */
        rescue("the script ended and the app is still open");
    return PAD_CIRCLE;
}
#endif

#ifdef DEBUG_BUILD
/* how much memory is in use: on the EE, where the heap ends now (it grows from the end of the program towards the
 * main thread's stack, at the top of the 32 MB) and how much of it is taken; on the IOP, the largest block its heap
 * still gives, by trying */
void debug_log_memory(const char *when)
{
    struct mallinfo m = mallinfo();
    int lo = 0, hi = 2048;
    while (hi - lo > 16) {
        int mid = (lo + hi) / 2;
        void *p = SifAllocIopHeap(mid * 1024);
        if (p) {
            SifFreeIopHeap(p);
            lo = mid;
        } else
            hi = mid;
    }
    log_msg("memory %s: EE heap ends at %08x (limit %08x), %d KB in use, %d KB free inside it; IOP largest free block %d KB", when,
            (unsigned)sbrk(0), (unsigned)EndOfHeap(), (int)(m.uordblks / 1024), (int)(m.fordblks / 1024), lo);
}
#endif

#ifdef DEBUG_BUILD
/* which of the IOP's services still answer, each asked from a thread of its own that is given 3 seconds (a call that
 * never comes back stays there: the test is over anyway once one is found dead) */
extern void *_gp;
volatile int askDone;
int askResult;
static char askStack[4][0x4000] __attribute__((aligned(16)));

void ask_netman(void *a) { (void)a; askResult = NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0); askDone = 1; ExitDeleteThread(); }
void ask_heap(void *a) { void *p = SifAllocIopHeap(64); (void)a; askResult = p != NULL; if (p) SifFreeIopHeap(p); askDone = 1; ExitDeleteThread(); }
void ask_files(void *a) { (void)a; askResult = fileXioDevctl("mmce0:", 0x3, NULL, 0, NULL, 0); askDone = 1; ExitDeleteThread(); }
void ask_sound(void *a) { (void)a; askResult = audsrv_adpcm_set_volume_and_pan(23, MAX_VOLUME, 0); askDone = 1; ExitDeleteThread(); }

void ask(const char *who, void (*f)(void *), int n)
{
    ee_thread_t th;
    ee_thread_status_t me;
    int id;
    u64 end = now_ms() + 3000;
    ReferThreadStatus(GetThreadId(), &me);
    memset(&th, 0, sizeof(th));
    th.func = f;
    th.stack = askStack[n];
    th.stack_size = sizeof(askStack[0]);
    th.gp_reg = &_gp;
    th.initial_priority = me.current_priority;
    askDone = 0;
    if ((id = CreateThread(&th)) < 0)
        return;
    StartThread(id, NULL);
    while (!askDone && now_ms() < end)
        sleep_ms(50);
    if (askDone)
        log_msg("IOP: %s answered (%d)", who, askResult);
    else
        log_msg("IOP: %s DID NOT ANSWER in 3 s", who);
}

/* every thread of the program: where it starts (addr2line names it), its priority (0 = the first to run) and what it
 * is doing (status 1 running, 2 ready, 4 waiting, 8 suspended, 16 dormant; waiting 1 = asleep, 2 = on a semaphore) */
void debug_log_threads(const char *when)
{
    int id;
    log_msg("EE threads %s:", when);
    for (id = 0; id < 256; id++) {
        ee_thread_status_t st;
        memset(&st, 0, sizeof(st));
        if (ReferThreadStatus(id, &st) < 0 || !st.func || st.status == 0 || st.status == 16)   /* 0: a slot left by an old one */
            continue;
        log_msg("  thread %d: starts at %x, priority %d, status %d, waiting %d on %d", id, (unsigned)st.func, st.current_priority, st.status,
                (int)st.waitType, (int)st.waitId);
    }
}

void debug_ask_iop(const char *when)
{
    debug_log_threads(when);
    log_msg("IOP services %s:", when);
    ask("its heap", ask_heap, 0);
    ask("the network driver", ask_netman, 1);
    ask("the sound driver", ask_sound, 2);
    ask("the sd2psx driver", ask_files, 3);
}
#endif

#ifdef DEBUG_BUILD
/* ------------------------------------------------------------ the rescue (debug builds, on a console)

   Tests on a console run with nobody at the controller, started by a launcher that takes commands over the network.
   rescue.txt in the data folder lists what to open (one path per line, the ones on the sd2psx first) when the test
   can't end by itself: the script ended with the app still open, its time ran out ("@<seconds>" at the start of the
   script, 300 without it) or the program crashed. The log is saved first. Without a rescue.txt (PCSX2) none of this
   exists. */

extern void *_gp;
char rescuePaths[4][200];
int mainThread = -1, watchThread = -1, watchAlarm = -1;
volatile int rescuing;
u64 rescueAt;
static char watchStack[0x8000] __attribute__((aligned(16)));
char debugCrashStack[0x8000] __attribute__((aligned(16)));
u32 debugCrashInfo[5];   /* cause, where, the address it tried, ra, sp */

void load_sd_drivers(int sd)
{
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
    if (sd) {
        SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
        SifExecModuleBuffer(mmceman_irx, size_mmceman_irx, 0, NULL, NULL);
    }
    fileXioExit();   /* the RPC binding is from before the reset: made again */
    fileXioInit();
}

void rescue(const char *why)
{
    static char c[260], part[80];
    int i, again = rescuing++;
    if (GetThreadId() != mainThread)
        SuspendThread(mainThread);
    logSema = -1;   /* whoever held it has been stopped */
    if (!again)
        log_msg("[rescue] %s", why);
    ui_abort();
    padOpen = 0;
    iop_reset();
    load_sd_drivers(1);
    if (!again && logRam.len) {
        snprintf(c, sizeof(c), "%sdebug-log.txt", dataDir);
        file_write(c, logRam.data, logRam.len);
    }
    for (i = 0; i < nRescue; i++) {
        char *q;
        int dev = device_of(rescuePaths[i]);
        snprintf(c, sizeof(c), "%s", rescuePaths[i]);
        if ((q = strstr(c, "mmce?:")) != NULL)
            q[4] = (strncmp(sdRoot, "mmce", 4) == 0) ? sdRoot[4] : '0';
        if (dev == DEV_SD) {
            if (file_exists(c))
                LoadELFFromFile(c, 0, NULL);   /* only comes back if it couldn't run it */
            continue;
        }
        iop_reset();
        load_sd_drivers(0);
        if (open_device(dev, c, sizeof(c), part, sizeof(part)) == 0) {
            if (part[0])
                LoadELFFromFileWithPartition(c, part, 0, NULL);
            else
                LoadELFFromFile(c, 0, NULL);
        }
    }
    ExecOSD(0, NULL);
    for (;;)
        ;
}

/* an exception: the kernel jumps here, still in its own mode. Note what happened and return from the exception into
 * debug_crash_entry, on a stack of its own (the one that crashed may be the problem) */
void debug_exception_stub(void);
void debug_crash_entry(void) __attribute__((noreturn, used));
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".set push\n"
    ".set noreorder\n"
    ".set noat\n"
    ".globl debug_exception_stub\n"
    ".ent debug_exception_stub\n"
    "debug_exception_stub:\n"
    "    la    $k0, debugCrashInfo\n"
    "    mfc0  $k1, $13\n"
    "    sw    $k1, 0($k0)\n"
    "    mfc0  $k1, $14\n"
    "    sw    $k1, 4($k0)\n"
    "    mfc0  $k1, $8\n"
    "    sw    $k1, 8($k0)\n"
    "    sw    $ra, 12($k0)\n"
    "    sw    $sp, 16($k0)\n"
    "    la    $sp, debugCrashStack + 0x7ff0\n"
    "    la    $k0, debug_crash_entry\n"
    "    mtc0  $k0, $14\n"
    "    sync.p\n"
    "    eret\n"
    "    nop\n"
    ".end debug_exception_stub\n"
    ".set pop\n"
);

void debug_crash_entry(void)
{
    static char t[160];
    EIntr();
    snprintf(t, sizeof(t), "crashed: exception %d at %08x, address %08x, ra %08x, sp %08x (thread %d, the main one is %d)",
             (int)((debugCrashInfo[0] >> 2) & 31), (unsigned)debugCrashInfo[1], (unsigned)debugCrashInfo[2], (unsigned)debugCrashInfo[3],
             (unsigned)debugCrashInfo[4], GetThreadId(), mainThread);
    rescue(t);
}

void on_watch_alarm(s32 id, u16 time, void *common)
{
    (void)id;
    (void)time;
    (void)common;
    if (rescuing)
        return;
    if (iGetTimerSystemTime() / (kBUSCLK / 1000) < rescueAt) {
        watchAlarm = iSetAlarm(kHBLNK_NTSC, on_watch_alarm, NULL);   /* about a second */
        return;
    }
    watchAlarm = -1;
    iSuspendThread(mainThread);
    iWakeupThread(watchThread);
}

/* where a stopped thread was: the return addresses left on its stack (the words that point right after a call), from
 * the deepest call up. The ones below where the thread is now are leftovers of earlier calls; the unpacked ELF and
 * addr2line turn the list into function names */
extern char _ftext[], _etext[];

void log_stack(int thread)
{
    static char t[200];
    ee_thread_status_t st;
    u32 *p, *end;
    int n = 0;
    memset(&st, 0, sizeof(st));
    if (ReferThreadStatus(thread, &st) < 0 || !st.stack || st.stack_size <= 0)
        return;
    end = (u32 *)((char *)st.stack + st.stack_size);
    t[0] = 0;
    for (p = (u32 *)st.stack; p < end && n < 60; p++) {
        u32 w = *p, op;
        if (w < (u32)_ftext + 8 || w >= (u32)_etext || (w & 3))
            continue;
        op = *(u32 *)(w - 8);
        if ((op & 0xFC000000) != 0x0C000000 && (op & 0xFC00003F) != 0x00000009)   /* a jal or a jalr */
            continue;
        snprintf(t + strlen(t), sizeof(t) - strlen(t), " %x", (unsigned)w);
        if (++n % 12 == 0) {
            log_msg("[rescue] stack of thread %d:%s", thread, t);
            t[0] = 0;
        }
    }
    if (t[0])
        log_msg("[rescue] stack of thread %d:%s", thread, t);
}

void watch_loop(void *arg)
{
    static char t[160];
    ee_thread_status_t st;
    ee_sema_t sema;
    (void)arg;
    SleepThread();
    memset(&st, 0, sizeof(st));
    ReferThreadStatus(mainThread, &st);
    log_msg("[rescue] the log's semaphore is %d", logSema);
    logSema = -1;
    memset(&sema, 0, sizeof(sema));
    if (st.waitType == 2 && ReferSemaStatus(st.waitId, &sema) >= 0)
        log_msg("[rescue] semaphore %d: count %d of %d, %d waiting", (int)st.waitId, sema.count, sema.max_count, sema.wait_threads);
    log_stack(mainThread);
    snprintf(t, sizeof(t), "the time ran out (%d s); main thread: status %d, waiting %d on %d", rescueSeconds, st.status, st.waitType,
             st.waitId);
    rescue(t);
}

void rescue_init(void)
{
    char c[260], *line;
    buffer_t b = {0};
    ee_thread_t th;
    int i;
    probes_read();
    snprintf(c, sizeof(c), "%srescue.txt", dataDir);
    if (file_read(c, &b) == 0 && b.len)
        for (line = strtok((char *)b.data, "\r\n"); line && nRescue < 4; line = strtok(NULL, "\r\n")) {
            snprintf(rescuePaths[nRescue], sizeof(rescuePaths[0]), "%s", line);
            trim(rescuePaths[nRescue]);
            if (rescuePaths[nRescue][0])
                nRescue++;
        }
    buf_free(&b);
    if (!nRescue)
        return;
    mainThread = GetThreadId();
    for (i = 1; i <= 3; i++)
        SetVTLBRefillHandler(i, debug_exception_stub);
    for (i = 4; i <= 7; i++)
        SetVCommonHandler(i, debug_exception_stub);
    for (i = 10; i <= 13; i++)
        SetVCommonHandler(i, debug_exception_stub);
    memset(&th, 0, sizeof(th));
    th.func = watch_loop;
    th.stack = watchStack;
    th.stack_size = sizeof(watchStack);
    th.gp_reg = &_gp;
    th.initial_priority = 0;
    watchThread = CreateThread(&th);
    StartThread(watchThread, NULL);
    rescueAt = now_ms() + (u64)rescueSeconds * 1000;
    watchAlarm = SetAlarm(kHBLNK_NTSC, on_watch_alarm, NULL);
    log_msg("[rescue] ready: %d s, then %s", rescueSeconds, rescuePaths[0]);
}

/* probe.txt in the data folder, one device per line ("mass:/", "mx4sio:/", "ata:/", "hdd0:__common:pfs:/"): right
 * before leaving, the drivers of each one are loaded the way run_elf does and what is in its root goes to the log.
 * It tries the devices a console has without writing to them and without the test losing the console. */
char probes[6][80];
int nProbes, probing;

void list_to_log(const char *dir)
{
    char t[400] = "";
    struct dirent *e = NULL;
    DIR *d = opendir(dir);
    int n = 0;
    if (!d) {
        log_msg("[probe]   %s can't be opened", dir);
        return;
    }
    while (n < 14 && (e = readdir(d)) != NULL) {
        snprintf(t + strlen(t), sizeof(t) - strlen(t), "%s%s", n ? ", " : "", e->d_name);
        n++;
    }
    closedir(d);
    log_msg("[probe]   %s has: %s%s", dir, n ? t : "nothing", n == 14 ? ", ..." : "");
}

void run_probes(void)
{
    static char c[260], part[80];
    int i;
    if (!nProbes)
        return;
    probing = 1;   /* the rescue stays on watch: a driver may hang */
    iop_cleanup(0);
    for (i = 0; i < nProbes; i++) {
        int dev = device_of(probes[i]), r;
        u64 t0 = now_ms();
        iop_reset();
        load_sd_drivers(0);
        snprintf(c, sizeof(c), "%s", probes[i]);
        log_msg("[probe] %s: loading its drivers", probes[i]);   /* in the log even if the rescue has to step in */
        r = open_device(dev, c, sizeof(c), part, sizeof(part));
        log_msg("[probe] %s: %d ms%s", probes[i], (int)(now_ms() - t0), r == 0 ? ", the path is there" : "");
        if (dev == DEV_HDD) {
            list_to_log("hdd0:");
            list_to_log("pfs0:/");
        } else if (dev == DEV_MC)
            list_to_log("mc0:/");
        else
            list_to_log("mass0:/");
    }
    nProbes = probing = 0;
    iop_reset();   /* back to the sd2psx alone: the log is written again on the way out */
    load_sd_drivers(1);
}

void probes_read(void)
{
    char c[260], *line;
    buffer_t b = {0};
    snprintf(c, sizeof(c), "%sprobe.txt", dataDir);
    if (file_read(c, &b) == 0 && b.len)
        for (line = strtok((char *)b.data, "\r\n"); line && nProbes < 6; line = strtok(NULL, "\r\n")) {
            snprintf(probes[nProbes], sizeof(probes[0]), "%s", line);
            trim(probes[nProbes]);
            if (probes[nProbes][0])
                nProbes++;
        }
    buf_free(&b);
}

/* leaving the normal way: the alarm must not ring inside the next program */
void rescue_stop(void)
{
    if (!nRescue || rescuing || probing)
        return;
    rescuing = 1;
    if (watchAlarm >= 0)
        ReleaseAlarm(watchAlarm);
    watchAlarm = -1;
}
#endif
