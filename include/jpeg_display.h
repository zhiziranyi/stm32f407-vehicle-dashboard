#ifndef JPEG_DISPLAY_H
#define JPEG_DISPLAY_H

#include "stm32f4xx_hal.h"
#include "tjpgd.h"
#include "ff.h"
#include "tft_display.h"

int JPEG_Display(FIL *file);

#endif /* JPEG_DISPLAY_H */
