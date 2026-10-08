/* SD2Cloud -- a keyboard on the screen, for the few names that are typed (a template's): the letters, the digits, a
 * space and the two marks a folder's name can have. X types the key under the cursor, square erases, START takes the
 * name and circle gives it up. */
#include "app.h"

#define KB_COLS  10
#define KB_ROWS  4
#define KB_KEYS  (KB_COLS * KB_ROWS)
#define KEY_CASE '\001'   /* the key that changes between capitals and small letters */
static const char kbKeys[KB_KEYS + 1] = "ABCDEFGHIJ" "KLMNOPQRST" "UVWXYZ0123" "456789-_ \001";

static struct {
    const char *title;
    char text[64];
    int max;       /* how many characters the name can have */
    int cursor;    /* the key the cursor is on */
    int small;     /* the letters are typed small */
    int fresh;     /* the name it came with wasn't touched yet: the first key typed takes its place */
} kb;

#define KB_X0   100
#define KB_Y0   182
#define KB_W    44
#define KB_H    40
#define FIELD_Y 118
#define FIELD_H 40

static void scene_keyboard(float t)
{
    char label[4];
    int i, lh = ui_line_height(FONT_BROWSER), w = ui_measure(FONT_BROWSER, kb.text);
    float x = (int)(W / 2.0f - w / 2.0f);
    look_space();
    look_frame();
    ui_alpha(look_fade(t));
    look_title(W / 2.0f, 86, kb.title, 1);
    /* the name, in a box, with a bar where the next letter goes */
    look_panel(140, FIELD_Y, 360, FIELD_H);
    ui_text(FONT_BROWSER, x, FIELD_Y + (FIELD_H - lh) / 2.0f, kb.fresh ? COLOR_DIM : COLOR_TEXT, kb.text);
    if ((int)(t * 2) % 2 == 0)
        ui_rect(x + w + 2, FIELD_Y + 8, 2, FIELD_H - 16, COLOR_ACCENT, 0x80);
    for (i = 0; i < KB_KEYS; i++) {
        char c = kbKeys[i];
        float cx = KB_X0 + (i % KB_COLS) * KB_W + KB_W / 2.0f, y = KB_Y0 + (i / KB_COLS) * KB_H;
        int on = i == kb.cursor;
        if (on)   /* (the light of the selected item, whatever the key shows) */
            ui_light(cx, y + lh / 2.0f + 1, 30, lh * 0.9f, COLOR_GLOW, 0x20 + (int)(0x34 * look_pulse()));
        if (c == ' ') {   /* the space: a low bracket */
            u32 color = on ? COLOR_ITEM_ON : COLOR_ITEM;
            ui_rect(cx - 9, y + lh - 7, 18, 2, color, 0x80);
            ui_rect(cx - 9, y + lh - 12, 2, 5, color, 0x80);
            ui_rect(cx + 7, y + lh - 12, 2, 5, color, 0x80);
            continue;
        }
        if (c == KEY_CASE)
            snprintf(label, sizeof(label), "%s", kb.small ? "aA" : "Aa");
        else
            snprintf(label, sizeof(label), "%c", kb.small ? tolower((unsigned char)c) : c);
        ui_text(FONT_BROWSER, (int)(cx - ui_measure(FONT_BROWSER, label) / 2.0f), y, on ? COLOR_ITEM_ON : COLOR_ITEM, label);
    }
    {
        legend_t l[4] = {{BUTTON_CIRCLE, T(T_BACK)}, {BUTTON_CROSS, T(T_KB_TYPE)}, {BUTTON_SQUARE, T(T_KB_ERASE)},
                         {BUTTON_START, T(T_FINISH)}};
        look_legend(l, 4, 0);
    }
}

/* A name typed on the screen. text = the name it starts with ("" = none), which the first key typed takes the place
 * of; size - 1 = how long it can be. 1 = START: text is the name, without spaces around it (never empty);
 * 0 = circle, and text is as it was */
int keyboard(const char *title, char *text, size_t size)
{
    ui_lock();
    kb.title = title;
    snprintf(kb.text, sizeof(kb.text), "%s", text);
    kb.max = size - 1 < sizeof(kb.text) - 1 ? (int)size - 1 : (int)sizeof(kb.text) - 1;
    kb.cursor = 0;
    kb.small = 0;
    kb.fresh = kb.text[0] != 0;
    ui_unlock();
    ui_scene(scene_keyboard);
    for (;;) {
        u32 b = wait_nav(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_CROSS | PAD_SQUARE | PAD_CIRCLE | PAD_START);
        int n = (int)strlen(kb.text), sound = SND_MOVE;
        if (b & PAD_CIRCLE) {
            sound_play(SND_BACK);
            return 0;
        }
        if (b & PAD_START) {
            char name[64];
            snprintf(name, sizeof(name), "%s", kb.text);
            trim(name);
            if (!name[0]) {
                sound_play(SND_BACK);
                continue;
            }
            snprintf(text, size, "%s", name);
            sound_play(SND_CONFIRM);
            return 1;
        }
        ui_lock();
        if (b & PAD_LEFT)
            kb.cursor = kb.cursor % KB_COLS ? kb.cursor - 1 : kb.cursor + KB_COLS - 1;
        else if (b & PAD_RIGHT)
            kb.cursor = (kb.cursor + 1) % KB_COLS ? kb.cursor + 1 : kb.cursor - KB_COLS + 1;
        else if (b & PAD_UP)
            kb.cursor = (kb.cursor + KB_KEYS - KB_COLS) % KB_KEYS;
        else if (b & PAD_DOWN)
            kb.cursor = (kb.cursor + KB_COLS) % KB_KEYS;
        else if (b & PAD_SQUARE) {   /* (erasing from the name it came with is a way of changing only its end) */
            sound = n ? SND_CONFIRM : SND_BACK;
            if (n)
                kb.text[n - 1] = 0;
            kb.fresh = 0;
        } else if (kbKeys[kb.cursor] == KEY_CASE) {
            kb.small = !kb.small;
            sound = SND_CONFIRM;
        } else {
            char c = kbKeys[kb.cursor];
            if (kb.fresh)
                n = 0;
            if (n < kb.max && !(c == ' ' && !n)) {   /* (a name doesn't start with a space) */
                kb.text[n] = kb.small ? tolower((unsigned char)c) : c;
                kb.text[n + 1] = 0;
                kb.fresh = 0;
                /* a word starts with a capital and goes on in small letters, unless the key says otherwise */
                if (isalpha((unsigned char)c))
                    kb.small = 1;
                else if (!isdigit((unsigned char)c))
                    kb.small = 0;
                sound = SND_CONFIRM;
            } else
                sound = SND_BACK;
        }
        ui_unlock();
        sound_play(sound);
    }
}
