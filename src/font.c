/*
 * SD2Cloud -- text with a TrueType font, through FreeType:
 * each glyph is rendered once, the first time it's needed, into a 256x256 texture of 8-bit coverage (the color table
 * turns the coverage into white with that much alpha), and every letter is a sprite of that texture, tinted by the
 * text's color. gsKit's texture manager sends the textures to the GS when they're drawn, and again after a new glyph
 * was added to one.
 * Not thread-safe: only the drawing thread uses it (or the main thread with the screen locked, see ui.c).
 */
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gsKit.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include "font.h"

GSGLOBAL *ui_gs_global(void);
void log_msg(const char *fmt, ...);

#define ATLAS 256
#define MAX_ATLASES 6
#define MAX_FONTS 10
#define DIRECT 0x180   /* glyphs of codes below this have a slot of their own; the others share a short list */
#define EXTRA 48

typedef struct {
    short x, y, w, h;      /* where the bitmap is in the atlas */
    short left, top;       /* the bitmap's corner from the pen (top = above the baseline) */
    int advance;           /* 1/64 pixel */
    unsigned int index;    /* FreeType's glyph index (kerning) */
    unsigned char atlas, ok;
} glyph_t;

typedef struct {
    GSTEXTURE tex;
    int shelfY, shelfH, penX;   /* packing in rows ("shelves") */
} atlas_t;

typedef struct {
    FT_Face face;
    int bold, line, ascent, kerning;
    glyph_t direct[DIRECT];
    struct {
        unsigned int code;
        glyph_t g;
    } extra[EXTRA];
    int nextra, natlases;
    atlas_t *atlases[MAX_ATLASES];
} fnt_t;

static FT_Library lib;
static fnt_t *fonts[MAX_FONTS];
static int nfonts;
static u32 clut[256] __attribute__((aligned(64)));

int font_init(void)
{
    int i;
    if (FT_Init_FreeType(&lib))
        return -1;
    /* white with alpha = coverage (the GS's alpha goes up to 0x80); the GS reads a 256-color table with entries 8-15
     * and 16-23 of every 32 swapped, so each value goes to the swapped place */
    for (i = 0; i < 256; i++) {
        int pos = (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
        clut[pos] = 0x00FFFFFF | ((u32)((i * 128 + 127) / 255) << 24);
    }
    return 0;
}

static atlas_t *new_atlas(fnt_t *f)
{
    atlas_t *a;
    if (f->natlases == MAX_ATLASES || !(a = calloc(1, sizeof(*a))))
        return NULL;
    a->tex.Width = ATLAS;
    a->tex.Height = ATLAS;
    a->tex.PSM = GS_PSM_T8;
    a->tex.ClutPSM = GS_PSM_CT32;
    a->tex.Clut = clut;
    a->tex.ClutStorageMode = GS_CLUT_STORAGE_CSM1;
    a->tex.Filter = GS_FILTER_NEAREST;
    a->tex.Mem = memalign(128, ATLAS * ATLAS);
    if (!a->tex.Mem) {
        free(a);
        return NULL;
    }
    memset(a->tex.Mem, 0, ATLAS * ATLAS);
    f->atlases[f->natlases++] = a;
    return a;
}

/* a place of w x h (with a pixel of space around it) in the font's atlases */
static int place(fnt_t *f, int w, int h, int *atlas, int *x, int *y)
{
    atlas_t *a = f->natlases ? f->atlases[f->natlases - 1] : new_atlas(f);
    if (!a || w + 2 > ATLAS || h + 2 > ATLAS)
        return -1;
    if (a->penX + w + 2 > ATLAS) {   /* the shelf is full: a new one below */
        a->shelfY += a->shelfH;
        a->shelfH = 0;
        a->penX = 0;
    }
    if (a->shelfY + h + 2 > ATLAS) {   /* the atlas is full: a new one */
        if (!(a = new_atlas(f)))
            return -1;
    }
    *atlas = f->natlases - 1;
    *x = a->penX + 1;
    *y = a->shelfY + 1;
    a->penX += w + 2;
    if (h + 2 > a->shelfH)
        a->shelfH = h + 2;
    return 0;
}

/* loads the glyph into FreeType's slot: outline, emboldened, rendered to coverage */
static int render(fnt_t *f, unsigned int code, unsigned int *index)
{
    FT_GlyphSlot s = f->face->glyph;
    *index = FT_Get_Char_Index(f->face, code);
    if (!*index)
        *index = FT_Get_Char_Index(f->face, '?');
    if (FT_Load_Glyph(f->face, *index, FT_LOAD_NO_BITMAP))
        return -1;
    if (f->bold && s->format == FT_GLYPH_FORMAT_OUTLINE)
        FT_Outline_Embolden(&s->outline, f->bold);
    if (FT_Render_Glyph(s, FT_RENDER_MODE_NORMAL))
        return -1;
    return 0;
}

static glyph_t *glyph(fnt_t *f, unsigned int code)
{
    glyph_t *g = NULL;
    FT_GlyphSlot s = f->face->glyph;
    int i, atlas = 0, x = 0, y = 0;
    if (code < DIRECT)
        g = &f->direct[code];
    else {
        for (i = 0; i < f->nextra; i++)
            if (f->extra[i].code == code)
                return &f->extra[i].g;
        if (f->nextra == EXTRA)
            return NULL;
        f->extra[f->nextra].code = code;
        g = &f->extra[f->nextra++].g;
    }
    if (g->ok)
        return g;
    if (render(f, code, &g->index) != 0)
        return NULL;
    g->w = s->bitmap.width;
    g->h = s->bitmap.rows;
    g->left = s->bitmap_left;
    g->top = s->bitmap_top;
    g->advance = s->advance.x + f->bold;
    if (g->w && g->h) {
        atlas_t *a;
        unsigned char *dst;
        if (place(f, g->w, g->h, &atlas, &x, &y) != 0)
            return NULL;
        a = f->atlases[atlas];
        dst = (unsigned char *)a->tex.Mem + y * ATLAS + x;
        for (i = 0; i < g->h; i++)
            memcpy(dst + i * ATLAS, s->bitmap.buffer + i * s->bitmap.pitch, g->w);
        gsKit_TexManager_invalidate(ui_gs_global(), &a->tex);   /* send it again, with the new glyph */
    }
    g->atlas = atlas;
    g->x = x;
    g->y = y;
    g->ok = 1;
    return g;
}

int font_load(const void *ttf, int ttfSize, int pixels, int bold)
{
    fnt_t *f;
    int c;
    if (nfonts == MAX_FONTS || !(f = calloc(1, sizeof(*f))))
        return -1;
    if (FT_New_Memory_Face(lib, ttf, ttfSize, 0, &f->face) || FT_Set_Pixel_Sizes(f->face, 0, pixels)) {
        log_msg("font: couldn't load the font at %d px", pixels);
        free(f);
        return -1;
    }
    f->bold = bold;
    f->kerning = FT_HAS_KERNING(f->face);
    f->line = (f->face->size->metrics.height + 63) >> 6;
    f->ascent = (f->face->size->metrics.ascender + 63) >> 6;
    fonts[nfonts] = f;
    /* the Latin-1 letters right away (the small fonts): drawing later only looks them up */
    if (pixels <= 32)
        for (c = 32; c < 256; c++)
            if (c < 127 || c >= 160)
                glyph(f, c);
    return nfonts++;
}

/* next character of a UTF-8 text */
static unsigned int next_utf8(const unsigned char **p)
{
    const unsigned char *s = *p;
    unsigned int c = *s++;
    if (c >= 0x80) {
        int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
        unsigned int v = c & (0x3F >> extra);
        if (!extra)
            v = '?';
        while (extra-- && (*s & 0xC0) == 0x80)
            v = (v << 6) | (*s++ & 0x3F);
        c = v;
    }
    *p = s;
    return c;
}

/* walks a text: draws it (when draw) and returns the width */
static int walk(int id, float x, float y, u64 color, const char *utf8, int draw)
{
    fnt_t *f;
    GSGLOBAL *gs = ui_gs_global();
    const unsigned char *p = (const unsigned char *)utf8;
    int pen = 0, base;   /* pen in 1/64 pixel */
    unsigned int prev = 0;
    if (id < 0 || id >= nfonts || !utf8)
        return 0;
    f = fonts[id];
    base = (int)y + f->ascent;
    while (*p) {
        unsigned int c = next_utf8(&p);
        glyph_t *g = glyph(f, c);
        if (!g)
            continue;
        if (f->kerning && prev && g->index) {
            FT_Vector k;
            if (!FT_Get_Kerning(f->face, prev, g->index, FT_KERNING_DEFAULT, &k))
                pen += k.x;
        }
        prev = g->index;
        if (draw && g->w) {
            atlas_t *a = f->atlases[g->atlas];
            float gx = (float)((int)x + ((pen + 32) >> 6) + g->left), gy = (float)(base - g->top);
            gsKit_TexManager_bind(gs, &a->tex);
            gsKit_prim_sprite_texture(gs, &a->tex, gx, gy, g->x, g->y, gx + g->w, gy + g->h, g->x + g->w, g->y + g->h, 1, color);
        }
        pen += g->advance;
    }
    return (pen + 32) >> 6;
}

int font_draw(int id, float x, float y, u64 color, const char *utf8) { return walk(id, x, y, color, utf8, 1); }
int font_width(int id, const char *utf8) { return walk(id, 0, 0, 0, utf8, 0); }
int font_line(int id) { return id >= 0 && id < nfonts ? fonts[id]->line : 0; }

int font_rasterize(int id, const char *utf8, void (*plot)(int x, int y, int coverage, void *u), void *u)
{
    fnt_t *f;
    const unsigned char *p = (const unsigned char *)utf8;
    int pen = 0, i, j;
    if (id < 0 || id >= nfonts)
        return 0;
    f = fonts[id];
    while (*p) {
        unsigned int index;
        FT_GlyphSlot s = f->face->glyph;
        if (render(f, next_utf8(&p), &index) != 0)
            continue;
        for (j = 0; j < (int)s->bitmap.rows; j++)
            for (i = 0; i < (int)s->bitmap.width; i++) {
                int a = s->bitmap.buffer[j * s->bitmap.pitch + i];
                if (a)
                    plot(((pen + 32) >> 6) + s->bitmap_left + i, j - s->bitmap_top, a, u);
            }
        pen += s->advance.x + f->bold;
    }
    return (pen + 32) >> 6;
}
