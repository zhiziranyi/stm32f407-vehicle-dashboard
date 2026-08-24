#include "sd_card.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"

static FATFS g_fs;
static int g_mounted = 0;

FATFS *SD_GetFS(void) { return &g_fs; }

int SD_Init(void)
{
    char SDPath[4] = {0};

    if (FATFS_LinkDriver(&SD_Driver, SDPath) != 0)
        return -1;

    if (f_mount(&g_fs, "0:", 1) != FR_OK)
        return -2;

    g_mounted = 1;
    return 0;
}

int SD_FileOpen(FIL *file, const char *path)
{
    if (!g_mounted) return -1;
    return (f_open(file, path, FA_READ) == FR_OK) ? 0 : -1;
}

unsigned int SD_FileRead(FIL *file, void *buf, unsigned int size)
{
    UINT br = 0;
    f_read(file, buf, size, &br);
    return br;
}

int SD_FileClose(FIL *file)
{
    return (f_close(file) == FR_OK) ? 0 : -1;
}

unsigned int SD_FileSize(FIL *file)
{
    return (unsigned int)f_size(file);
}
