/* SD2Cloud -- the screen (gsKit): a drawing thread that draws the current scene every frame, text with a TrueType
 * font, images, lights and the controller's symbols */
#ifndef UI_H
#define UI_H

#include <tamtypes.h>

/* colors 0xRRGGBB */
#define COLOR_TEXT    0xF0F0F2   /* text on panels */
#define COLOR_DIM     0x8C96A8   /* secondary text */
#define COLOR_ITEM    0x9CC4F0   /* list items (the console's light blue) */
#define COLOR_ITEM_ON 0xE2F1FF   /* the selected item */
#define COLOR_GLOW    0x3A78E0   /* the light around the selected item */
#define COLOR_TITLE   0xE8DE6E   /* titles (yellow) */
#define COLOR_ACCENT  0x7CC8FF
#define COLOR_OK      0x7EE08E
#define COLOR_WARN    0xFFD45A
#define COLOR_ERROR   0xFF7A7A

/* fonts */
enum {
    FONT_TEXT,      /* 18 px: lists, panels, legend */
    FONT_SMALL,     /* 15 px: details */
    FONT_TITLE,     /* 26 px: the big titles (game name on the backup screen, the code on the sign-in screen) */
    FONT_HEADER,    /* 22 px: "SD2Cloud" in the header */
    FONT_BROWSER,   /* 23 px, bold: the saves screen */
    FONT_HUGE,      /* 92 px: the number on the big memory card */
    FONT_COUNT
};

/* images (the PNGs in assets) */
enum { IMG_SPACE, IMG_GLOW, IMG_BUTTONS, IMG_CARD, IMG_MINICARD, IMG_COUNT };

int ui_init(void);                 /* 0 = ok; starts the drawing thread (a black screen until the first scene) */
void ui_end(void);                 /* stops the drawing thread */
#ifdef DEBUG_BUILD
void ui_abort(void);               /* the same without waiting for the frame being drawn (the rescue, in system.c) */
#endif
int ui_width(void);
int ui_height(void);

/* ---- scenes: draw(t) is called every frame on the drawing thread, t = seconds since this scene started. The main
 * thread changes what the scene shows between ui_lock and ui_unlock (it waits for the frame being drawn to end) */
void ui_scene(void (*draw)(float t));
void ui_lock(void);
void ui_unlock(void);
float ui_clock(void);              /* seconds since the app started (animations that go on across scenes) */

/* ---- drawing (from a scene) */
void ui_alpha(int a);              /* opacity of what's drawn next, 0..128 (fades) */
void ui_additive(int on);          /* light adds up (glows) instead of covering */
void ui_flush(void);               /* sends what was queued to the GS */

int ui_text(int font, float x, float y, u32 color, const char *utf8);   /* returns the width */
int ui_measure(int font, const char *utf8);
int ui_line_height(int font);
void ui_text_center(int font, float cx, float y, u32 color, const char *utf8);
void ui_text_right(int font, float right, float y, u32 color, const char *utf8);
/* text with a light around its letters (the selected item), glow = its color */
int ui_text_glow(int font, float x, float y, u32 color, u32 glow, const char *utf8);
/* a dark copy one pixel down and right first (light text over light backgrounds) */
int ui_text_shadow(int font, float x, float y, u32 color, const char *utf8);
/* wraps the text into lines that fit the width; returns the y after the last line */
int ui_paragraph(int font, float x, float y, int width, u32 color, const char *utf8);
int ui_paragraph_height(int font, int width, const char *utf8);
/* a single line; if it is wider than width, it is cut and ends with "..." */
int ui_text_fit(int font, float x, float y, int width, u32 color, const char *utf8);

void ui_rect(float x, float y, float w, float h, u32 color, int alpha);
void ui_gradient(float x, float y, float w, float h, u32 top, int alphaTop, u32 bottom, int alphaBottom);   /* vertical */
void ui_line(float x1, float y1, float x2, float y2, u32 color, int alpha);
void ui_triangle(float x1, float y1, float x2, float y2, float x3, float y3, u32 color, int alpha);
void ui_quad(const float *xy, const u32 *colors, const int *alphas);   /* 4 corners in strip order: tl, tr, bl, br */
/* an image stretched over a rectangle, tinted (0xFFFFFF = as it is); the part version takes texture coordinates */
void ui_image(int img, float x, float y, float w, float h, u32 tint, int alpha);
void ui_image_part(int img, float x, float y, float w, float h, float u0, float v0, float u1, float v1, u32 tint, int alpha);
/* a soft round light centered on (cx, cy) */
void ui_light(float cx, float cy, float rx, float ry, u32 color, int alpha);
/* QR code of a text, with its white margin; returns the side in pixels (0 = it didn't fit) */
int ui_qr(const char *text, float x, float y, int module);

/* the controller's symbols */
enum { BUTTON_CROSS, BUTTON_CIRCLE, BUTTON_TRIANGLE, BUTTON_SQUARE,
       BUTTON_START, BUTTON_SELECT };   /* those two as the controller has them: a small arrow and a small bar */
void ui_button(int button, float x, float y, float size);

#ifdef DEBUG_BUILD
/* saves the next frame drawn (to check the screens on a PC); waits until the scene has been on screen for a moment,
 * so its fade-in is over */
int ui_capture(const char *path);
#endif

#endif
