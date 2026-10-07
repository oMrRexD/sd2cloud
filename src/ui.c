/*
 * SD2Cloud -- the screen, with gsKit.
 *
 * A thread of its own draws the current scene every frame (60 times a second), so the screen keeps moving while the
 * main thread waits on the network, on the microSD or on the controller. The main thread only says which scene is on
 * screen (ui_scene) and changes what it shows between ui_lock and ui_unlock. The drawing thread sleeps on a semaphore
 * the vblank interrupt signals (gsKit's own wait spins on the GS register and would take the CPU away from the main
 * thread) and its priority is one above the main thread's, so it gets in right at each frame and then sleeps again.
 *
 * Text comes from font.c (a TrueType font through FreeType); the images are PNGs embedded in the program (assets/),
 * decoded by image.c; gsKit's texture manager sends every texture to the GS when it's used.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#ifdef DEBUG_BUILD
#include <screenshot.h>
#endif
#include "font.h"
#include "ui.h"
#include "qrcodegen.h"
#include "image.h"

void log_msg(const char *fmt, ...);
u64 now_ms(void);

static GSGLOBAL *gs;
static int alpha = 0x80;

/* ------------------------------------------------------------ assets embedded by the build (bin2c) */

#define ASSET(name) extern unsigned char name[]; extern unsigned int size_##name;
ASSET(asset_space_png)
ASSET(asset_glow_png)
ASSET(asset_buttons_png)
ASSET(asset_card_png)
ASSET(asset_minicard_png)
ASSET(asset_font_ttf)
#undef ASSET

static GSTEXTURE images[IMG_COUNT];

/* fonts: size in pixels and how much bolder, in 1/64 pixel (the font is a round one, a little thin for a TV) */
static const struct {
    int px, bold;
} fontSpec[FONT_COUNT] = {
    [FONT_TEXT] = {18, 40}, [FONT_SMALL] = {15, 24}, [FONT_TITLE] = {26, 52}, [FONT_HEADER] = {22, 40},
    [FONT_BROWSER] = {23, 56}, [FONT_HUGE] = {92, 120},
};
static int fontId[FONT_COUNT];

/* ------------------------------------------------------------ colors */

static u64 gs_color(u32 c, int a)
{
    a = a * alpha / 0x80;
    return GS_SETREG_RGBAQ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, a, 0);
}

/* with a texture, the vertex color multiplies it: 0x80 = 1.0 */
static u64 tex_color(u32 c, int a)
{
    a = a * alpha / 0x80;
    return GS_SETREG_RGBAQ(((c >> 16) & 0xFF) >> 1, ((c >> 8) & 0xFF) >> 1, (c & 0xFF) >> 1, a, 0);
}

void ui_alpha(int a) { alpha = a < 0 ? 0 : a > 0x80 ? 0x80 : a; }

void ui_additive(int on)
{
    /* normal: (Cs - Cd) * As + Cd; additive: (Cs - 0) * As + Cd */
    gsKit_set_primalpha(gs, on ? GS_SETREG_ALPHA(0, 2, 0, 1, 0) : GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
}

void ui_flush(void) { gsKit_queue_exec(gs); }

/* ------------------------------------------------------------ the drawing thread */

extern void *_gp;
static int vsyncSema = -1, lockSema = -1, drawThread = -1, vsyncHandler = -1;
static volatile int drawStop, drawDone;
static void (*volatile sceneFn)(float t);
static volatile u64 sceneStart;
static u64 clockStart;
static unsigned char drawStack[128 * 1024] __attribute__((aligned(16)));   /* FreeType renders new glyphs here */
#ifdef DEBUG_BUILD
static const char *volatile capturePath;
static volatile int captureResult;
#endif

static int on_vblank(int cause)
{
    (void)cause;
    iSignalSema(vsyncSema);
    ExitHandler();
    return 0;
}

/* The anti-flicker filter of the PS2's own menus. On an interlaced TV each field shows every other line, so a detail one
 * line thick (the strokes of the letters, the glows) is on screen in one field and gone in the next: it flickers and
 * looks jagged. Both read circuits show the same frame, the first one a line lower, mixed half and half: every line on
 * the TV is the average of two neighbouring lines of the frame. Only in interlaced field mode (gsKit's default) */
static int antiFlicker;

static void show_buffer(int buffer)
{
    u32 fbp = gs->ScreenBuffer[buffer] / 8192;
    GS_SET_DISPFB2(fbp, gs->Width / 64, gs->PSM, 0, 0);
    if (antiFlicker)
        GS_SET_DISPFB1(fbp, gs->Width / 64, gs->PSM, 0, 1);
}

static void anti_flicker_on(void)
{
    if (gs->Interlace != GS_INTERLACED || gs->Field != GS_FIELD)
        return;
    antiFlicker = 1;
    GS_SET_DISPLAY1(gs->StartX + gs->StartXOffset, gs->StartY + gs->StartYOffset, gs->MagH, gs->MagV, gs->DW - 1, gs->DH - 1);
    show_buffer(0);
    /* both circuits on; the output = circuit 1 x ALP + circuit 2 x (1 - ALP), with ALP = 0x80 (half) */
    GS_SET_PMODE(1, 1, 1, 0, 0, 0x80);
}

/* gsKit_sync_flip without its wait (the vblank was already waited for) */
static void flip_now(void)
{
    if (!gs->FirstFrame && gs->DoubleBuffering == GS_SETTING_ON) {
        show_buffer(gs->ActiveBuffer & 1);
        gs->ActiveBuffer ^= 1;
    }
    gsKit_setactive(gs);
}

#ifdef DEBUG_BUILD
/* the SDK's capture (ps2_screenshot) masks PATH3 through VIF1 on every line and only releases it by writing an
 * MSKPATH3(0) straight into the VIF1 FIFO; on PCSX2 that doesn't take and gsKit hangs on the next draw. Here the
 * MSKPATH3(0) goes by DMA on the VIF1 channel (D1), like a normal packet. */
static void release_path3(void)
{
    static u32 packet[4] __attribute__((aligned(16))) = {0x06000000, 0, 0, 0};   /* MSKPATH3(0), NOP, NOP, NOP */
    volatile u32 *chcr = (volatile u32 *)0x10009000, *madr = (volatile u32 *)0x10009010, *qwc = (volatile u32 *)0x10009020;
    FlushCache(0);
    while (*chcr & 0x100)
        ;
    *qwc = 1;
    *madr = (u32)packet;
    *chcr = 0x101;   /* normal mode, from memory to VIF1, start */
    asm volatile("sync.l");
    while (*chcr & 0x100)
        ;
}
#endif

static void draw_loop(void *arg)
{
    (void)arg;
    while (!drawStop) {
        void (*fn)(float);
        u64 t0, spent;
        WaitSema(lockSema);
        t0 = now_ms();
        alpha = 0x80;
        ui_additive(0);
        gsKit_clear(gs, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
        fn = sceneFn;
        if (fn)
            fn((now_ms() - sceneStart) / 1000.0f);
        gsKit_queue_exec(gs);
#ifdef DEBUG_BUILD
        if (capturePath && now_ms() - sceneStart > 700) {
            /* the frame that was just drawn is in the draw buffer until the flip; read as CT32 (CT24 is stored in
             * 32-bit words): the SDK's CT24 conversion reads the bytes out of order */
            captureResult = ps2_screenshot_file(capturePath, gs->ScreenBuffer[gs->ActiveBuffer & 1] / 256, gs->Width, gs->Height, 0);
            release_path3();
            capturePath = NULL;
        }
#endif
        spent = now_ms() - t0;
        SignalSema(lockSema);
        /* this thread is above the main one: if it drew frame after frame, the main thread would never run. So it
         * always sleeps at least a third of what it drew (and 3 ms): the screen takes at most 3/4 of the CPU, and a
         * heavy screen gets fewer frames per second instead of stopping the program. A vblank that went by while
         * drawing doesn't count */
        while (PollSema(vsyncSema) >= 0)
            ;
        do
            WaitSema(vsyncSema);
        while (now_ms() - t0 < spent + (spent / 3 > 3 ? spent / 3 : 3));
#ifdef DEBUG_BUILD
        {   /* how long the frames take, every 5 s (debug builds) */
            static u64 sum, last;
            static unsigned int n, worst;
            sum += spent, n++;
            if (spent > worst)
                worst = spent;
            if (now_ms() - last > 5000) {
                log_msg("frames: %u ms on average, %u at worst (%u frames)", (unsigned)(sum / n), worst, n);
                sum = n = worst = 0;
                last = now_ms();
            }
        }
#endif
        flip_now();
        gsKit_TexManager_nextFrame(gs);
    }
    drawDone = 1;
    SleepThread();
}

void ui_lock(void)
{
    if (lockSema >= 0)
        WaitSema(lockSema);
}

void ui_unlock(void)
{
    if (lockSema >= 0)
        SignalSema(lockSema);
}

void ui_scene(void (*draw)(float t))
{
    ui_lock();
    if (sceneFn != draw)
        sceneStart = now_ms();
    sceneFn = draw;
    ui_unlock();
}

float ui_clock(void) { return (now_ms() - clockStart) / 1000.0f; }

#ifdef DEBUG_BUILD
int ui_capture(const char *path)
{
    if (drawThread < 0)
        return -1;
    capturePath = path;
    while (capturePath)
        usleep(10000);
    return captureResult;
}
#endif

/* ------------------------------------------------------------ start and end */

static int load_images(void)
{
    static const struct {
        unsigned char *d;
        unsigned int *n;
    } src[IMG_COUNT] = {
        [IMG_SPACE] = {asset_space_png, &size_asset_space_png},       [IMG_GLOW] = {asset_glow_png, &size_asset_glow_png},
        [IMG_BUTTONS] = {asset_buttons_png, &size_asset_buttons_png}, [IMG_CARD] = {asset_card_png, &size_asset_card_png},
        [IMG_MINICARD] = {asset_minicard_png, &size_asset_minicard_png},
    };
    int i;
    for (i = 0; i < IMG_COUNT; i++)
        if (image_load_png(&images[i], src[i].d, *src[i].n) != 0) {
            log_msg("ui: image %d didn't load", i);
            return -1;
        }
    return 0;
}

int ui_init(void)
{
    ee_thread_t th;
    ee_thread_status_t me;
    ee_sema_t s;
    int i;

    gs = gsKit_init_global();
    if (!gs)
        return -1;
    gs->PSM = GS_PSM_CT24;
    gs->PSMZ = GS_PSMZ_16S;
    gs->ZBuffering = GS_SETTING_ON;   /* only the 3D icons test it (icon.c); everything 2D draws over anything */
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_ON;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    anti_flicker_on();
    gsKit_set_test(gs, GS_ZTEST_OFF);
    gsKit_set_clamp(gs, GS_CMODE_CLAMP);
    ui_additive(0);
    gsKit_TexManager_init(gs);   /* all the video memory after the screen buffers is the textures' */

    if (font_init() != 0)
        return -2;
    for (i = 0; i < FONT_COUNT; i++)
        if ((fontId[i] = font_load(asset_font_ttf, size_asset_font_ttf, fontSpec[i].px, fontSpec[i].bold)) < 0)
            return -3;
    if (load_images() != 0)
        return -4;

    clockStart = sceneStart = now_ms();
    memset(&s, 0, sizeof(s));
    s.init_count = 0;
    s.max_count = 1;
    if ((vsyncSema = CreateSema(&s)) < 0)
        return -5;
    s.init_count = 1;
    if ((lockSema = CreateSema(&s)) < 0)
        return -5;
    vsyncHandler = AddIntcHandler(INTC_VBLANK_S, on_vblank, 0);
    EnableIntc(INTC_VBLANK_S);
    ReferThreadStatus(GetThreadId(), &me);
    memset(&th, 0, sizeof(th));
    th.func = draw_loop;
    th.stack = drawStack;
    th.stack_size = sizeof(drawStack);
    th.gp_reg = &_gp;
    th.initial_priority = me.current_priority > 0 ? me.current_priority - 1 : 0;
    drawStop = drawDone = 0;
    if ((drawThread = CreateThread(&th)) < 0)
        return -6;
    log_msg("drawing thread: priority %d (main thread %d); screen %dx%d, %s, anti-flicker %s", th.initial_priority,
            me.current_priority, gs->Width, gs->Height, gs->Interlace == GS_INTERLACED ? "interlaced" : "progressive",
            antiFlicker ? "on" : "off");
#ifdef DEBUG_BUILD
    log_msg("semaphores: the frame's %d, the screen's lock %d", vsyncSema, lockSema);
#endif
    StartThread(drawThread, NULL);
    return 0;
}

void ui_end(void)
{
    if (drawThread >= 0) {
        drawStop = 1;
        while (!drawDone)
            usleep(2000);
        TerminateThread(drawThread);
        DeleteThread(drawThread);
        drawThread = -1;
    }
    if (vsyncHandler >= 0) {
        DisableIntc(INTC_VBLANK_S);
        RemoveIntcHandler(INTC_VBLANK_S, vsyncHandler);
        vsyncHandler = -1;
    }
    if (gs) {
        gsKit_deinit_global(gs);
        gs = NULL;
    }
}

#ifdef DEBUG_BUILD
void ui_abort(void)
{
    if (drawThread >= 0 && drawThread != GetThreadId())
        TerminateThread(drawThread);
    drawThread = -1;
    if (vsyncHandler >= 0) {
        DisableIntc(INTC_VBLANK_S);
        RemoveIntcHandler(INTC_VBLANK_S, vsyncHandler);
        vsyncHandler = -1;
    }
}
#endif

GSGLOBAL *ui_gs_global(void) { return gs; }
int ui_width(void) { return gs->Width; }
int ui_height(void) { return gs->Height; }

/* ------------------------------------------------------------ text */

int ui_text(int font, float x, float y, u32 color, const char *utf8)
{
    int w = font_draw(fontId[font], x, y, tex_color(color, 0x80), utf8);
    gsKit_queue_exec(gs);   /* each text goes to the GS by itself: one big queue made gsKit hang */
    return w;
}

static int text_a(int font, float x, float y, u32 color, int a, const char *utf8)
{
    int w = font_draw(fontId[font], x, y, tex_color(color, a), utf8);
    gsKit_queue_exec(gs);
    return w;
}

int ui_measure(int font, const char *utf8) { return font_width(fontId[font], utf8); }
int ui_rasterize(int font, const char *utf8, void (*plot)(int x, int y, int coverage, void *u), void *u)
{
    return font_rasterize(fontId[font], utf8, plot, u);
}
int ui_line_height(int font) { return font_line(fontId[font]); }

void ui_text_center(int font, float cx, float y, u32 color, const char *utf8)
{
    ui_text(font, (int)(cx - ui_measure(font, utf8) / 2), y, color, utf8);
}

void ui_text_right(int font, float right, float y, u32 color, const char *utf8)
{
    ui_text(font, (int)(right - ui_measure(font, utf8)), y, color, utf8);
}

int ui_text_glow(int font, float x, float y, u32 color, u32 glow, const char *utf8)
{
    /* the letters again, around themselves and faint, added up: a halo that follows their shape */
    static const signed char ring[][2] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}, {-2, -2}, {2, 2}, {-2, 2}, {2, -2},
                                          {-3, 0}, {3, 0}, {0, -3}, {0, 3}};
    int i;
    ui_additive(1);
    for (i = 0; i < (int)(sizeof(ring) / sizeof(ring[0])); i++)
        font_draw(fontId[font], x + ring[i][0], y + ring[i][1], tex_color(glow, i < 8 ? 0x22 : 0x14), utf8);
    gsKit_queue_exec(gs);
    ui_additive(0);
    return ui_text(font, x, y, color, utf8);
}

int ui_text_shadow(int font, float x, float y, u32 color, const char *utf8)
{
    text_a(font, x + 1, y + 2, 0x000000, 0x50, utf8);
    return ui_text(font, x, y, color, utf8);
}

/* lays a text out in lines of at most width; draw = draw them (else only count). Returns the number of lines */
static int lay_out(int font, float x, float y, int width, u32 color, const char *text, int draw)
{
    char line[256], word[128];
    const char *p = text;
    int lines = 0, lh = ui_line_height(font) + 2;
    line[0] = 0;
    while (*p) {
        size_t n = strcspn(p, " \n");
        char test[400];
        snprintf(word, sizeof(word), "%.*s", (int)n, p);
        snprintf(test, sizeof(test), "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && ui_measure(font, test) > width) {
            if (draw)
                ui_text(font, x, y + lines * lh, color, line);
            lines++;
            snprintf(line, sizeof(line), "%s", word);
        } else
            snprintf(line, sizeof(line), "%s", test);
        p += n;
        if (*p == '\n') {   /* a line break in the text itself */
            if (draw)
                ui_text(font, x, y + lines * lh, color, line);
            lines++;
            line[0] = 0;
            p++;
            continue;
        }
        while (*p == ' ')
            p++;
    }
    if (line[0]) {
        if (draw)
            ui_text(font, x, y + lines * lh, color, line);
        lines++;
    }
    return lines;
}

int ui_paragraph(int font, float x, float y, int width, u32 color, const char *utf8)
{
    return (int)y + lay_out(font, x, y, width, color, utf8, 1) * (ui_line_height(font) + 2);
}

int ui_paragraph_height(int font, int width, const char *utf8)
{
    return lay_out(font, 0, 0, width, 0, utf8, 0) * (ui_line_height(font) + 2);
}

int ui_text_fit(int font, float x, float y, int width, u32 color, const char *utf8)
{
    char c[256];
    size_t n;
    if (ui_measure(font, utf8) <= width)
        return ui_text(font, x, y, color, utf8);
    snprintf(c, sizeof(c), "%s", utf8);
    for (n = strlen(c); n > 0; n--) {
        if ((c[n] & 0xC0) == 0x80)   /* don't cut a UTF-8 character in half */
            continue;
        snprintf(c + n, sizeof(c) - n, "\xE2\x80\xA6");   /* ... */
        if (ui_measure(font, c) <= width)
            break;
    }
    return ui_text(font, x, y, color, c);
}

/* ------------------------------------------------------------ shapes */

void ui_rect(float x, float y, float w, float h, u32 color, int a)
{
    gsKit_prim_sprite(gs, x, y, x + w, y + h, 1, gs_color(color, a));
}

void ui_gradient(float x, float y, float w, float h, u32 top, int alphaTop, u32 bottom, int alphaBottom)
{
    u64 a = gs_color(top, alphaTop), b = gs_color(bottom, alphaBottom);
    gsKit_prim_quad_gouraud(gs, x, y, x + w, y, x, y + h, x + w, y + h, 1, a, a, b, b);
}

void ui_line(float x1, float y1, float x2, float y2, u32 color, int a)
{
    gsKit_prim_line(gs, x1, y1, x2, y2, 1, gs_color(color, a));
}

void ui_triangle(float x1, float y1, float x2, float y2, float x3, float y3, u32 color, int a)
{
    gsKit_prim_triangle(gs, x1, y1, x2, y2, x3, y3, 1, gs_color(color, a));
    /* gsKit joins a primitive to the one before it when they are of the same kind, and of two flat triangles joined
     * that way only the first shows: an empty sprite after each keeps them apart */
    gsKit_prim_sprite(gs, x1, y1, x1, y1, 1, gs_color(0, 0));
}

void ui_quad(const float *xy, const u32 *colors, const int *alphas)
{
    gsKit_prim_quad_gouraud(gs, xy[0], xy[1], xy[2], xy[3], xy[4], xy[5], xy[6], xy[7], 1, gs_color(colors[0], alphas[0]),
                            gs_color(colors[1], alphas[1]), gs_color(colors[2], alphas[2]), gs_color(colors[3], alphas[3]));
}

/* ------------------------------------------------------------ images */

void ui_image_part(int img, float x, float y, float w, float h, float u0, float v0, float u1, float v1, u32 tint, int a)
{
    GSTEXTURE *t = &images[img];
    gsKit_TexManager_bind(gs, t);
    gsKit_prim_sprite_texture(gs, t, x, y, u0, v0, x + w, y + h, u1, v1, 1, tex_color(tint, a));
    gsKit_queue_exec(gs);
}

void ui_image(int img, float x, float y, float w, float h, u32 tint, int a)
{
    ui_image_part(img, x, y, w, h, 0, 0, images[img].Width, images[img].Height, tint, a);
}

void ui_light(float cx, float cy, float rx, float ry, u32 color, int a)
{
    ui_additive(1);
    ui_image(IMG_GLOW, cx - rx, cy - ry, 2 * rx, 2 * ry, color, a);
    ui_additive(0);
}

void ui_button(int button, float x, float y, float size)
{
    ui_image_part(IMG_BUTTONS, x, y, size, size, button * 32, 0, button * 32 + 32, 32, 0xFFFFFF, 0x80);
}

/* ------------------------------------------------------------ QR code */

int ui_qr(const char *text, float x, float y, int module)
{
    static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)], tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    static char last[256];
    static int ok;
    int n, side, i, j;
    if (strcmp(text, last) != 0) {   /* encoded once, drawn every frame */
        snprintf(last, sizeof(last), "%s", text);
        ok = qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO, true);
    }
    if (!ok)
        return 0;
    n = qrcodegen_getSize(qr);
    side = (n + 8) * module;   /* 4 modules of margin on each side */
    ui_rect(x, y, side, side, 0xFFFFFF, 0x80);
    for (j = 0; j < n; j++) {
        for (i = 0; i < n; i++)
            if (qrcodegen_getModule(qr, i, j))
                ui_rect(x + (i + 4) * module, y + (j + 4) * module, module, module, 0x000000, 0x80);
        gsKit_queue_exec(gs);
    }
    return side;
}
