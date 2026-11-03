/* SD2Cloud -- at IGR, triangle: what comes after it, this time only. The program in the settings, another one of
 * APPS, the PS2 Browser, or the PS2 turned off; when the sync ends, and when it is cancelled. The list is looked at
 * from watch_cancel, while what is under way goes on: it takes the screen's place while it is open (ui_cover), and
 * once something else is picked the header says what (ui_over). */
#include "app.h"

#define NEXT_MAX (MAX_APPS + 3)
#define NEXT_ROWS 6
#define NEXT_W 380
#define NEXT_KEYS (PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE)

static struct {
    int open;           /* the list is on the screen */
    int letGo;          /* closed: its buttons aren't anyone's until they are let go of */
    int n, cursor, top;
    int chosen;         /* the one picked (-1 = the settings' one: nothing was) */
    u32 down;           /* the buttons down at the last look */
    u64 since;          /* when the list was last pressed on (it isn't waited for forever at the end) */
    char items[NEXT_MAX][100];
    char values[NEXT_MAX][200];
} nx = {.chosen = -1};

static void scene_next(float t)
{
    int rows = nx.n < NEXT_ROWS ? nx.n : NEXT_ROWS, h = 56 + rows * 32 + 24, x = (W - NEXT_W) / 2,
        y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, NEXT_W, h);
    look_title(W / 2.0f, y + 20, T(T_IGR_AFTER), 1);
    for (i = nx.top; i < nx.n && i < nx.top + rows; i++)
        look_item(W / 2.0f, y + 64 + (i - nx.top) * 32, nx.items[i], i == nx.cursor, 1);
    if (nx.top > 0)
        look_arrow(x + NEXT_W - 26, y + 60, 0, 0x6E9AE0);
    if (nx.top + rows < nx.n)
        look_arrow(x + NEXT_W - 26, y + 56 + rows * 32 - 2, 1, 0x6E9AE0);
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
}

/* over the sync's screens, once something else was picked: what comes after, in the header's top right */
static void over_next(void)
{
    char s[160];
    snprintf(s, sizeof(s), T(T_IGR_AFTER_NOW), nx.items[nx.chosen]);
    fit(FONT_SMALL, s, sizeof(s), 300);
    ui_text_right(FONT_SMALL, LOOK_LINE_X1, 40, COLOR_ACCENT, s);
}

/* the list, made when it is opened (the folders of APPS are read then: nothing of it is read at an IGR it isn't
 * opened at). First what the settings have, the OPL "Automatic" stands for named; then the other programs */
static void next_list(void)
{
    char now[260], w[260];
    int i, k;
    if (!appsListed)
        list_apps();
    resolve_target(cfg.igr_return, now, sizeof(now));
    nx.n = 0;
    target_name(!strcasecmp(cfg.igr_return, "auto") ? now : cfg.igr_return, nx.items[0], sizeof(nx.items[0]));
    snprintf(nx.values[nx.n++], sizeof(nx.values[0]), "%s", cfg.igr_return);
    for (i = 0; i < nApps && nx.n < NEXT_MAX - 2; i++) {
        target_on_sd(apps[i].path, w, sizeof(w));
        if (!strcasecmp(w, now))   /* (the one the settings have, first already) */
            continue;
        snprintf(nx.items[nx.n], sizeof(nx.items[0]), "%s", given_name(apps[i].path) ? given_name(apps[i].path) : apps[i].title);
        target_for_ini(apps[i].path, nx.values[nx.n++], sizeof(nx.values[0]));
    }
    for (i = 2; i < nx.n; i++)   /* (the programs by name, as the OPL's Apps tab has them: the folder's order is any) */
        for (k = i; k > 1 && strcasecmp(nx.items[k - 1], nx.items[k]) > 0; k--) {
            char ti[100], tv[200];
            memcpy(ti, nx.items[k], sizeof(ti));
            memcpy(tv, nx.values[k], sizeof(tv));
            memcpy(nx.items[k], nx.items[k - 1], sizeof(ti));
            memcpy(nx.values[k], nx.values[k - 1], sizeof(tv));
            memcpy(nx.items[k - 1], ti, sizeof(ti));
            memcpy(nx.values[k - 1], tv, sizeof(tv));
        }
    if (strcasecmp(now, "osd") != 0) {
        snprintf(nx.items[nx.n], sizeof(nx.items[0]), "%s", T(T_RET_OSD));
        snprintf(nx.values[nx.n++], sizeof(nx.values[0]), "osd");
    }
    if (strcasecmp(now, "poweroff") != 0) {
        snprintf(nx.items[nx.n], sizeof(nx.items[0]), "%s", T(T_POWER_OFF));
        snprintf(nx.values[nx.n++], sizeof(nx.values[0]), "poweroff");
    }
    for (i = 0; i < nx.n; i++)
        fit(FONT_TEXT, nx.items[i], sizeof(nx.items[0]), NEXT_W - 60);
}

static void next_open(void)
{
    next_list();
    ui_lock();
    nx.cursor = nx.chosen >= 0 ? nx.chosen : 0;
    nx.top = nx.cursor >= NEXT_ROWS ? nx.cursor - NEXT_ROWS + 1 : 0;
    nx.open = 1;
    ui_unlock();
    ui_cover(scene_next);
    nx.since = now_ms();
    log_msg("IGR: the list of what comes after it opened");
}

static void next_close(int picked)
{
    ui_cover(NULL);
    nx.open = 0;
    nx.letGo = 1;
    if (picked < 0)
        return;
    nx.chosen = picked;
    ui_over(picked > 0 ? over_next : NULL);   /* (the first is the settings' one: nothing to say) */
    log_msg("IGR: after it, %s", nx.values[picked]);
}

/* from watch_cancel, at IGR, with the buttons down now. 1 = they are the list's (it is open, or was just closed with
 * them): circle and square aren't the sync's then */
int igr_next_poll(u32 pad)
{
    u32 pressed = pad & ~nx.down;
    nx.down = pad;
#ifdef DEBUG_BUILD
    if (debug_take('w'))   /* (script: the list, or the screen with what was picked; once what came before is on it) */
        debug_capture_and_stop();
    if (!nx.open && debug_take('H'))   /* (script: the list opened, as triangle opens it) */
        pressed |= PAD_TRIANGLE;
    if (nx.open && debug_take('y')) {   /* (script: that one of the list picked, as X picks it) */
        int k = debug_digit();
        next_close(k >= 0 && k < nx.n ? k : -1);
        return 1;
    }
#endif
    if (nx.letGo) {
        if (pad & NEXT_KEYS)
            return 1;
        nx.letGo = 0;
    }
    if (!nx.open) {
        if (!(pressed & PAD_TRIANGLE))
            return 0;
        sound_play(SND_CONFIRM);
        next_open();
        return 1;
    }
    if (pressed)
        nx.since = now_ms();
    if (pressed & (PAD_CIRCLE | PAD_TRIANGLE)) {
        sound_play(SND_BACK);
        next_close(-1);
    } else if (pressed & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        next_close(nx.cursor);
    } else if (pressed & (PAD_UP | PAD_DOWN)) {
        ui_lock();
        nx.cursor = list_move(pressed, nx.cursor, nx.n, NEXT_ROWS);
        scroll_to(nx.cursor, &nx.top, NEXT_ROWS);
        ui_unlock();
        sound_play(SND_MOVE);
    }
    return 1;
}

/* the list's buttons aren't anyone else's: it is open, or was closed with them still down */
int igr_next_busy(void) { return nx.open || nx.letGo; }

/* the list, open, waited for while it is being used: 30 s with no button pressed on it close it as it was */
void igr_next_wait(void)
{
    while (nx.open && now_ms() - nx.since < 30000) {
        igr_next_poll(pad_buttons());
        sleep_ms(16);
    }
    if (nx.open)
        next_close(-1);
}

/* IGR is over (the sync done, cancelled, or nothing to do): to what comes after it */
void igr_leave(void)
{
    igr_next_wait();
    leave(nx.chosen >= 0 ? nx.values[nx.chosen] : cfg.igr_return);
}
