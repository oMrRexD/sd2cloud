/* SD2Cloud -- the 3D icon of a PS2 save (icon.c) */
#ifndef ICON_H
#define ICON_H

#include <gsKit.h>

struct icon_frame {
    int shape, nkeys;
    float *t, *v;            /* key times and values */
};

typedef struct {
    int ok;
    int swing;               /* swings instead of turning all the way round (the flat memory card) */
    int shapes, nv;          /* animation shapes and vertices (3 per triangle) */
    float *pos;              /* shapes * nv * 3, y up, z toward the viewer */
    float *nrm;              /* nv * 3 */
    float *uv;               /* nv * 2, 0..1 */
    unsigned char *col;      /* nv * 4, 0x80 = 1.0 */
    int frameLength, nframes;
    float speed;
    struct icon_frame *frames;
    unsigned short tex[128 * 128] __attribute__((aligned(64)));   /* A1B5G5R5 */
    float lightDir[3][3], lightCol[3][3], ambient[3];
    float center[3], radius;
    char title[100];         /* from icon.sys, when it's in letters the font has */
    char line1[72], line2[72];   /* the same, as the two lines icon.sys splits it in */
    GSTEXTURE gtex;          /* the texture for gsKit's texture manager (Mem = tex) */
    GSPRIMUVPOINT *still;    /* the still icon, ready for the GS (icon_draw with t < 0) */
    int stillCount;
    float stillAt[3];        /* where and how big it was worked out */
} icon_t;

icon_t *icon_load(const buffer_t *iconsys, const buffer_t *ico);   /* NULL = couldn't read it */
icon_t *icon_make_sd2psx(void);   /* the memory card with "SD2PSX" on the label; with the screen locked (the font) */
void icon_free(icon_t *ic);       /* with the screen locked when it may be on screen */
/* draws it in a square of side size centered on (cx, cy), seen from above like in the PS2 browser: turning and
 * animated at t seconds, or still and facing the front with t < 0 */
void icon_draw(icon_t *ic, float cx, float cy, float size, float t);
/* the saves grid, through one camera like the PS2 browser's (5 columns, 4 rows; cell 0, 0 = top left) */
void icon_draw_cell(icon_t *ic, int col, int row, float t);
/* a point of a cell's icon on screen: height 0 = its base, 2.5 = the middle of its 5-unit box */
void icon_cell_point(int col, int row, float height, float *x, float *y);

#endif
