/* SD2Cloud -- the dialog: a box with a title, text, a bar and buttons, which most questions and messages are, and the
 * list to pick one thing from. */
#include "app.h"

/* ------------------------------------------------------------ the dialog: a box with a title, text, a bar, buttons */

dialog_t dlg, next;

static void scene_dialog(float t)
{
    int w = dlg.wide ? 520 : 440, x = (W - w) / 2, pad = 28, inner = w - 2 * pad, h = 2 * pad, y, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    if (dlg.title[0])
        h += ui_paragraph_height(FONT_TEXT, inner, dlg.title) + 10;
    for (i = 0; i < dlg.nlines; i++)
        h += ui_paragraph_height(dlg.line[i].font, inner, dlg.line[i].text) + dlg.line[i].gap;
    if (dlg.permille >= 0)
        h += 18 + (dlg.note[0] ? 22 : 0);
    if (h < 130)
        h = 130;
    y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2;
    look_panel(x, y, w, h);
    y += pad;
    if (h == 130 && !dlg.title[0] && dlg.nlines == 1 && dlg.permille < 0)   /* a single short line: in the middle */
        y += (130 - 2 * pad - ui_paragraph_height(dlg.line[0].font, inner, dlg.line[0].text)) / 2;
    if (dlg.title[0]) {
        if (dlg.titleColor == COLOR_TITLE && ui_paragraph_height(FONT_TEXT, inner, dlg.title) <= ui_line_height(FONT_TEXT) + 2) {
            look_title(x + pad, y, dlg.title, 0);
            y += ui_line_height(FONT_TEXT) + 2;
        } else
            y = ui_paragraph(FONT_TEXT, x + pad, y, inner, dlg.titleColor, dlg.title);
        y += 10;
    }
    for (i = 0; i < dlg.nlines; i++)
        y = ui_paragraph(dlg.line[i].font, x + pad, y, inner, dlg.line[i].color, dlg.line[i].text) + dlg.line[i].gap;
    if (dlg.permille >= 0) {
        look_bar(x + pad, y + 8, inner, dlg.permille);
        if (dlg.note[0])
            ui_text_right(FONT_SMALL, x + pad + inner, y + 20, COLOR_DIM, dlg.note);
    }
    look_legend(dlg.legend, dlg.nlegend, 0);
}

void dlg_new(u32 titleColor, const char *title)
{
    memset(&next, 0, sizeof(next));
    next.permille = -1;
    next.titleColor = titleColor;
    snprintf(next.title, sizeof(next.title), "%s", title ? title : "");
}

void dlg_line(int font, u32 color, int gap, const char *text)
{
    if (next.nlines == DLG_LINES || !text || !text[0])
        return;
    snprintf(next.line[next.nlines].text, sizeof(next.line[0].text), "%s", text);
    next.line[next.nlines].color = color;
    next.line[next.nlines].font = font;
    next.line[next.nlines++].gap = gap;
}

void dlg_bar(int permille, const char *note)
{
    next.permille = permille < 0 ? 0 : permille;
    snprintf(next.note, sizeof(next.note), "%s", note ? note : "");
}

/* the buttons, left to right (text ids; 0 = none) */
void dlg_buttons(int b1, int t1, int b2, int t2)
{
    next.nlegend = 0;
    if (t1)
        next.legend[next.nlegend].button = b1, next.legend[next.nlegend++].text = T(t1);
    if (t2)
        next.legend[next.nlegend].button = b2, next.legend[next.nlegend++].text = T(t2);
}

/* a third one, after those */
void dlg_button(int button, int text)
{
    if (next.nlegend < 3)
        next.legend[next.nlegend].button = button, next.legend[next.nlegend++].text = T(text);
}

void dlg_show(void)
{
    ui_lock();
    dlg = next;
    ui_unlock();
    ui_scene(scene_dialog);
}

/* only the background and the frame: between two screens */
void scene_frame(float t)
{
    (void)t;
    look_space();
    look_frame();
}

/* a message: title (may be NULL) and a text */
void message(u32 titleColor, const char *title, u32 textColor, const char *text)
{
    dlg_new(titleColor, title);
    dlg_line(FONT_TEXT, textColor, 0, text);
    dlg_show();
}

/* a message that waits for X or circle */
void message_wait(u32 titleColor, const char *title, u32 textColor, const char *text)
{
    dlg_new(titleColor, title);
    dlg_line(FONT_TEXT, textColor, 0, text);
    dlg_buttons(BUTTON_CROSS, T_BACK, 0, 0);
    dlg_show();
    wait_button(PAD_CROSS | PAD_CIRCLE, 0);
    sound_play(SND_BACK);
}

/* asks before doing something: 1 = X (yesText), 0 = circle */
int confirm(const char *title, const char *text, int yesText)
{
    dlg_new(COLOR_TITLE, title);
    if (text)
        dlg_line(FONT_TEXT, COLOR_TEXT, 0, text);
    dlg_buttons(BUTTON_CIRCLE, T_BACK, BUTTON_CROSS, yesText);
    dlg_show();
    if (wait_button(PAD_CROSS | PAD_CIRCLE, 0) & PAD_CROSS) {
        sound_play(SND_CONFIRM);
        return 1;
    }
    sound_play(SND_BACK);
    return 0;
}

/* -------- a list to pick from, in a box (the card's menu, the card to copy a save to) */

#define PICK_MAX 128
#define PICK_ROWS 6
#define PICK_W 380   /* the box; an item gets 60 less than that, clear of the arrows on its right */
static struct {
    char title[100];
    char items[PICK_MAX][100];
    int n, cursor, top;
} pick;

static void scene_pick(float t)
{
    int rows = pick.n < PICK_ROWS ? pick.n : PICK_ROWS, w = PICK_W, h = 56 + rows * 32 + 24, x = (W - w) / 2,
        y = LOOK_TOP + (LOOK_BOTTOM - LOOK_TOP - h) / 2, i;
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_panel(x, y, w, h);
    look_title(W / 2.0f, y + 20, pick.title, 1);
    for (i = pick.top; i < pick.n && i < pick.top + rows; i++)
        look_item(W / 2.0f, y + 64 + (i - pick.top) * 32, pick.items[i], i == pick.cursor, 1);
    if (pick.top > 0)
        look_arrow(x + w - 26, y + 60, 0, 0x6E9AE0);
    if (pick.top + rows < pick.n)
        look_arrow(x + w - 26, y + h - 26, 1, 0x6E9AE0);
    {
        legend_t l[2] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_SELECT)}};
        look_legend(l, 2, 0);
    }
}

/* the index picked, or -1 (circle). start = the item the cursor starts on (the current value) */
int choose(const char *title, const char *const *items, int n, int start)
{
    int i;
    if (n > PICK_MAX)
        n = PICK_MAX;
    ui_lock();
    snprintf(pick.title, sizeof(pick.title), "%s", title);
    for (i = 0; i < n; i++) {   /* a program's name can be any length: one that doesn't fit the box is cut */
        snprintf(pick.items[i], sizeof(pick.items[0]), "%s", items[i]);
        fit(FONT_TEXT, pick.items[i], sizeof(pick.items[0]), PICK_W - 60);
    }
    pick.n = n;
    pick.cursor = start >= 0 && start < n ? start : 0;
    pick.top = pick.cursor >= PICK_ROWS ? pick.cursor - PICK_ROWS + 1 : 0;
    ui_unlock();
    ui_scene(scene_pick);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_CROSS | PAD_CIRCLE);
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return -1;
        }
        if (b & PAD_CROSS) {
            sound_play(SND_CONFIRM);
            return pick.cursor;
        }
        ui_lock();
        pick.cursor = (b & PAD_UP) ? (pick.cursor + n - 1) % n : (pick.cursor + 1) % n;
        if (pick.cursor < pick.top)
            pick.top = pick.cursor;
        if (pick.cursor >= pick.top + PICK_ROWS)
            pick.top = pick.cursor - PICK_ROWS + 1;
        ui_unlock();
        sound_play(SND_MOVE);
    }
}
