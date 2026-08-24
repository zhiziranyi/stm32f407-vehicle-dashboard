#include "lv_port_fs.h"
#include "ff.h"
#include "lv_conf.h"
#include <string.h>

static char g_drv_letter = LV_FS_FATFS_LETTER;

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    FIL *f = lv_mem_alloc(sizeof(FIL));
    if (!f) return NULL;

    BYTE flags = FA_READ;
    if (mode & LV_FS_MODE_WR) flags |= FA_WRITE;

    char full[128];
    if (path[0] == '/') path++;
    lv_snprintf(full, sizeof(full), "%c:%s", g_drv_letter, path);
    if (f_open(f, full, flags) != FR_OK) {
        lv_mem_free(f);
        return NULL;
    }
    if (mode & LV_FS_MODE_WR) f_lseek(f, f_size(f));
    return f;
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    FIL *f = (FIL *)file_p;
    f_close(f);
    lv_mem_free(f);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf,
                           uint32_t btr, uint32_t *br)
{
    (void)drv;
    UINT brr;
    FRESULT res = f_read((FIL *)file_p, buf, btr, &brr);
    *br = brr;
    return (res == FR_OK) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf,
                            uint32_t btw, uint32_t *bw)
{
    (void)drv;
    UINT bww;
    FRESULT res = f_write((FIL *)file_p, buf, btw, &bww);
    *bw = bww;
    return (res == FR_OK) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos,
                           lv_fs_whence_t whence)
{
    (void)drv;
    FIL *f = (FIL *)file_p;
    if (whence == LV_FS_SEEK_SET)
        f_lseek(f, pos);
    else if (whence == LV_FS_SEEK_CUR)
        f_lseek(f, f_tell(f) + pos);
    else if (whence == LV_FS_SEEK_END)
        f_lseek(f, f_size(f) + pos);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    *pos_p = f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

static void *fs_dir_open(lv_fs_drv_t *drv, const char *path)
{
    (void)drv;
    DIR *d = lv_mem_alloc(sizeof(DIR));
    if (!d) return NULL;

    char full[128];
    if (path[0] == '/') path++;
    lv_snprintf(full, sizeof(full), "%c:%s", g_drv_letter, path);
    if (f_opendir(d, full) != FR_OK) {
        lv_mem_free(d);
        return NULL;
    }
    return d;
}

static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *dir_p, char *fn)
{
    (void)drv;
    FILINFO fno;
    if (f_readdir((DIR *)dir_p, &fno) != FR_OK || fno.fname[0] == 0)
        return LV_FS_RES_FS_ERR;
    strncpy(fn, fno.fname, 255);
    fn[255] = '\0';
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *dir_p)
{
    (void)drv;
    f_closedir((DIR *)dir_p);
    lv_mem_free(dir_p);
    return LV_FS_RES_OK;
}

void lv_port_fs_init(void)
{
    lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter      = g_drv_letter;
    fs_drv.open_cb     = fs_open;
    fs_drv.close_cb    = fs_close;
    fs_drv.read_cb     = fs_read;
    fs_drv.write_cb    = fs_write;
    fs_drv.seek_cb     = fs_seek;
    fs_drv.tell_cb     = fs_tell;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;
    fs_drv.cache_size   = LV_FS_FATFS_CACHE_SIZE;
    lv_fs_drv_register(&fs_drv);
}
