/* SD2Cloud -- PNG images as gsKit textures (image.c) */
#ifndef IMAGE_H
#define IMAGE_H

#include <stddef.h>
#include <gsKit.h>

/* decodes a PNG in memory into tex (Mem, and Clut for a palette image), for gsKit_TexManager_bind to send to the GS
 * when it is drawn. 0 = ok */
int image_load_png(GSTEXTURE *tex, const void *data, size_t size);
void image_free(GSTEXTURE *tex);

#endif
