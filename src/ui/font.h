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
int font_has(unsigned int code);   /* 1 = the font has this character (a Unicode code), as things are now */
/* the Japanese characters read, for every font (also the ones loaded after): 0 = they are. spare = only if that much
 * memory is still left after them (0 = whatever is left) */
int font_japanese(long spare);
void font_japanese_free(void);     /* let go of: their glyphs already drawn stay, any other is a '?' until read again */

#endif
