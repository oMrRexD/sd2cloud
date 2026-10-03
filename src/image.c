/*
 * SD2Cloud -- the interface's PNG images (assets/, embedded in the program) as gsKit textures.
 *
 * Decoded with libpng's simplified API, straight from memory. A palette image (all of the interface's are) becomes an
 * 8-bit texture with a 256-color table, which takes a quarter of the video memory of a 32-bit one; anything else
 * becomes a 32-bit texture. The texture stays in RAM: gsKit's texture manager sends it to the GS when it is drawn.
 */
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <png.h>
#include <gsKit.h>
#include "image.h"

/* PNG alpha goes from 0 to 255, the GS's from 0 (clear) to 0x80 (opaque) */
static u32 gs_alpha(u32 a) { return (a + 1) >> 1; }

/* the GS reads a 256-color table (CSM1) in blocks of 32 with entries 8-15 and 16-23 swapped: where entry i goes */
static unsigned int clut_slot(unsigned int i) { return (i & ~0x18u) | ((i & 0x08u) << 1) | ((i & 0x10u) >> 1); }

static int read_indexed(png_image *img, GSTEXTURE *tex)
{
    static png_byte colors[256 * 4];   /* RGBA, as libpng gives the color map */
    u32 *clut;
    unsigned int i;
    img->format = PNG_FORMAT_RGBA_COLORMAP;
    tex->PSM = GS_PSM_T8;
    tex->ClutPSM = GS_PSM_CT32;
    tex->ClutStorageMode = GS_CLUT_STORAGE_CSM1;
    tex->Mem = memalign(128, gsKit_texture_size_ee(img->width, img->height, GS_PSM_T8));
    tex->Clut = memalign(128, 256 * sizeof(u32));
    if (!tex->Mem || !tex->Clut)
        return -1;
    memset(colors, 0, sizeof(colors));
    if (!png_image_finish_read(img, NULL, tex->Mem, 0, colors))   /* one index byte per pixel */
        return -1;
    clut = tex->Clut;
    for (i = 0; i < 256; i++) {
        const png_byte *c = &colors[i * 4];
        clut[clut_slot(i)] = i < img->colormap_entries ? c[0] | (c[1] << 8) | (c[2] << 16) | (gs_alpha(c[3]) << 24) : 0;
    }
    return 0;
}

static int read_rgba(png_image *img, GSTEXTURE *tex)
{
    u8 *p, *end;
    img->format = PNG_FORMAT_RGBA;
    tex->PSM = GS_PSM_CT32;
    tex->Mem = memalign(128, gsKit_texture_size_ee(img->width, img->height, GS_PSM_CT32));
    if (!tex->Mem || !png_image_finish_read(img, NULL, tex->Mem, 0, NULL))
        return -1;
    for (p = (u8 *)tex->Mem, end = p + img->width * img->height * 4; p < end; p += 4)
        p[3] = gs_alpha(p[3]);
    return 0;
}

int image_load_png(GSTEXTURE *tex, const void *data, size_t size)
{
    png_image img;
    int r;
    memset(tex, 0, sizeof(*tex));
    memset(&img, 0, sizeof(img));
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&img, data, size))
        return -1;
    tex->Width = img.width;
    tex->Height = img.height;
    tex->Filter = GS_FILTER_LINEAR;   /* the background is drawn at twice its size */
    r = (img.format & PNG_FORMAT_FLAG_COLORMAP) ? read_indexed(&img, tex) : read_rgba(&img, tex);
    png_image_free(&img);
    if (r != 0)
        image_free(tex);
    return r;
}

void image_free(GSTEXTURE *tex)
{
    free(tex->Mem);
    free(tex->Clut);
    memset(tex, 0, sizeof(*tex));
}
