#ifndef SD_CARD_H
#define SD_CARD_H

#include "stm32f4xx_hal.h"
#include "ff.h"

FATFS *SD_GetFS(void);
int SD_Init(void);
int SD_FileOpen(FIL *file, const char *path);
unsigned int SD_FileRead(FIL *file, void *buf, unsigned int size);
int SD_FileClose(FIL *file);
unsigned int SD_FileSize(FIL *file);

#endif /* SD_CARD_H */
