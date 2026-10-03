/* SD2Cloud -- the look of the screens (look.c): inspired by the PlayStation BB Navigator's (a starry space with soft
 * lights, a thin frame, glowing menus) and, for the saves of a card, by the PS2 browser's */
#ifndef LOOK_H
#define LOOK_H

#include <tamtypes.h>

/* the frame */
#define LOOK_LINE_X0   36
#define LOOK_LINE_X1   604
#define LOOK_TOP       74    /* the line under the header */
#define LOOK_BOTTOM    372   /* the line over the legend */

int look_fade(float t);                        /* opacity of a scene's content t seconds after it opened */
void look_space(void);                         /* the background: nebula, drifting lights, twinkling stars */
void look_frame(void);                         /* the header (name, version) and the two lines */
void look_news(const char *tag);               /* a newer version exists: the header shows it next to this one */
typedef struct {
    int button;                                /* BUTTON_* */
    const char *text;
} legend_t;
/* the buttons at the bottom, right-aligned; with apart, the last one stands apart at the right edge */
void look_legend(const legend_t *items, int n, int apart);
void look_panel(float x, float y, float w, float h);   /* a box for messages */
/* an item of a menu; selected = it glows. center: x is the center. Returns the width */
int look_item(float x, float y, const char *text, int selected, int center);
float look_pulse(void);                        /* 0..1..0, slowly: what makes the selected item breathe */
/* text that breathes, from dim to bright and back, with its light (the selected item) */
int look_glow_text(int font, float x, float y, u32 dim, u32 bright, const char *text);
void look_title(float x, float y, const char *text, int center);   /* yellow */
/* the big glass memory card: number (CardN's) in big digits, or a short label */
void look_card(float x, float y, const char *number, const char *label);
#define LOOK_CARD_W 180
#define LOOK_CARD_H 216
void look_bar(float x, float y, float w, int permille);
void look_arrow(float cx, float y, int down, u32 color);    /* "more above / below" */
void look_halo(float cx, float cy, float r, int alpha);     /* white light behind the selected save */

#endif
