/**
 * lvgl_ui.h — cheji407 车载仪表 GUI
 *
 * 暗色汽车主题 | 240x240 TFT | 摇杆导航
 * 4个页面: 仪表盘 / 电机状态 / 车辆控制 / 车身状态
 */

#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl.h"

/* ================================================================== */
/* API                                                                */
/* ================================================================== */

/** 初始化 GUI (创建所有页面) */
void lvgl_ui_init(void);

/** CAN 数据更新回调 — 收到 CAN 报文后刷新界面 */
void lvgl_ui_on_can_update(void);

/** 处理摇杆按键 (由主循环调用) */
void lvgl_ui_handle_key(uint32_t key);

/** 页面切换动画 — 修改当前页 */
void lvgl_ui_set_page(uint8_t page_id);

/** 获取当前页面 */
uint8_t lvgl_ui_get_page(void);
int lvgl_ui_is_viewing(void);  /* 返回1=正在看图(跳过LVGL刷新) */

#endif /* LVGL_UI_H */
