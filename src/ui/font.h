/* SD2Cloud -- text with a TrueType font (font.c) */
#ifndef FONT_H
#define FONT_H

#include <tamtypes.h>

int font_init(void);   /* 0 = ok */
/* a font at a size in pixels; bold = how much thicker the strokes get, in 1/64 pixel (0 = as drawn). The ttf stays in
 * use while the font exists. Returns the font's id, or -1 */
int font_load(const void *ttf, int ttfSize, int pixels, int bold);
/* draws a UTF-8 text with its line's top at y (the GS color, 0x80 = full); returns the width */
int font_draw(int id, float x, float y, u64 color, const char *utf8);
int font_width(int id, const char *utf8);
int font_line(int id);     /* line height in pixels */

#endif
