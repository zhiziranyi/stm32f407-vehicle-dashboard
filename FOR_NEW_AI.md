# 项目接手指南

## 项目概述

车载仪表系统，双MCU架构：
- **cheji407** (STM32F407ZGT6, STM32Cube): TFT仪表 + LVGL + CAN + SD卡 + 摇杆
- **cheji103** (STM32F103C8T6, STM32Cube): BMS模拟 + SVPWM电机控制 → CAN → F407

## 当前状态

✅ 仪表盘4页（DASH/MOTOR/CTRL/BODY）+ 摇杆导航：全部正常
✅ CAN通信：F407 ←→ F103 500kbps，正常收发
✅ 电机控制：开环SVPWM，300RPM，驾驶模式切换
✅ SD卡：FatFs工作，文件系统可读写

🔄 MEDIA页面（第5页）：正在开发中，代码已添加但编译报错

## 编译命令

```bash
PIO="c:/Users/13957/.platformio/penv/Scripts/pio"
cd "c:/Users/13957/Documents/PlatformIO/Projects/cheji407"
"$PIO" run
```

## MEDIA页面开发任务

### 目标
新增第5个Tab页"MEDIA"，照搬 `C:\Users\13957\Documents\PlatformIO\Projects\youxiji` 项目的文件浏览器。youxiji已经有完整的.raw图片查看、.txt文本阅读、.vid视频播放功能。

### 照搬youxiji的代码

youxiji项目文件位置：
- `C:\Users\13957\Documents\PlatformIO\Projects\youxiji\src\lvgl_ui.c` — 完整UI代码

关键代码段在youxiji的lvgl_ui.c中：
- 文件浏览器（media_build_list, media_show_tab, media_nav, view_raw, view_txt, vid_frame等函数）
- 查看状态管理：V_NONE / V_RAW / V_TXT / V_VID
- 子页面切换：IMG(.raw) / TXT(.txt) / VID(.vid)

youxiji的lvgl_ui.c只做文件浏览器（没有仪表盘），所以变量和函数可以直接用。

### 实现步骤

**步骤1**：在 `cheji407/src/lvgl_ui.c` 中完整照搬youxiji的媒体代码。需要添加：

```c
// 变量声明（添加到静态变量区域）
#define MEDIA_MAX 30
static lv_obj_t *g_media_status;
static lv_group_t *g_media_group;
static lv_obj_t *g_btns_img[MEDIA_MAX], *g_btns_txt[MEDIA_MAX], *g_btns_vid[MEDIA_MAX];
static char g_names_img[MEDIA_MAX][48], g_names_txt[MEDIA_MAX][48], g_names_vid[MEDIA_MAX][48];
static int g_cnt_img, g_cnt_txt, g_cnt_vid;
static int g_focus_img, g_focus_txt, g_focus_vid;
static int g_media_tab = 0;
#define V_NONE 0
#define V_RAW  1
#define V_TXT  2
#define V_VID  3
static int g_view = V_NONE;
static uint8_t g_txt_buf[4100]; static unsigned int g_txt_len;
static int g_txt_lines[256], g_txt_lc, g_txt_scroll;
static FIL g_vid_f; static int g_vid_frames, g_vid_fidx, g_vid_delay;
static uint32_t g_vid_next;
static lv_style_t g_style_focus;

// 函数：从youxiji照搬
str_end(), btn_focus_set(), media_build_list(), media_show_tab(), media_nav()
view_raw(), view_txt(), txt_lines(), txt_render()
view_vid(), vid_frame()
page_media()  // 初始化函数
```

**步骤2**：在 `lvgl_ui_init()` 中添加第5个Tab：
```c
lv_obj_t *t4 = lv_tabview_add_tab(g_tabview, "MEDIA");
lv_obj_set_style_bg_color(t4, C_BG, 0);
page_media(t4);
```

**步骤3**：在 `lvgl_ui_handle_key()` 中添加MEDIA页处理（pg==4时），照搬youxiji的key handling逻辑：
```c
if (pg == 4) {
    // 文本查看时
    if (g_view == V_TXT) { ... }
    // 图片/视频查看时
    if (g_view == V_RAW) { ... }
    if (g_view == V_VID) { ... }
    // 浏览器模式：LEFT/RIGHT选文件，UP/DOWN切子页面，ENTER打开
    ...
}
```
照搬youxiji的 `lvgl_ui_handle_key` 函数中处理VIEW_TEXT/VIEW_RAW/VIEW_VID的逻辑。

**步骤4**：添加 `lvgl_ui_is_viewing()` 函数：
```c
int lvgl_ui_is_viewing(void) { return (g_view == V_RAW || g_view == V_TXT || g_view == V_VID); }
```
在 `main.c` 中已添加 `if (!lvgl_ui_is_viewing()) lv_timer_handler();`，确保看图片/文本/视频时暂停LVGL刷新。

**步骤5**：更新 `lvgl_ui_set_page` 中的页数限制从4改为5：
```c
if(id<5)
```

**步骤6**：更新所有`pg < 3`为`pg < 4`（因为现在有5个页面，MEDIA是第5页index=4）：
全局替换 `pg < 3` → `pg < 4`

### SD卡文件格式说明

youxiji的媒体文件是PC端预转换的，不是原始JPG/MP4：

| F407识别后缀 | 实际格式 | PC端转换方式 |
|---|---|---|
| `.raw` | 240×240 RGB565原始像素 | `convert_jpeg.py`(在SD卡根目录) |
| `.txt` | UTF-8文本 | 直接放 |
| `.vid` | 8字节头(帧数+fps) + 帧数据 | `convert_video.py` |

SD卡（E:\）已有文件：`nvpu.raw`, `qunzi.raw`, `xiao.raw`, `11.txt`, `222.txt`, `1.vid`, `2.vid`

### 注意事项

1. **不要用JPEG解码**：youxiji的图片是.raw格式（115200字节=240×240×2），直接读文件→推TFT，不需要tjpgd
2. **查看器全用直接TFT写入**：不经过LVGL渲染，这样才不会被LVGL覆盖
3. **LVGL组导航**：MEDIA页面的文件列表使用LVGL group + focus实现选择高亮
4. **F407 CAN error中断已关闭**（SCE_IRQn），加入新代码不要重新打开
5. **颜色宏**：当前代码使用浅色主题（C_BG等），新增代码保持用这些宏
6. **不要删除现有的DASH/MOTOR/CTRL/BODY页面代码**

### 测试方法

1. 烧录F407
2. 摇杆LEFT切到MEDIA页
3. UP/DOWN切IMG/TXT/VID子页面
4. LEFT/RIGHT选文件
5. ENTER打开

### 需要的include

在lvgl_ui.c中已添加：
```c
#include "jpeg_display.h"  // 可能不需要了，但保留无妨
#include "ff.h"
#include "tft_display.h"
#include <string.h>
```
