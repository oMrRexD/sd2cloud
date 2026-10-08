/*
 * SD2Cloud -- the look of the screens. It follows the spirit of the PlayStation BB Navigator: a deep blue space with
 * soft lights drifting and stars twinkling, a thin line under the header and another over the buttons, menus in light
 * blue where the selected item glows, yellow titles and translucent boxes. The saves of a card are laid out the PS2
 * browser's way, over the same space, with a white light at the selected icon.
 * Everything here is drawn by SD2Cloud itself (assets/ is made by tools/make_assets.py); no image of the console is
 * used.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "ui.h"
#include "look.h"

int look_fade(float t)
{
    return t >= 0.25f ? 0x80 : (int)(0x80 * t / 0.25f);
}

/* ------------------------------------------------------------ background */

#define STARS 120
static struct {
    short x, y;
    unsigned char size, base;
    float speed, phase;
} stars[STARS];
static int starsReady;

static void make_stars(void)
{
    unsigned int s = 20261002;
    int i;
    for (i = 0; i < STARS; i++) {
        s = s * 1103515245u + 12345u;
        stars[i].x = 40 + (s >> 8) % 560;
        s = s * 1103515245u + 12345u;
        stars[i].y = 78 + (s >> 8) % 292;
        s = s * 1103515245u + 12345u;
        stars[i].size = ((s >> 8) % 9 == 0) ? 2 : 1;
        stars[i].base = 0x28 + (s >> 12) % 0x50;
        s = s * 1103515245u + 12345u;
        stars[i].speed = 0.6f + ((s >> 8) % 100) / 40.0f;
        stars[i].phase = ((s >> 16) % 628) / 100.0f;
    }
    starsReady = 1;
}

void look_space(void)
{
    float t = ui_clock();
    int i, prev = 0;
    /* the nebula drifts a little; the image is a bit larger than the screen so its edges never show */
    ui_image(IMG_SPACE, -18 + 10 * sinf(t * 0.031f), -14 + 8 * cosf(t * 0.023f), 676, 476, 0xFFFFFF, 0x80);
    /* soft lights wandering slowly */
    ui_light(80 + 40 * sinf(t * 0.07f), 250 + 24 * cosf(t * 0.05f), 150, 110, 0x3A5AD8, 0x2C + (int)(10 * sinf(t * 0.4f)));
    ui_light(470 + 50 * sinf(t * 0.045f + 1), 262 + 30 * sinf(t * 0.037f), 170, 130, 0x4A62C8, 0x24);
    ui_light(190 + 60 * sinf(t * 0.03f + 2), 150 + 22 * cosf(t * 0.06f), 120, 60, 0x6A4AB8, 0x1C);
    ui_light(330 + 150 * sinf(t * 0.021f), 290 + 50 * sinf(t * 0.05f + 0.7f), 44, 44, 0x9AB0FF, 0x26 + (int)(12 * sinf(t * 0.9f)));
    /* the header and the bottom stay darker, like a frame */
    ui_gradient(0, 0, ui_width(), 58, 0x000000, 0x50, 0x000000, 0x50);
    ui_gradient(0, 58, ui_width(), 26, 0x000000, 0x50, 0x000000, 0x00);
    ui_gradient(0, LOOK_BOTTOM - 10, ui_width(), 22, 0x000000, 0x00, 0x000000, 0x50);
    ui_gradient(0, LOOK_BOTTOM + 12, ui_width(), ui_height() - LOOK_BOTTOM - 12, 0x000000, 0x50, 0x000000, 0x60);
    /* stars */
    if (!starsReady)
        make_stars();
    for (i = 0; i < STARS; i++) {
        int a = (int)(stars[i].base * (0.55f + 0.45f * sinf(t * stars[i].speed + stars[i].phase)));
        ui_rect(stars[i].x, stars[i].y, stars[i].size, stars[i].size, 0xDCE6FF, a);
        if (++prev == 40) {
            ui_flush();
            prev = 0;
        }
    }
    ui_flush();
}

/* ------------------------------------------------------------ frame and legend */

static char news[24];

void look_news(const char *tag) { snprintf(news, sizeof(news), "%s", tag); }

void look_frame(void)
{
    char v[48];
    /* the name alone, in one weight */
    ui_text(FONT_HEADER, 48, 33, 0xEAECEF, "SD2Cloud");
    if (news[0]) {   /* "v0.1 >> v0.2", the new one in light blue */
        int nw = ui_measure(FONT_SMALL, news);
        ui_text_right(FONT_SMALL, LOOK_LINE_X1 - 4, 40, COLOR_ACCENT, news);
        snprintf(v, sizeof(v), "v%s \xC2\xBB ", APP_VERSION);   /* the font has no arrow: >> (U+00BB) */
        ui_text_right(FONT_SMALL, LOOK_LINE_X1 - 4 - nw, 40, 0x6A7486, v);
    } else {
        snprintf(v, sizeof(v), "v%s", APP_VERSION);
        ui_text_right(FONT_SMALL, LOOK_LINE_X1 - 4, 40, 0x6A7486, v);
    }
#ifdef DEBUG_BUILD
    {   /* the debug build says so, before its version: nothing else on the screen tells it from the program */
        int w = ui_measure(FONT_SMALL, v) + (news[0] ? ui_measure(FONT_SMALL, news) : 0);
        ui_text_right(FONT_SMALL, LOOK_LINE_X1 - 4 - w - 10, 40, COLOR_WARN, "debug");
    }
#endif
    ui_rect(LOOK_LINE_X0, LOOK_TOP, LOOK_LINE_X1 - LOOK_LINE_X0, 1, 0x5C6476, 0x80);
    ui_rect(LOOK_LINE_X0, LOOK_TOP + 1, LOOK_LINE_X1 - LOOK_LINE_X0, 1, 0x5C6476, 0x28);
    ui_rect(LOOK_LINE_X0, LOOK_BOTTOM, LOOK_LINE_X1 - LOOK_LINE_X0, 1, 0x5C6476, 0x80);
    ui_rect(LOOK_LINE_X0, LOOK_BOTTOM + 1, LOOK_LINE_X1 - LOOK_LINE_X0, 1, 0x5C6476, 0x28);
    ui_flush();
}

void look_legend(const legend_t *items, int n, int apart)
{
    /* (five of them only fit the line a little closer together) */
    const int size = 20, gap = n > 4 ? 22 : 26, y = LOOK_BOTTOM + 11;
    float x = LOOK_LINE_X1 - 8;
    int i;
    for (i = n - 1; i >= 0; i--) {
        int w = ui_measure(FONT_TEXT, items[i].text);
        x -= w;
        ui_text(FONT_TEXT, x, y, 0xE6E8EC, items[i].text);
        x -= size + 6;
        ui_button(items[i].button, x, y + 1, size);
        x -= (apart && i == n - 1) ? 70 : gap;
    }
}

/* ------------------------------------------------------------ boxes, menus, titles */

void look_panel(float x, float y, float w, float h)
{
    /* soft edges: a few slightly bigger, fainter boxes around it */
    ui_rect(x - 6, y - 6, w + 12, h + 12, 0x040A18, 0x14);
    ui_rect(x - 3, y - 3, w + 6, h + 6, 0x06102A, 0x1C);
    ui_gradient(x, y, w, h, 0x0B1830, 0x5C, 0x0E2246, 0x66);
    /* a light from below, inside */
    ui_light(x + w * 0.55f, y + h * 0.78f, w * 0.42f, h * 0.5f, 0x1E4C9C, 0x34);
    ui_rect(x, y, w, 1, 0x6A8CC8, 0x1C);
    ui_flush();
}

/* 0..1..0 every 1.8 s, smooth: the selected item breathes, like the BB Navigator's (and the screen never looks
 * frozen) */
float look_pulse(void)
{
    return 0.5f - 0.5f * cosf(ui_clock() * 6.2831853f / 1.8f);
}

static u32 mix(u32 a, u32 b, float k)
{
    int s, out = 0;
    for (s = 0; s < 24; s += 8) {
        int x = (a >> s) & 0xFF, y = (b >> s) & 0xFF;
        out |= ((int)(x + (y - x) * k) & 0xFF) << s;
    }
    return out;
}

int look_glow_text(int font, float x, float y, u32 dim, u32 bright, const char *text)
{
    int w = ui_measure(font, text), lh = ui_line_height(font);
    float p = look_pulse();
    /* a soft light behind that breathes, and the letters themselves sharp: on a real TV a halo made of copies of the
     * letters around them (fine on PCSX2) smears them into a blur */
    ui_light(x + w / 2.0f, y + lh / 2.0f + 1, w / 2.0f + 40, lh * 0.9f, COLOR_GLOW, 0x14 + (int)(0x34 * p));
    return ui_text(font, x, y, mix(dim, bright, 0.45f + 0.55f * p), text);
}

int look_item(float x, float y, const char *text, int selected, int center)
{
    int w = ui_measure(FONT_TEXT, text);
    float left = center ? (int)(x - w / 2) : x;
    if (!selected)
        return ui_text(FONT_TEXT, left, y, COLOR_ITEM, text);
    return look_glow_text(FONT_TEXT, left, y, COLOR_ITEM, COLOR_ITEM_ON, text);
}

void look_title(float x, float y, const char *text, int center)
{
    int w = ui_measure(FONT_TEXT, text);
    float left = center ? (int)(x - w / 2) : x;
    ui_text_glow(FONT_TEXT, left, y, COLOR_TITLE, 0x4A3E08, text);
}

void look_card(float x, float y, const char *number, const char *label)
{
    ui_image(IMG_CARD, x, y, LOOK_CARD_W, LOOK_CARD_H, 0xFFFFFF, 0x80);
    if (number && number[0]) {
        int lh = ui_line_height(FONT_HUGE);
        ui_text_center(FONT_HUGE, x + LOOK_CARD_W / 2.0f, y + LOOK_CARD_H * 0.52f - lh / 2.0f, 0x4FB4BC, number);
    } else if (label && label[0]) {
        int lh = ui_line_height(FONT_TEXT), w = ui_measure(FONT_TEXT, label);
        if (w > LOOK_CARD_W - 40)
            ui_text_fit(FONT_TEXT, x + 20, y + LOOK_CARD_H * 0.62f - lh / 2.0f, LOOK_CARD_W - 40, 0x8CD4DA, label);
        else
            ui_text_center(FONT_TEXT, x + LOOK_CARD_W / 2.0f, y + LOOK_CARD_H * 0.62f - lh / 2.0f, 0x8CD4DA, label);
    }
}

/* ------------------------------------------------------------ progress and arrows */

void look_bar(float x, float y, float w, int permille)
{
    float f;
    if (permille < 0)
        permille = 0;
    if (permille > 1000)
        permille = 1000;
    f = w * permille / 1000.0f;
    ui_rect(x, y, w, 4, 0x3A4456, 0x80);
    ui_rect(x, y, f, 4, 0xF4F6FA, 0x80);
    if (permille > 0 && permille < 1000)
        ui_light(x + f, y + 2, 16, 8, 0x9CC8FF, 0x30);
}

void look_arrow(float cx, float y, int down, u32 color)
{
    float bob = 2 * sinf(ui_clock() * 4);
    if (down)
        ui_triangle(cx - 9, y + bob, cx + 9, y + bob, cx, y + 10 + bob, color, 0x80);
    else
        ui_triangle(cx, y - bob, cx + 9, y + 10 - bob, cx - 9, y + 10 - bob, color, 0x80);
}

/* ------------------------------------------------------------ the saves screen: the light behind the selected one */

void look_halo(float cx, float cy, float r, int alpha)
{
    ui_light(cx, cy, r, r, 0xFFFFFF, alpha);
    ui_light(cx, cy, r * 0.55f, r * 0.55f, 0xFFFFFF, alpha);
}
