/* SD2Cloud -- the clock and the controller: the time, the buttons held, and waiting for one to be pressed. */
#include "platform.h"

char padArea[256] __attribute__((aligned(64)));
int padOpen;

/* ------------------------------------------------------------ time and controller */

u64 now_ms(void) { return GetTimerSystemTime() / (kBUSCLK / 1000); }   /* the SDK's clock() wraps around in ~14 s */

void sleep_ms(int ms) { usleep(ms * 1000); }

u32 pad_buttons(void)
{
    static u64 lost;   /* since when the controller hasn't answered (0 = it answers) */
    struct padButtonStatus b;
    int st;
    if (!padOpen)
        return 0;
    st = padGetState(0, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) {
        /* The controller and the memory card in slot 1 share the PS2's port: a device that drops its card for a moment
         * left the controller without answering for most of a minute (an sd2psx, 10/2026). After 3 s of that, the
         * controller's port is opened again, and again 3 s later if it still doesn't answer */
        u64 now = now_ms();
        if (!lost)
            lost = now;
        else if (now - lost > 3000) {
            padPortClose(0, 0);
            log_msg("controller: no answer for 3 s (state %d), its port opened again (%d)", st, padPortOpen(0, 0, padArea));
            lost = now;
        }
        return 0;
    }
    lost = 0;
    if (padRead(0, 0, &b) == 0)
        return 0;
    return 0xffff ^ b.btns;
}

void (*idleHook)(void);

u32 wait_button(u32 mask, int seconds)
{
    u64 end = now_ms() + (u64)seconds * 1000, release = now_ms() + 2000;
#ifdef DEBUG_BUILD
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
        if (idleHook)
            idleHook();
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
#ifdef DEBUG_BUILD
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
        if (idleHook)
            idleHook();
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
