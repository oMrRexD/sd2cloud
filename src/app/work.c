/* SD2Cloud -- the screen of something being done to a card or to a save (the icon turning, a bar): a backup on its
 * way to Drive, a card restored, installed, copied or made, a save going into a card. And giving it up: circle
 * asks, and the answer stops it or not. */
#include "app.h"

/* ------------------------------------------------------------ checking */

void checking_progress(int i, int n, const card_t *c)
{
    char t[160];
    dlg_new(COLOR_TITLE, igrMode ? T(T_IGR_TITLE) : T(T_SEARCHING));
    snprintf(t, sizeof(t), "%s %s", T(T_CHECKING), c->name[0] ? c->name : c->base);
    dlg_line(FONT_TEXT, COLOR_DIM, 0, t);
    dlg_bar(n ? (i * 1000) / n : 0, NULL);
    dlg_show();
#ifdef DEBUG_BUILD
    if (n && i * 2 >= n)
        debug_capture_if('P');
#endif
}

/* ------------------------------------------------------------ cancelling */

/* watches the controller during uploads and downloads (curl calls it too, while it transfers): a new press of circle =
 * the user wants to cancel. Only the press counts, not the button held down: the circle that answered "keep going"
 * may still be held when the upload resumes. The question itself waits for the next progress update */
int circleDown;

void watch_cancel(void)
{
    int down = (pad_buttons() & PAD_CIRCLE) != 0;
    if (down && !circleDown)
        cancelLatched = 1;
    circleDown = down;
#ifdef DEBUG_BUILD
    if (debug_take('K'))
        cancelLatched = 1;
#endif
}

int watchOff;   /* the device isn't watched (device_watch): something is under way that can't be left halfway */

/* 1 = yes, cancel */
int confirm_cancel(int title, int text)
{
    int yes;
    watchOff++;   /* (asked in the middle of what is being done) */
    sound_play(SND_BACK);
    dlg_new(COLOR_WARN, T(title));
    dlg_line(FONT_TEXT, COLOR_TEXT, 0, T(text));
    dlg_buttons(BUTTON_CIRCLE, T_CANCEL_NO, BUTTON_CROSS, T_CANCEL_YES);
    dlg_show();
    yes = (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) != 0;
    sound_play(yes ? SND_CONFIRM : SND_BACK);
    cancelLatched = 0;
    circleDown = 1;   /* the circle of "keep going": it counts again only after being released */
    log_msg("cancel? %s", yes ? "yes" : "no, keep going");
    watchOff--;
    return yes;
}

/* ------------------------------------------------------------ backup */

const card_t *current;
int currentN, totalN;   /* "Uploading currentN of totalN" */

/* the icon of the card being sent: on a game's card (Game ID), the 3D icon of its newest save; on a card shared by
 * many games (CardN, named folders, BootCard) or when the save's icon can't be read, the SD2PSX memory card */
static icon_t *cardIcon;
u64 iconStart;
/* a single save on its way (save_to_cloud): the screen shows its icon (the saves screen's, not freed here) and its
 * name instead of the card's */
icon_t *saveIcon;
char saveTitle[100];
static volatile long long shownDone, shownTotal;
/* a save on its way into a card (save_into): the same screen, saying that instead of the upload; workFixed = what
 * is being done can't be given up any more, so the legend doesn't offer to */
const char *workText;
int workFixed;

void load_card_icon(const card_t *c)
{
    buffer_t iconsys = {0}, ico = {0};
    char folder[33];
    icon_t *ic = NULL;
    if (c->type == TYPE_GAMEID && mcfs_newest_save_icon(c->path, folder, &iconsys, &ico) == 0 && (ic = icon_load(&iconsys, &ico)))
        log_msg("%s: icon of %s (%d vertices, %d shapes) \"%s\"", c->id, folder, ic->nv, ic->shapes, ic->title);
    buf_free(&iconsys);
    buf_free(&ico);
    ui_lock();
    if (cardIcon != sd2psxIcon)
        icon_free(cardIcon);
    cardIcon = ic ? ic : sd2psxIcon;
    current = c;
    iconStart = now_ms();
    ui_unlock();
}

/* the screen while a card is sent, like a console's saving screen: the icon turning on the left, in a soft light; the
 * game (or the card), the card and what's happening on the right, over a thin white bar */
void scene_upload(float t)
{
    char s[200];
    const card_t *c = current;
    long long done = shownDone, total = shownTotal;
    const char *title;
    int x = 300, y, w = LOOK_LINE_X1 - 8 - 300;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    if (!c)
        return;
    title = saveIcon ? saveTitle
            : c->type == TYPE_GAMEID && cardIcon && cardIcon != sd2psxIcon && cardIcon->title[0] ? cardIcon->title
            : c->name[0] ? c->name : c->base;
    look_halo(168, 222, 118, 0x1A);
    icon_draw(saveIcon ? saveIcon : cardIcon, 168, 218, 210, (now_ms() - iconStart) / 1000.0f);
    y = ui_paragraph(FONT_TITLE, x, 138, w, COLOR_TEXT, title) + 2;
    if (strcmp(title, c->base)) {
        ui_text_fit(FONT_SMALL, x, y, w, COLOR_DIM, c->base);
        y += ui_line_height(FONT_SMALL) + 4;
    }
    y += 18;
    ui_text_fit(FONT_TEXT, x, y, w, COLOR_TEXT, workText ? workText : T(T_BACKING_UP));
    y += ui_line_height(FONT_TEXT) + 12;
    look_bar(x, y, w, total ? (int)(done * 1000 / total) : 0);
    if (totalN > 1) {
        snprintf(s, sizeof(s), T(T_UPLOADING), currentN, totalN);
        ui_text_right(FONT_SMALL, x + w, y + 12, COLOR_DIM, s);
    }
    if (!workFixed) {
        legend_t l = {BUTTON_CIRCLE, T(T_CANCEL)};
        look_legend(&l, 1, 0);
    }
}

void upload_screen(long long done, long long total)
{
    shownDone = done;
    shownTotal = total;
    ui_scene(scene_upload);
}

/* A whole card that is not being sent but restored, installed from a file, copied to a device or made: the same
 * screen, saying that instead (text). own = a game's card turns the icon of its newest save, as when it is sent; any
 * other, and a card that isn't there yet, the memory card. fixed = what is being done can't be given up */
void card_work(const card_t *c, int own, const char *text, int fixed)
{
    if (own)
        load_card_icon(c);
    ui_lock();
    if (!own) {
        if (cardIcon != sd2psxIcon)
            icon_free(cardIcon);
        cardIcon = sd2psxIcon;
        current = c;
        iconStart = now_ms();
    }
    saveIcon = NULL;
    workText = text;
    workFixed = fixed;
    currentN = totalN = 1;
    ui_unlock();
    cancelLatched = 0;
    upload_screen(0, 1);
}

void card_work_done(void)
{
    ui_scene(scene_frame);   /* off the screen before what it shows is let go of */
    ui_lock();
    current = NULL;
    workText = NULL;
    workFixed = 0;
    ui_unlock();
}

#ifdef DEBUG_BUILD
/* script "V<n>" at the start: shows the backup screen of card n (in the menu's order) for 1.5 s without sending
 * anything, and captures it: to check the icons on PCSX2 */
void debug_preview(int k)
{
    u64 end;
    if (k < 0 || k >= nCards)
        return;
    currentN = totalN = 1;
    load_card_icon(&cards[k]);
    /* the main thread only sleeps, as if it were waiting on the network: the animation must keep going by itself */
    end = now_ms() + 1500;
    while (now_ms() < end) {
        upload_screen((long long)(now_ms() - iconStart), 3000);
        sleep_ms(100);
    }
    debug_capture_and_stop();
}
#endif

/* -------- a save on its way into a card (imported from a .psu, copied or moved from another card or from a card
 * file): the screen of an upload, with the save's icon turning (the blue cube when it has none), its name, the card
 * it goes to and a bar for the whole of it: reading it 15%, writing it 65%, reading it back 20%. Circle gives up,
 * while there is something to give up: the card is then left as it was */

void save_into(const card_t *to, const save_view_t *v, int text)
{
    char name[100];
    save_name(v, name, sizeof(name));
    ui_lock();
    current = to;
    saveIcon = v->icon ? v->icon : cubeIcon;
    snprintf(saveTitle, sizeof(saveTitle), "%s", name);
    workText = T(text);
    workFixed = 0;
    iconStart = now_ms();
    currentN = totalN = 1;
    ui_unlock();
    cancelLatched = 0;
    upload_screen(0, 1);
}

int save_progress(int phase, long long done, long long total)
{
    static const int start[] = {0, 150, 800}, span[] = {150, 650, 200};
#ifdef DEBUG_BUILD
    if (phase == MCFS_STEP_WRITE && total && done * 2 >= total && debug_take('k'))
        cancelLatched = 1;   /* (script: circle halfway through the writing) */
#endif
    if (phase != MCFS_STEP_CHECK) {
        watch_cancel();
        if (cancelLatched && confirm_cancel(T_WORK_CANCEL_TITLE, T_RESTORE_CANCEL_TEXT)) {
            ui_lock();   /* what was written is undone before this returns, which takes a moment */
            workText = T(T_CANCELLING);
            workFixed = 1;
            ui_unlock();
            ui_scene(scene_upload);
            return 1;
        }
    }
    ui_lock();
    workFixed = phase == MCFS_STEP_CHECK;
    ui_unlock();
    if (done > total)
        done = total;
    upload_screen(start[phase] + (total ? span[phase] * done / total : 0), 1000);
#ifdef DEBUG_BUILD
    if (phase == MCFS_STEP_WRITE && total && done * 2 >= total)
        debug_capture_if('W');
#endif
    return 0;
}

/* nothing more to give up (a save that was moved is still to be deleted from where it was) */
void save_into_fixed(void)
{
    ui_lock();
    workFixed = 1;
    ui_unlock();
}

void save_into_done(void)
{
    ui_scene(scene_frame);   /* off the screen before the icon is let go of */
    ui_lock();
    saveIcon = NULL;
    workText = NULL;
    workFixed = 0;
    ui_unlock();
}
