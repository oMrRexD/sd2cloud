/*
 * SD2Cloud -- the 3D icon of a PS2 save, drawn the way the PS2 browser shows it: the animation blends the icon's
 * shapes, it is lit by the three lights and the ambient color of icon.sys, and it slowly turns.
 *
 * The icon file (the one icon.sys names) as documented by mymc/mymc+ (Ross Ridge and contributors):
 *   header: magic 0x00010000, shapes, texture type, (1.0f), vertex count
 *   per vertex: shapes x (x, y, z, w) s16, normal (x, y, z, w) s16, uv s16 x2, color u8 x4   (s16 = 4.12 fixed point)
 *   animation: id 1, frame length, speed (float), play offset, frame count; per frame: shape, key count (+1), 2 words,
 *              then (time, value) float pairs
 *   texture: 128x128 A1B5G5R5, raw (type 7) or RLE (u32 size, then u16 codes)
 * When a card is shared by many games (CardN, named folders) or the icon can't be read, a memory card model made here
 * with "SD2PSX" on its label is shown instead.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>
#include "common.h"
#include "ui.h"
#include "icon.h"

GSGLOBAL *ui_gs_global(void);

#define TEX 128

static unsigned short le16u(const unsigned char *p) { return p[0] | (p[1] << 8); }
static short le16s(const unsigned char *p) { return (short)(p[0] | (p[1] << 8)); }
static unsigned int le32u(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }
static float lef(const unsigned char *p)
{
    union {
        unsigned int u;
        float f;
    } v;
    v.u = le32u(p);
    return v.f;
}

void icon_free(icon_t *ic)
{
    int i;
    if (!ic)
        return;
    gsKit_TexManager_free(ui_gs_global(), &ic->gtex);
    free(ic->still);
    free(ic->pos);
    free(ic->nrm);
    free(ic->uv);
    free(ic->col);
    for (i = 0; ic->frames && i < ic->nframes; i++) {   /* no frames yet when the file was refused for its count */
        free(ic->frames[i].t);
        free(ic->frames[i].v);
    }
    free(ic->frames);
    free(ic);
}

static icon_t *new_icon(void)
{
    icon_t *ic = memalign(64, sizeof(icon_t));
    if (!ic)
        return NULL;
    memset(ic, 0, sizeof(*ic));
    ic->gtex.Width = TEX;
    ic->gtex.Height = TEX;
    ic->gtex.PSM = GS_PSM_CT16;
    ic->gtex.Filter = GS_FILTER_LINEAR;
    ic->gtex.Mem = (u32 *)ic->tex;
    return ic;
}

/* ------------------------------------------------------------ icon.sys */

/* Shift-JIS to plain text: ASCII, the full-width letters, digits and the usual punctuation (most western games write
 * their titles that way). Anything else (kana, kanji) makes it give up: -1 */
static int sjis_to_ascii(const unsigned char *s, int n, char *out, int size)
{
    static const struct {
        unsigned short code;
        char c;
    } punct[] = {
        {0x8140, ' '}, {0x8143, ','}, {0x8144, '.'}, {0x8146, ':'}, {0x8147, ';'}, {0x8148, '?'}, {0x8149, '!'}, {0x814F, '^'},
        {0x8151, '_'}, {0x815B, '-'}, {0x815C, '-'}, {0x815D, '-'}, {0x815E, '/'}, {0x8160, '~'}, {0x8162, '|'}, {0x8165, '\''},
        {0x8166, '\''}, {0x8167, '"'}, {0x8168, '"'}, {0x8169, '('}, {0x816A, ')'}, {0x816D, '['}, {0x816E, ']'}, {0x816F, '{'},
        {0x8170, '}'}, {0x817B, '+'}, {0x817C, '-'}, {0x8181, '='}, {0x8183, '<'}, {0x8184, '>'}, {0x8190, '$'}, {0x8193, '%'},
        {0x8194, '#'}, {0x8195, '&'}, {0x8196, '*'}, {0x8197, '@'},
    };
    int i = 0, k = 0, j;
    while (i < n && s[i] && k + 1 < size) {
        unsigned int c = s[i];
        if (c < 0x80) {
            out[k++] = c >= 0x20 ? (char)c : ' ';
            i++;
            continue;
        }
        if (i + 1 >= n)
            break;
        c = (c << 8) | s[i + 1];
        i += 2;
        if (c >= 0x824F && c <= 0x8258)
            out[k++] = '0' + (c - 0x824F);
        else if (c >= 0x8260 && c <= 0x8279)
            out[k++] = 'A' + (c - 0x8260);
        else if (c >= 0x8281 && c <= 0x829A)
            out[k++] = 'a' + (c - 0x8281);
        else {
            for (j = 0; j < (int)(sizeof(punct) / sizeof(punct[0])) && punct[j].code != c; j++)
                ;
            if (j == (int)(sizeof(punct) / sizeof(punct[0])))
                return -1;
            out[k++] = punct[j].c;
        }
    }
    out[k] = 0;
    return k;
}

/* the title (2 lines in the file, joined with a space), the lights and the ambient color */
static void read_iconsys(icon_t *ic, const unsigned char *d)
{
    char a[80], b[80];
    int brk = le16u(d + 6), i, j;
    if (brk > 68)
        brk = 68;
    ic->title[0] = ic->line1[0] = ic->line2[0] = 0;
    if (sjis_to_ascii(d + 192, brk, a, sizeof(a)) >= 0 && sjis_to_ascii(d + 192 + brk, 68 - brk, b, sizeof(b)) >= 0) {
        trim(a);
        trim(b);
        snprintf(ic->title, sizeof(ic->title), "%s%s%s", a, a[0] && b[0] ? " " : "", b);
        snprintf(ic->line1, sizeof(ic->line1), "%s", a);
        snprintf(ic->line2, sizeof(ic->line2), "%s", b);
    }
    for (i = 0; i < 3; i++) {
        float x = lef(d + 80 + i * 16), y = lef(d + 84 + i * 16), z = lef(d + 88 + i * 16), l = sqrtf(x * x + y * y + z * z);
        ic->lightDir[i][0] = l > 0.0001f ? x / l : 0;
        ic->lightDir[i][1] = l > 0.0001f ? y / l : 0;
        ic->lightDir[i][2] = l > 0.0001f ? z / l : 0;
        for (j = 0; j < 3; j++)
            ic->lightCol[i][j] = lef(d + 128 + i * 16 + j * 4);
    }
    for (j = 0; j < 3; j++)
        ic->ambient[j] = lef(d + 176 + j * 4);
}

/* ------------------------------------------------------------ the icon file */

static int read_texture(icon_t *ic, const unsigned char *d, size_t n, size_t off, unsigned int type)
{
    size_t size = TEX * TEX * 2, t = 0, end;
    if (!(type & 4)) {   /* no texture: plain white */
        memset(ic->tex, 0xFF, sizeof(ic->tex));
        return 0;
    }
    if (type == 7) {
        if (off + size > n)
            return -1;
        memcpy(ic->tex, d + off, size);
        return 0;
    }
    if (off + 4 > n || off + 4 + le32u(d + off) > n)
        return -1;
    end = off + 4 + le32u(d + off);
    /* its size, then 16-bit codes. With the top bit set, (0x10000 - code) values follow as they are, however many
     * (a busy picture has runs of thousands); a 0 comes alone, as some games' tools write between two repeats; any
     * other code is how many times the next value repeats */
    for (off += 4; off + 2 <= end;) {
        unsigned int code = le16u(d + off), k;
        off += 2;
        if (code & 0x8000) {
            k = (0x10000 - code) * 2;
            if (off + k > end || t + k > size)
                return -1;
            memcpy((unsigned char *)ic->tex + t, d + off, k);
            off += k;
            t += k;
        } else if (code) {
            if (off + 2 > end || t + code * 2 > size)
                return -1;
            for (k = 0; k < code; k++, t += 2)
                memcpy((unsigned char *)ic->tex + t, d + off, 2);
            off += 2;
        }
    }
    return 0;
}

static void bounds(icon_t *ic)
{
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f}, r = 0;
    int i, j;
    for (i = 0; i < ic->nv; i++)
        for (j = 0; j < 3; j++) {
            float v = ic->pos[i * 3 + j];
            if (v < lo[j])
                lo[j] = v;
            if (v > hi[j])
                hi[j] = v;
        }
    for (j = 0; j < 3; j++)
        ic->center[j] = (lo[j] + hi[j]) / 2;
    for (i = 0; i < ic->nv; i++) {
        float dx = ic->pos[i * 3] - ic->center[0], dy = ic->pos[i * 3 + 1] - ic->center[1], dz = ic->pos[i * 3 + 2] - ic->center[2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > r)
            r = d;
    }
    ic->radius = r > 1 ? r : 1;
}

icon_t *icon_load(const buffer_t *iconsys, const buffer_t *ico)
{
    const unsigned char *d = ico->data;
    size_t n = ico->len, off = 20, stride;
    unsigned int type;
    int i, s, f;
    icon_t *ic;
    if (!iconsys->data || iconsys->len < 964 || !d || n < 20 || le32u(d) != 0x00010000 || !(ic = new_icon()))
        return NULL;
    read_iconsys(ic, iconsys->data);
    ic->shapes = le32u(d + 4);
    type = le32u(d + 8);
    ic->nv = le32u(d + 16);
    if (ic->shapes < 1 || ic->shapes > 32 || ic->nv < 3 || ic->nv > 20000)
        goto bad;
    stride = ic->shapes * 8 + 16;
    if (off + (size_t)ic->nv * stride > n)
        goto bad;
    ic->pos = malloc(sizeof(float) * 3 * ic->nv * ic->shapes);
    ic->nrm = malloc(sizeof(float) * 3 * ic->nv);
    ic->uv = malloc(sizeof(float) * 2 * ic->nv);
    ic->col = malloc(4 * ic->nv);
    if (!ic->pos || !ic->nrm || !ic->uv || !ic->col)
        goto bad;
    /* (x, -y, -z): the icon's y points down and its z away from the viewer */
    for (i = 0; i < ic->nv; i++, off += stride) {
        const unsigned char *v = d + off;
        for (s = 0; s < ic->shapes; s++) {
            float *p = ic->pos + ((size_t)s * ic->nv + i) * 3;
            p[0] = le16s(v + s * 8) / 4096.0f;
            p[1] = -le16s(v + s * 8 + 2) / 4096.0f;
            p[2] = -le16s(v + s * 8 + 4) / 4096.0f;
        }
        v += ic->shapes * 8;
        ic->nrm[i * 3] = le16s(v) / 4096.0f;
        ic->nrm[i * 3 + 1] = -le16s(v + 2) / 4096.0f;
        ic->nrm[i * 3 + 2] = -le16s(v + 4) / 4096.0f;
        ic->uv[i * 2] = le16s(v + 8) / 4096.0f;
        ic->uv[i * 2 + 1] = le16s(v + 10) / 4096.0f;
        memcpy(ic->col + i * 4, v + 12, 4);
    }
    /* animation */
    if (off + 20 > n || le32u(d + off) != 1)
        goto bad;
    ic->frameLength = le32u(d + off + 4);
    ic->speed = lef(d + off + 8);
    ic->nframes = le32u(d + off + 16);
    off += 20;
    if (ic->nframes < 0 || ic->nframes > 64)
        goto bad;
    ic->frames = calloc(ic->nframes ? ic->nframes : 1, sizeof(ic->frames[0]));
    if (!ic->frames)
        goto bad;
    for (f = 0; f < ic->nframes; f++) {
        int k, keys;
        if (off + 16 > n)
            goto bad;
        ic->frames[f].shape = le32u(d + off);
        keys = (int)le32u(d + off + 4) - 1;
        off += 16;
        if (keys < 0)
            keys = 0;
        if (keys > 256 || off + (size_t)keys * 8 > n)
            goto bad;
        ic->frames[f].nkeys = keys;
        ic->frames[f].t = malloc(sizeof(float) * (keys + 1));
        ic->frames[f].v = malloc(sizeof(float) * (keys + 1));
        if (!ic->frames[f].t || !ic->frames[f].v)
            goto bad;
        for (k = 0; k < keys; k++, off += 8) {
            ic->frames[f].t[k] = lef(d + off);
            ic->frames[f].v[k] = lef(d + off + 4);
        }
    }
    if (read_texture(ic, d, n, off, type) != 0)
        goto bad;
    bounds(ic);
    ic->ok = 1;
    FlushCache(0);
    return ic;
bad:
    icon_free(ic);
    return NULL;
}

/* ------------------------------------------------------------ the SD2PSX memory card */

static unsigned short rgb16(int r, int g, int b) { return 0x8000 | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3); }

/* a over b, a of 15 */
static unsigned short mix16(unsigned short b, unsigned short c, int a)
{
    int k, out = 0x8000;
    for (k = 0; k < 15; k += 5) {
        int x = (b >> k) & 31, y = (c >> k) & 31;
        out |= ((x * (15 - a) + y * a) / 15) << k;
    }
    return out;
}

/* text into the texture, centered on x with its top at y, squashed vertically by sy and, when it is wider than maxw,
 * horizontally too (font.c renders it: the screen must be locked) */
typedef struct {
    unsigned short *tex, color;
    float x0, base, sx, sy;
} tex_text_t;

static void tex_plot(int x, int y, int coverage, void *u)
{
    tex_text_t *p = u;
    int tx = (int)(p->x0 + x / p->sx), ty = (int)(p->base + y / p->sy);
    if (tx >= 0 && tx < TEX && ty >= 0 && ty < TEX)
        p->tex[ty * TEX + tx] = mix16(p->tex[ty * TEX + tx], p->color, coverage * 15 / 255);
}

static void tex_text(unsigned short *tex, int x, int y, float sy, int maxw, unsigned short color, const char *s)
{
    tex_text_t p;
    int width = ui_measure(FONT_TITLE, s);
    p.tex = tex;
    p.color = color;
    p.sy = sy;
    p.sx = width > maxw ? (float)width / maxw : 1;
    p.x0 = x - width / p.sx / 2;
    p.base = y + ui_line_height(FONT_TITLE) * 0.72f / sy;
    ui_rasterize(FONT_TITLE, s, tex_plot, &p);
}

icon_t *icon_make_sd2psx(void)
{
    /* outline of the card (y up), the top right corner cut like a PS2 memory card; z = +/- half the thickness */
    static const float px[5] = {-0.5f, 0.5f, 0.5f, 0.3f, -0.5f}, py[5] = {-0.65f, -0.65f, 0.45f, 0.65f, 0.65f};
    const float hz = 0.08f;
    unsigned short body = rgb16(84, 92, 106), edge = rgb16(52, 57, 66), label = rgb16(236, 238, 242), ink = rgb16(30, 34, 44);
    int i, k = 0, x, y;
    icon_t *ic = new_icon();
    if (!ic)
        return NULL;
    ic->shapes = 1;
    ic->nv = 9 + 9 + 5 * 6;
    ic->pos = malloc(sizeof(float) * 3 * ic->nv);
    ic->nrm = malloc(sizeof(float) * 3 * ic->nv);
    ic->uv = malloc(sizeof(float) * 2 * ic->nv);
    ic->col = malloc(4 * ic->nv);
    if (!ic->pos || !ic->nrm || !ic->uv || !ic->col) {
        icon_free(ic);
        return NULL;
    }
#define VTX(X, Y, Z, NX, NY, NZ, U, V)                                                              \
    do {                                                                                           \
        ic->pos[k * 3] = (X), ic->pos[k * 3 + 1] = (Y), ic->pos[k * 3 + 2] = (Z);                  \
        ic->nrm[k * 3] = (NX), ic->nrm[k * 3 + 1] = (NY), ic->nrm[k * 3 + 2] = (NZ);               \
        ic->uv[k * 2] = (U), ic->uv[k * 2 + 1] = (V);                                              \
        memset(ic->col + k * 4, 0x80, 4);                                                          \
        k++;                                                                                       \
    } while (0)
    /* front (the whole texture) and back (a plain spot of it), as fans from the first corner */
    for (i = 1; i < 4; i++) {
        VTX(px[0], py[0], hz, 0, 0, 1, px[0] + 0.5f, (0.65f - py[0]) / 1.3f);
        VTX(px[i], py[i], hz, 0, 0, 1, px[i] + 0.5f, (0.65f - py[i]) / 1.3f);
        VTX(px[i + 1], py[i + 1], hz, 0, 0, 1, px[i + 1] + 0.5f, (0.65f - py[i + 1]) / 1.3f);
    }
    for (i = 1; i < 4; i++) {
        VTX(px[0], py[0], -hz, 0, 0, -1, 0.03f, 0.03f);
        VTX(px[i + 1], py[i + 1], -hz, 0, 0, -1, 0.03f, 0.03f);
        VTX(px[i], py[i], -hz, 0, 0, -1, 0.03f, 0.03f);
    }
    /* the sides, a darker spot of the texture */
    for (i = 0; i < 5; i++) {
        int j = (i + 1) % 5;
        float dx = px[j] - px[i], dy = py[j] - py[i], l = sqrtf(dx * dx + dy * dy), nx = dy / l, ny = -dx / l;
        VTX(px[i], py[i], hz, nx, ny, 0, 0.97f, 0.03f);
        VTX(px[i], py[i], -hz, nx, ny, 0, 0.97f, 0.03f);
        VTX(px[j], py[j], hz, nx, ny, 0, 0.97f, 0.03f);
        VTX(px[j], py[j], hz, nx, ny, 0, 0.97f, 0.03f);
        VTX(px[i], py[i], -hz, nx, ny, 0, 0.97f, 0.03f);
        VTX(px[j], py[j], -hz, nx, ny, 0, 0.97f, 0.03f);
    }
#undef VTX
    /* the texture: the body, a darker spot for the sides and the label with the name */
    for (y = 0; y < TEX; y++)
        for (x = 0; x < TEX; x++)
            ic->tex[y * TEX + x] = body;
    for (y = 0; y < 8; y++)
        for (x = TEX - 8; x < TEX; x++)
            ic->tex[y * TEX + x] = edge;
    for (y = 58; y < 104; y++)
        for (x = 10; x < TEX - 10; x++)
            ic->tex[y * TEX + x] = label;
    tex_text(ic->tex, TEX / 2, 68, 1.3f, TEX - 34, ink, "SD2PSX");
    /* neutral lights: a key light from the top left, a soft one from below and the ambient */
    ic->lightDir[0][0] = -0.45f, ic->lightDir[0][1] = 0.6f, ic->lightDir[0][2] = 0.66f;
    ic->lightCol[0][0] = ic->lightCol[0][1] = ic->lightCol[0][2] = 0.6f;
    ic->lightDir[1][0] = 0.3f, ic->lightDir[1][1] = -0.8f, ic->lightDir[1][2] = 0.5f;
    ic->lightCol[1][0] = ic->lightCol[1][1] = 0.22f, ic->lightCol[1][2] = 0.3f;
    ic->ambient[0] = ic->ambient[1] = ic->ambient[2] = 0.5f;
    snprintf(ic->title, sizeof(ic->title), "SD2PSX");
    ic->swing = 1;
    bounds(ic);
    ic->ok = 1;
    FlushCache(0);
    return ic;
}

/* ------------------------------------------------------------ drawing */

/* the weight of each shape at a moment of the animation (the keys of each frame, interpolated in a loop) */
static void shape_weights(const icon_t *ic, float t, float *w)
{
    float len = ic->frameLength > 0 ? (float)ic->frameLength : 1, sum = 0;
    float now = fmodf(t * 60.0f * (ic->speed > 0 ? ic->speed : 1.0f), len);
    int f, k, s;
    for (s = 0; s < ic->shapes; s++)
        w[s] = 0;
    for (f = 0; f < ic->nframes; f++) {
        const struct icon_frame *fr = &ic->frames[f];
        float lastT = -1e9f, lastV = 0, nextT = 1e9f, nextV = 0, value;
        if (fr->shape < 0 || fr->shape >= ic->shapes || !fr->nkeys)
            continue;
        for (k = 0; k < fr->nkeys; k++) {
            float kt = fr->t[k] <= now ? fr->t[k] : fr->t[k] - len;
            float nt = fr->t[k] >= now ? fr->t[k] : fr->t[k] + len;
            if (kt > lastT)
                lastT = kt, lastV = fr->v[k];
            if (nt < nextT)
                nextT = nt, nextV = fr->v[k];
        }
        value = nextT > lastT ? lastV + (nextV - lastV) * (now - lastT) / (nextT - lastT) : lastV;
        w[fr->shape] += value;
    }
    for (s = 0; s < ic->shapes; s++)
        sum += w[s];
    if (sum <= 0.0001f) {
        for (s = 0; s < ic->shapes; s++)
            w[s] = 0;
        w[0] = 1;
    } else
        for (s = 0; s < ic->shapes; s++)
            w[s] /= sum;
}

/* ------------------------------------------------------------ the camera
 *
 * Icons are made for the PS2 browser's camera, so they are shown through one like it. Measured on a photo of the
 * browser (the positions of 18 icons of a card whose .mcd we have, and the icons themselves): one perspective camera
 * for the whole grid, looking at it from about 29 degrees above, 60 units away (icons are modeled in a 5 x 5 box, base
 * at 0); the rows recede like a floor (65 degrees from upright) and every icon keeps the size its maker gave it. Flat
 * icons are modeled tilted back to face that camera (a road sign 41 degrees, the PlayStation logo 22): seen from the
 * front they look squashed, from below they almost disappear. */
#define ASPECT  1.126f   /* a PS2 pixel is narrower than it is tall on a 4:3 TV (640 x 448) */
#define ELEV    0.506f   /* 29 degrees */

typedef struct {
    float eye[3], right[3], up[3], fwd[3];
    float fx, fy, cx, cy;   /* focal lengths and the screen point the camera looks at */
    float near, far;        /* the depths mapped onto the Z buffer */
} camera_t;

static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/* a camera dist units away from target, ELEV above it, k pixels per unit at the target */
static void camera_look(camera_t *c, const float *target, float dist, float k, float sx, float sy)
{
    float s = sinf(ELEV), co = cosf(ELEV);
    c->eye[0] = target[0], c->eye[1] = target[1] + dist * s, c->eye[2] = target[2] + dist * co;
    c->fwd[0] = 0, c->fwd[1] = -s, c->fwd[2] = -co;
    c->right[0] = 1, c->right[1] = 0, c->right[2] = 0;
    c->up[0] = 0, c->up[1] = co, c->up[2] = -s;
    c->fx = k * dist;
    c->fy = c->fx / ASPECT;
    c->cx = sx, c->cy = sy;
    c->near = dist - 40, c->far = dist + 40;
}

/* the saves grid: 5 columns, the rows receding; cell (0, 0) is the top left */
#define GRID_SX   5.5f
#define GRID_SY   5.0f
#define GRID_LEAN 1.134f   /* 65 degrees */

static camera_t gridCam;
static int gridReady;

static void grid_offset(int col, int row, float *o)
{
    /* where the model's origin goes: the cell, minus half the 5-unit box so the box's center sits on the cell */
    float r = (1.5f - row) * GRID_SY;
    o[0] = (col - 2) * GRID_SX;
    o[1] = r * cosf(GRID_LEAN) - 2.5f;
    o[2] = -r * sinf(GRID_LEAN);
}

static camera_t *grid_camera(void)
{
    if (!gridReady) {
        static const float center[3] = {0, 0, 0};
        camera_look(&gridCam, center, 60, 14.5f, 321, 209);
        gridReady = 1;
    }
    return &gridCam;
}

static void project(const camera_t *c, const float *w, float *sx, float *sy, float *z)
{
    float rel[3] = {w[0] - c->eye[0], w[1] - c->eye[1], w[2] - c->eye[2]};
    float d = dot3(rel, c->fwd);
    if (d < 0.1f)
        d = 0.1f;
    *sx = c->cx + c->fx * dot3(rel, c->right) / d;
    *sy = c->cy - c->fy * dot3(rel, c->up) / d;
    *z = d;
}

void icon_cell_point(int col, int row, float height, float *x, float *y)
{
    float o[3], z;
    grid_offset(col, row, o);
    o[1] += height;   /* 0 = the base of the 5-unit box, 2.5 = its middle */
    project(grid_camera(), o, x, y, &z);
}

/* the icon's vertices, ready for the GS (color, texture coordinates, position), at a moment of its animation: shapes
 * blended, turned around the upright axis through pivot (spin), moved by offset, seen through the camera and lit by
 * icon.sys's lights. Returns how many (whole triangles only) */
static int transform(icon_t *ic, const camera_t *cam, const float *pivot, const float *offset, float spin, float t,
                     GSPRIMUVPOINT *out)
{
    GSGLOBAL *gs = ui_gs_global();
    static float w[32];
    int n, s, k, single = -1, count = ic->nv - ic->nv % 3;
    float ca = cosf(spin), sa = sinf(spin), span = cam->far - cam->near;
    shape_weights(ic, t < 0 ? 0 : t, w);
    for (s = 0; s < ic->shapes; s++)   /* only one shape counts (no animation, or a key right on it): no blending */
        if (w[s] != 0)
            single = single == -1 ? s : -2;
    for (n = 0; n < count; n++) {
        float p[3], wp[3], nx, nz, l[3], sx, sy, d, zn;
        const float *q, *nr = ic->nrm + n * 3;
        if (single >= 0) {
            q = ic->pos + ((size_t)single * ic->nv + n) * 3;
            p[0] = q[0], p[1] = q[1], p[2] = q[2];
        } else {
            p[0] = p[1] = p[2] = 0;
            for (s = 0; s < ic->shapes; s++)
                if (w[s] != 0) {
                    q = ic->pos + ((size_t)s * ic->nv + n) * 3;
                    p[0] += w[s] * q[0], p[1] += w[s] * q[1], p[2] += w[s] * q[2];
                }
        }
        /* turn around the upright axis through the pivot, then place it */
        p[0] -= pivot[0], p[1] -= pivot[1], p[2] -= pivot[2];
        wp[0] = p[0] * ca + p[2] * sa + offset[0];
        wp[1] = p[1] + offset[1];
        wp[2] = -p[0] * sa + p[2] * ca + offset[2];
        nx = nr[0] * ca + nr[2] * sa;
        nz = -nr[0] * sa + nr[2] * ca;
        /* lights: ambient + the three directional ones (Lambert), times the vertex color (0x80 = 1.0); never brighter
         * than the texture itself (side by side with the PS2 browser, letting it go up to 2x blew white icons like
         * Xenosaga's rabbit out to flat white), and a little dimmer overall, like the browser */
        l[0] = ic->ambient[0], l[1] = ic->ambient[1], l[2] = ic->ambient[2];
        for (k = 0; k < 3; k++) {
            float lam = ic->lightDir[k][0] * nx + ic->lightDir[k][1] * nr[1] + ic->lightDir[k][2] * nz;
            if (lam > 0) {
                l[0] += lam * ic->lightCol[k][0];
                l[1] += lam * ic->lightCol[k][1];
                l[2] += lam * ic->lightCol[k][2];
            }
        }
        for (k = 0; k < 3; k++) {
            float c = l[k] * ic->col[n * 4 + k];
            l[k] = (c > 128 ? 128 : c) * 0.92f;
        }
        project(cam, wp, &sx, &sy, &d);
        zn = (cam->far - d) / span;   /* nearer = bigger z (the GS keeps the nearest) */
        out[n].rgbaq = color_to_RGBAQ((u8)l[0], (u8)l[1], (u8)l[2], 0x80, 0);
        out[n].uv = vertex_to_UV(&ic->gtex, ic->uv[n * 2] * TEX, ic->uv[n * 2 + 1] * TEX);
        out[n].xyz2 = vertex_to_XYZ2(gs, sx, sy, 2 + (int)(60000.0f * (zn < 0 ? 0 : zn > 1 ? 1 : zn)));
    }
    return count;
}

/* sends the triangles to the GS in lists (one block per 900 triangles) instead of one primitive each. The Z test is
 * "greater": of two faces at the same depth the first drawn stays (cards with their front and back on the same plane,
 * like EA's, would otherwise show the mirrored back) */
static void send(icon_t *ic, const GSPRIMUVPOINT *v, int count)
{
    GSGLOBAL *gs = ui_gs_global();
    int prevAlpha = gs->PrimAlphaEnable, i;
    gsKit_TexManager_bind(gs, &ic->gtex);
    gs->PrimAlphaEnable = GS_SETTING_OFF;
    gsKit_set_test(gs, GS_ZTEST_ON);
    gs->Test->ZTST = 3;   /* GREATER */
    gsKit_set_test(gs, 0);
    for (i = 0; i < count; i += 2700) {
        gsKit_prim_list_triangle_goraud_texture_uv_3d(gs, &ic->gtex, count - i < 2700 ? count - i : 2700, v + i);
        gsKit_queue_exec(gs);
    }
    gs->Test->ZTST = 2;   /* back to gsKit's GEQUAL */
    gsKit_set_test(gs, GS_ZTEST_OFF);
    gs->PrimAlphaEnable = prevAlpha;
}

/* draws through cam: still (worked out once and kept: key tells where) or animated at t */
static void draw_with(icon_t *ic, const camera_t *cam, const float *pivot, const float *offset, float spin, float t,
                      const float *key)
{
    static GSPRIMUVPOINT *frame;
    static int frameCap;
    int count;
    if (t < 0) {
        /* still: the same every frame, so it's worked out once (until it moves) and only sent again. That's what keeps
         * a screen of 20 icons smooth: only the selected one is computed frame by frame */
        if (!ic->still || ic->stillAt[0] != key[0] || ic->stillAt[1] != key[1] || ic->stillAt[2] != key[2]) {
            if (!ic->still && !(ic->still = memalign(64, sizeof(GSPRIMUVPOINT) * ic->nv)))
                return;
            ic->stillCount = transform(ic, cam, pivot, offset, 0, -1, ic->still);
            ic->stillAt[0] = key[0], ic->stillAt[1] = key[1], ic->stillAt[2] = key[2];
        }
        send(ic, ic->still, ic->stillCount);
        return;
    }
    if (frameCap < ic->nv) {
        free(frame);
        frameCap = 0;
        if (!(frame = memalign(64, sizeof(GSPRIMUVPOINT) * ic->nv)))
            return;
        frameCap = ic->nv;
    }
    count = transform(ic, cam, pivot, offset, spin, t, frame);
    send(ic, frame, count);
}

void icon_draw_cell(icon_t *ic, int col, int row, float t)
{
    static const float axis[3] = {0, 0, 0};   /* in the grid, icons turn around their own model's axis, like the PS2's */
    float o[3], key[3] = {-1000.0f - col, (float)row, 0};
    if (!ic || !ic->ok)
        return;
    grid_offset(col, row, o);
    draw_with(ic, grid_camera(), axis, o, t < 0 ? 0 : t * 0.9f, t, key);
}

void icon_draw(icon_t *ic, float cx, float cy, float size, float t)
{
    camera_t cam;
    float origin[3] = {0, 0, 0}, key[3] = {cx, cy, size};
    if (!ic || !ic->ok)
        return;
    /* one icon by itself (the backup screen, the big card): the same angle as the browser, but framed by the icon's
     * own size, so a small one (Mister Mosquito fills a third of the 5-unit box) still fills the square */
    camera_look(&cam, origin, 30, size / (2.2f * ic->radius), cx, cy);
    /* it turns around its own middle, which goes to the camera's target */
    if (ic->swing)   /* the flat memory card swings instead of turning all the way */
        draw_with(ic, &cam, ic->center, origin, t < 0 ? 0 : 0.55f * sinf(t * 1.3f), t < 0 ? -1 : t, key);
    else
        draw_with(ic, &cam, ic->center, origin, t < 0 ? 0 : t * 0.9f, t, key);
}
