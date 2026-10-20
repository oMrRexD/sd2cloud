/*
 * SD2Cloud -- text with a TrueType font, through FreeType:
 * each glyph is rendered once, the first time it's needed, into a 256x256 texture of 8-bit coverage (the color table
 * turns the coverage into white with that much alpha), and every letter is a sprite of that texture, tinted by the
 * text's color. gsKit's texture manager sends the textures to the GS when they're drawn, and again after a new glyph
 * was added to one.
 * The Japanese characters are in a font of their own (assets/font-ja.ttf.gz), 1.5 MB once read: it is read into memory
 * only when they are needed (font_japanese), and then each font has a face of it beside its own.
 * Not thread-safe: only the drawing thread uses it (or the main thread with the screen locked, see ui.c).
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <kernel.h>
#include <zlib.h>
#include <gsKit.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include "font.h"

GSGLOBAL *ui_gs_global(void);
void log_msg(const char *fmt, ...);

#define ATLAS 256
#define MAX_ATLASES 16   /* (made as they are needed: a font with Japanese on screen may fill several) */
#define MAX_FONTS 10
#define DIRECT 0x180   /* glyphs of codes below this have a slot of their own, as do the Cyrillic letters; the others
                         (Japanese, a few symbols) go in a table that grows as they are drawn */
#define CYRILLIC 0x400
#define N_CYRILLIC 0x130   /* Cyrillic and Cyrillic Supplement, the ones the font has (tools/make_font.py) */

typedef struct {
    short x, y, w, h;      /* where the bitmap is in the atlas */
    short left, top;       /* the bitmap's corner from the pen (top = above the baseline) */
    int advance;           /* 1/64 pixel */
    unsigned int index;    /* FreeType's glyph index (kerning) */
    unsigned char atlas, ok;   /* ok: 1 = made, 2 = it can't be (no room left): not tried again */
    unsigned char ja;      /* of the Japanese face */
    unsigned char stand;   /* a '?' in its place: no face had it (the Japanese one, read later, may) */
    unsigned char faint;   /* the bitmap's first columns that are less than half covered (up to 2): the stroke starts after */
} glyph_t;

typedef struct {
    unsigned int code;     /* 0 = a free place */
    glyph_t g;
} slot_t;

typedef struct {
    GSTEXTURE tex;
    int shelfY, shelfH, penX;   /* packing in rows ("shelves") */
} atlas_t;

typedef struct {
    FT_Face face, ja;      /* ja: of the Japanese characters (NULL until they are read) */
    int pixels, bold, line, ascent, kerning;
    int stroke;            /* where a 'C' starts, from the pen: a text whose first letter starts further on is drawn that
                              much to the left (walk) */
    glyph_t direct[DIRECT];
    glyph_t cyrillic[N_CYRILLIC];
    slot_t *table;              /* the other glyphs: open addressing, a power of two places (NULL until one is needed) */
    int tableSize, tableUsed, natlases;
    atlas_t *atlases[MAX_ATLASES];
} fnt_t;

static FT_Library lib;
static fnt_t *fonts[MAX_FONTS];
static int nfonts;
extern unsigned char asset_fontja_gz[];
extern unsigned int size_asset_fontja_gz;
static unsigned char *jaTtf;   /* the Japanese characters' font, read (NULL = not now) */
static unsigned int jaSize;
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

/* loads the glyph into the slot of the face that has it (the font's own, or the Japanese one): outline, emboldened,
 * rendered to coverage. g gets which face and glyph it was. NULL = it couldn't be */
static FT_GlyphSlot render(fnt_t *f, unsigned int code, glyph_t *g)
{
    FT_Face face = f->face;
    FT_GlyphSlot s;
    g->ja = g->stand = 0;
    if (!(g->index = FT_Get_Char_Index(face, code))) {
        if (f->ja && (g->index = FT_Get_Char_Index(f->ja, code)) != 0) {
            face = f->ja;
            g->ja = 1;
        } else {
            g->index = FT_Get_Char_Index(face, '?');
            g->stand = 1;
        }
    }
    s = face->glyph;
    if (FT_Load_Glyph(face, g->index, FT_LOAD_NO_BITMAP))
        return NULL;
    if (f->bold && s->format == FT_GLYPH_FORMAT_OUTLINE)
        FT_Outline_Embolden(&s->outline, f->bold);
    if (FT_Render_Glyph(s, FT_RENDER_MODE_NORMAL))
        return NULL;
    return s;
}

static unsigned int slot_of(unsigned int code, int size) { return (code * 2654435761u >> 12) & (size - 1); }

/* a code's place in the table (taken for it if it isn't there); NULL = no memory. The table doubles when it is 3/4
 * full, which moves the glyphs: a pointer to one lasts until the next call */
static glyph_t *table_glyph(fnt_t *f, unsigned int code)
{
    unsigned int i;
    if (f->tableUsed * 4 >= f->tableSize * 3) {
        int size = f->tableSize ? f->tableSize * 2 : 64, k;
        slot_t *t = calloc(size, sizeof(slot_t));
        if (!t)
            return NULL;
        for (k = 0; k < f->tableSize; k++)
            if (f->table[k].code) {
                for (i = slot_of(f->table[k].code, size); t[i].code; i = (i + 1) & (size - 1))
                    ;
                t[i] = f->table[k];
            }
        free(f->table);
        f->table = t;
        f->tableSize = size;
    }
    for (i = slot_of(code, f->tableSize); f->table[i].code && f->table[i].code != code; i = (i + 1) & (f->tableSize - 1))
        ;
    if (!f->table[i].code) {
        f->table[i].code = code;
        f->tableUsed++;
    }
    return &f->table[i].g;
}

static glyph_t *glyph(fnt_t *f, unsigned int code)
{
    glyph_t *g = NULL;
    FT_GlyphSlot s;
    int i, atlas = 0, x = 0, y = 0;
    if (code < DIRECT)
        g = &f->direct[code];
    else if (code - CYRILLIC < N_CYRILLIC)
        g = &f->cyrillic[code - CYRILLIC];
    else if (!code || !(g = table_glyph(f, code)))
        return NULL;
    if (g->ok)
        return g->ok == 1 ? g : NULL;
    if (!(s = render(f, code, g))) {
        g->ok = 2;
        return NULL;
    }
    g->w = s->bitmap.width;
    g->h = s->bitmap.rows;
    g->left = s->bitmap_left;
    g->top = s->bitmap_top;
    g->advance = s->advance.x + f->bold;
    for (g->faint = 0; g->faint < 2 && g->faint < g->w; g->faint++) {
        int most = 0;
        for (i = 0; i < g->h; i++)
            if (s->bitmap.buffer[i * s->bitmap.pitch + g->faint] > most)
                most = s->bitmap.buffer[i * s->bitmap.pitch + g->faint];
        if (most >= 128)
            break;
    }
    if (g->w && g->h) {
        atlas_t *a;
        unsigned char *dst;
        if (place(f, g->w, g->h, &atlas, &x, &y) != 0) {
            g->ok = 2;
            return NULL;
        }
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

/* the Japanese face of a font, when they are read */
static void japanese_face(fnt_t *f)
{
    if (jaTtf && (FT_New_Memory_Face(lib, jaTtf, jaSize, 0, &f->ja) || FT_Set_Pixel_Sizes(f->ja, 0, f->pixels)))
        f->ja = NULL;
}

int font_japanese(long spare)
{
    const unsigned char *gz = asset_fontja_gz;
    unsigned int n = size_asset_fontja_gz, size = gz[n - 4] | gz[n - 3] << 8 | gz[n - 2] << 16 | (unsigned)gz[n - 1] << 24;
    struct mallinfo m = mallinfo();
    z_stream z;
    int i, k, r;
    if (jaTtf)
        return 0;
    /* (gzip's last 4 bytes: how long it is unpacked) what is left must be more than that and spare */
    if (spare && (long)((char *)EndOfHeap() - (char *)sbrk(0)) + (long)m.fordblks < (long)size + spare)
        return -1;
    if (!(jaTtf = malloc(size)))
        return -1;
    memset(&z, 0, sizeof(z));
    z.next_in = (unsigned char *)gz;
    z.avail_in = n;
    z.next_out = jaTtf;
    z.avail_out = size;
    r = inflateInit2(&z, 16 + MAX_WBITS) == Z_OK ? inflate(&z, Z_FINISH) : Z_DATA_ERROR;
    inflateEnd(&z);
    if (r != Z_STREAM_END || z.total_out != size) {
        log_msg("font: the Japanese characters couldn't be unpacked (%d)", r);
        free(jaTtf);
        jaTtf = NULL;
        return -1;
    }
    jaSize = size;
    for (i = 0; i < nfonts; i++) {
        japanese_face(fonts[i]);
        for (k = 0; k < fonts[i]->tableSize; k++)   /* the '?' put in place of theirs: made again, now with theirs */
            if (fonts[i]->table[k].code && fonts[i]->table[k].g.stand)
                fonts[i]->table[k].g.ok = 0;
    }
    log_msg("font: the Japanese characters read (%u bytes)", size);
    return 0;
}

void font_japanese_free(void)
{
    int i;
    if (!jaTtf)
        return;
    for (i = 0; i < nfonts; i++)   /* (the glyphs made with it are drawn as they were: they are in the atlases) */
        if (fonts[i]->ja) {
            FT_Done_Face(fonts[i]->ja);
            fonts[i]->ja = NULL;
        }
    free(jaTtf);
    jaTtf = NULL;
    log_msg("font: the Japanese characters let go of");
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
    f->pixels = pixels;
    f->bold = bold;
    f->kerning = FT_HAS_KERNING(f->face);
    japanese_face(f);
    f->line = (f->face->size->metrics.height + 63) >> 6;
    f->ascent = (f->face->size->metrics.ascender + 63) >> 6;
    {
        glyph_t *g = glyph(f, 'C');
        f->stroke = g ? g->left + g->faint : 0;
    }
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
    int pen = 0, base, back = -1;   /* pen in 1/64 pixel */
    unsigned int prev = 0;   /* (the glyph before, of the font's own face: kerning is between two of those) */
    if (id < 0 || id >= nfonts || !utf8)
        return 0;
    f = fonts[id];
    base = (int)y + f->ascent;
    while (*p) {
        unsigned int c = next_utf8(&p);
        glyph_t *g = glyph(f, c);
        if (!g)
            continue;
        if (f->kerning && prev && g->index && !g->ja) {
            FT_Vector k;
            if (!FT_Get_Kerning(f->face, prev, g->index, FT_KERNING_DEFAULT, &k))
                pen += k.x;
        }
        prev = g->ja ? 0 : g->index;
        if (draw && g->w) {
            atlas_t *a = f->atlases[g->atlas];
            float gx, gy = (float)(base - g->top);
            /* the text's first stroke where a 'C' would start: texts one under the other start in the same column,
             * an "I" (a stem after a faint column) as a "C" does */
            if (back < 0) {
                back = g->left + g->faint - f->stroke;
                back = back < 0 ? 0 : back > 2 ? 2 : back;
            }
            gx = (float)((int)x - back + ((pen + 32) >> 6) + g->left);
            gsKit_TexManager_bind(gs, &a->tex);
            gsKit_prim_sprite_texture(gs, &a->tex, gx, gy, g->x, g->y, gx + g->w, gy + g->h, g->x + g->w, g->y + g->h, 1, color);
        }
        pen += g->advance;
        /* a word starts on a whole pixel. The advances have a fraction (the bold added to them), and rounding each
         * letter's place could take a pixel off the space before a word: of the 4 a 15 px font gives it, enough for
         * two short words to read as one on a TV */
        if (c == ' ')
            pen = (pen + 63) & ~63;
    }
    return (pen + 32) >> 6;
}

int font_draw(int id, float x, float y, u64 color, const char *utf8) { return walk(id, x, y, color, utf8, 1); }
int font_width(int id, const char *utf8) { return walk(id, 0, 0, 0, utf8, 0); }
int font_has(unsigned int code)
{
    return nfonts && (FT_Get_Char_Index(fonts[0]->face, code) || (fonts[0]->ja && FT_Get_Char_Index(fonts[0]->ja, code)));
}
int font_line(int id) { return id >= 0 && id < nfonts ? fonts[id]->line : 0; }
