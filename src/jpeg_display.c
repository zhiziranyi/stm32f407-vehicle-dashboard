#include "jpeg_display.h"
#include <string.h>

static unsigned char jpeg_pool[5500];
static FIL *g_jpeg_file;

static unsigned int jpeg_input_func(JDEC *jdec, unsigned char *buff, unsigned int nbyte)
{
    (void)jdec;
    if (buff) {
        UINT br;
        f_read(g_jpeg_file, buff, nbyte, &br);
        return br;
    } else {
        /* bitext() calls infunc(jd, NULL, 0) to fetch one byte.
           Return 0x100 | byte on success (so 0x00 ≠ error), or 0 on EOF. */
        unsigned char byte;
        UINT br;
        f_read(g_jpeg_file, &byte, 1, &br);
        return br ? (0x100U | byte) : 0;
    }
}

static int jpeg_output_func(JDEC *jdec, void *bitmap, JRECT *rect)
{
    (void)jdec;

    int src_w = rect->right - rect->left + 1;

    /* Skip if entirely off-screen (image larger than TFT) */
    if (rect->left >= TFT_WIDTH || rect->top >= TFT_HEIGHT ||
        rect->right < 0 || rect->bottom < 0) {
        return 1;
    }

    /* Clip to screen bounds */
    int skip_l = 0, skip_t = 0;
    if (rect->left < 0)      { skip_l = -rect->left; rect->left = 0; }
    if (rect->top < 0)       { skip_t = -rect->top;  rect->top = 0; }
    if (rect->right  >= TFT_WIDTH)  rect->right  = TFT_WIDTH - 1;
    if (rect->bottom >= TFT_HEIGHT) rect->bottom = TFT_HEIGHT - 1;

    int w = rect->right - rect->left + 1;
    int h = rect->bottom - rect->top + 1;

    unsigned char *src = (unsigned char *)bitmap + (skip_t * src_w + skip_l) * 3;
    uint16_t line_buf[TFT_WIDTH];
    int x, y;

    for (y = 0; y < h; y++) {
        uint16_t *dst = line_buf;
        unsigned char *px = src;
        for (x = 0; x < w; x++) {
            *dst++ = RGB565(px[0], px[1], px[2]);
            px += 3;
        }
        TFT_SetAddrWindow(rect->left, rect->top + y, rect->right, rect->top + y);
        TFT_PushColors(line_buf, w);
        src += src_w * 3;
    }
    return 1;
}

int JPEG_Display(FIL *file)
{
    JDEC jdec;
    JRESULT rc;

    g_jpeg_file = file;
    f_lseek(file, 0);

    memset(&jdec, 0, sizeof(JDEC));
    rc = jd_prepare(&jdec, jpeg_input_func, jpeg_pool, sizeof(jpeg_pool), NULL);
    if (rc != JDR_OK) return (int)rc;

    rc = jd_decomp(&jdec, jpeg_output_func, 0);
    if (rc != JDR_OK) return (int)rc;

    return 0;
}
