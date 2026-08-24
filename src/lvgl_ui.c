/**
 * lvgl_ui.c — cheji407 车载仪表 GUI
 *
 * 浅色日间模式 | 居中对称布局 | 大数字车速 | 扁平化车规设计
 * 1.54" 240×240 TFT
 *
 * 操作:
 *   LEFT/RIGHT → 切页面
 *   UP/DOWN     → 仪表页: 切换驾驶模式 / 控制页: 移动焦点
 *   ENTER       → 仪表页: 切换驾驶模式 / 控制页: 操作焦点项
 *   ESC         → 关闭弹窗 / 退出滑块调节
 */

#include "lvgl_ui.h"
#include "can_driver.h"
#include "can_parser.h"
#include "vehicle_fsm.h"
#include "data_logger.h"
#include "jpeg_display.h"
#include "ff.h"
#include "tft_display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ================================================================== */
/* 配色 — 浅色日间车规主题                                            */
/* ================================================================== */

#define C_BG        lv_color_hex(0xE73C)  /* #E8E8E0 浅灰背景 */
#define C_PANEL     lv_color_hex(0xDEFB)  /* #DCDCD4 面板 */
#define C_DIVIDER   lv_color_hex(0xCE79)  /* #CCCCC0 分割线 */
#define C_TEXT      lv_color_hex(0x10C4)  /* #101830 深蓝黑 */
#define C_TEXT2     lv_color_hex(0x632C)  /* #606878 灰字 */
#define C_BLUE      lv_color_hex(0x0198)  /* #0058C0 蓝 */
#define C_GREEN     lv_color_hex(0x0284)  /* #008840 绿 */
#define C_RED       lv_color_hex(0xC967)  /* #CC2828 红 */
#define C_AMBER     lv_color_hex(0xD520)  /* #D08800 橙 */
#define C_WHITE     lv_color_hex(0xFFFF)  /* 白 */

#define FONT_14     &lv_font_montserrat_14
#define FONT_20     &lv_font_montserrat_20
#define FONT_28     &lv_font_montserrat_28

/* ================================================================== */
/* 静态变量                                                           */
/* ================================================================== */

static lv_obj_t *g_tabview  = NULL;
static uint8_t   g_cur_page = 0;

#define MAX_FOCUS 8
static lv_obj_t *g_focus[MAX_FOCUS];
static uint8_t   g_focus_cnt  = 0;
static int8_t    g_focus_idx  = -1;
static uint8_t   g_edit_mode  = 0;

/* P0 DASH */
static lv_obj_t *g_gear_label = NULL;    /* [D] */
static lv_obj_t *g_speed_num  = NULL;    /* 60 */
static lv_obj_t *g_mode_label = NULL;    /* COMFORT */
static lv_obj_t *g_arc_rpm    = NULL;
static lv_obj_t *g_arc_soc    = NULL;
static lv_obj_t *g_lbl_rpm    = NULL;
static lv_obj_t *g_lbl_soc    = NULL;
static lv_obj_t *g_lbl_temp   = NULL;
static lv_obj_t *g_lbl_odo    = NULL;
static lv_obj_t *g_lbl_volt   = NULL;

/* P1 MOTOR */
static lv_obj_t *g_rpm_num   = NULL;
static lv_obj_t *g_temp_lbl  = NULL;
static lv_obj_t *g_chart     = NULL;
static lv_chart_series_t *g_series = NULL;

/* P2 CTRL */
static lv_obj_t *g_btn_eco, *g_btn_comf, *g_btn_sport;
static lv_obj_t *g_sw_ac, *g_lbl_ac;
static lv_obj_t *g_slider_regen, *g_lbl_regen;

/* P3 BODY */
static lv_obj_t *g_v_light, *g_v_lock, *g_v_win;

/* Status bar */
static lv_obj_t *g_bar_bot, *g_lbl_bar;
static lv_obj_t *g_popup = NULL;

/* ================================================================== */
/* Helpers                                                            */
/* ================================================================== */

static lv_obj_t *mk_label(lv_obj_t *p, const char *t, const lv_font_t *f,
                          lv_color_t c, lv_align_t a, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_align(l, a, x, y);
    return l;
}

static lv_obj_t *mk_divider(lv_obj_t *p, lv_coord_t y)
{
    lv_obj_t *d = lv_obj_create(p);
    lv_obj_set_size(d, 220, 1);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(d, C_DIVIDER, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_radius(d, 0, 0);
    return d;
}

/* ================================================================== */
/* Focus system                                                       */
/* ================================================================== */

static void obj_focus(lv_obj_t *obj, int on)
{
    if (on) {
        lv_obj_set_style_bg_color(obj, C_BLUE, 0);
        lv_obj_set_style_text_color(obj, C_WHITE, 0);
        lv_obj_set_style_border_color(obj, C_TEXT, 0);
        lv_obj_set_style_border_width(obj, 2, 0);
    } else {
        lv_obj_set_style_bg_color(obj, C_PANEL, 0);
        lv_obj_set_style_text_color(obj, C_TEXT, 0);
        lv_obj_set_style_border_color(obj, C_DIVIDER, 0);
        lv_obj_set_style_border_width(obj, 1, 0);
    }
}

static void ctrl_focus(lv_obj_t *obj, int on)
{
    lv_obj_set_style_border_color(obj, on ? C_BLUE : C_DIVIDER, 0);
    lv_obj_set_style_border_width(obj, on ? 3 : 1, 0);
}

static void focus_off_one(lv_obj_t *obj) { if(obj) { obj_focus(obj,0); lv_obj_set_style_border_width(obj,1,0); } }
static void focus_on_one(int idx)
{
    lv_obj_t *obj = g_focus[idx];
    if (!obj) return;
    if (idx <= 2) obj_focus(obj, 1); else ctrl_focus(obj, 1);
}

static void focus_clear(void) {
    for (int i = 0; i < g_focus_cnt; i++) focus_off_one(g_focus[i]);
    g_focus_idx = -1; g_edit_mode = 0;
}
static void focus_set(int idx) {
    if (idx < 0 || idx >= g_focus_cnt) return;
    if (g_focus_idx >= 0) focus_off_one(g_focus[g_focus_idx]);
    g_focus_idx = (int8_t)idx; focus_on_one(idx);
}
static void focus_reg(lv_obj_t *obj) { if (g_focus_cnt < MAX_FOCUS) g_focus[g_focus_cnt++] = obj; }

/* DASH页焦点按钮 (前向声明) */
static lv_obj_t *g_dash_btn[2] = {NULL,NULL};

static void page_focus_init(uint8_t page)
{
    focus_clear(); g_focus_cnt = 0; g_edit_mode = 0;
    if (page == 2) {
        focus_reg(g_btn_eco); focus_reg(g_btn_comf); focus_reg(g_btn_sport);
        focus_reg(g_sw_ac); focus_reg(g_slider_regen);
        focus_set(1); /* default: COMFORT */
    }
}

/* ================================================================== */
/* P0: DASH — 浅色日间仪表盘                                          */
/* ================================================================== */

static void page_dashboard(lv_obj_t *p)
{
    /* 点火按钮 — 显示当前状态 */
    g_dash_btn[0] = lv_btn_create(p);
    lv_obj_set_size(g_dash_btn[0], 80, 26);
    lv_obj_align(g_dash_btn[0], LV_ALIGN_TOP_MID, 0, 1);
    lv_obj_set_style_radius(g_dash_btn[0], 13, 0);
    lv_obj_set_style_bg_color(g_dash_btn[0], C_BLUE, 0);
    lv_obj_t *lb = lv_label_create(g_dash_btn[0]);
    lv_label_set_text(lb, "SLEEP");
    lv_obj_set_style_text_color(lb, lv_color_white(), 0);
    lv_obj_center(lb);

    mk_divider(p, 28);

    /* ---- 中央大数字车速 ---- */
    g_speed_num = mk_label(p, "0", FONT_28, C_TEXT, LV_ALIGN_CENTER, 0, -48);

    mk_label(p, "km/h", FONT_14, C_TEXT2, LV_ALIGN_CENTER, 0, -14);

    /* ---- 左右弧形仪表 ---- */
    /* RPM 弧 (左侧) */
    g_arc_rpm = lv_arc_create(p);
    lv_obj_set_size(g_arc_rpm, 80, 80);
    lv_obj_align(g_arc_rpm, LV_ALIGN_CENTER, -50, 18);
    lv_obj_set_style_arc_width(g_arc_rpm, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_rpm, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc_rpm, C_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc_rpm, C_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_arc_rpm, LV_OPA_TRANSP, 0);
    lv_arc_set_range(g_arc_rpm, 0, 2000);
    lv_arc_set_rotation(g_arc_rpm, 135);
    lv_arc_set_bg_angles(g_arc_rpm, 0, 270);
    lv_arc_set_value(g_arc_rpm, 600);

    g_lbl_rpm = mk_label(p, "600", FONT_14, C_BLUE, LV_ALIGN_CENTER, -50, 8);
    mk_label(p, "rpm", FONT_14, C_TEXT2, LV_ALIGN_CENTER, -50, 24);

    /* BATT 弧 (右侧) */
    g_arc_soc = lv_arc_create(p);
    lv_obj_set_size(g_arc_soc, 80, 80);
    lv_obj_align(g_arc_soc, LV_ALIGN_CENTER, 50, 18);
    lv_obj_set_style_arc_width(g_arc_soc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_soc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc_soc, C_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc_soc, C_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_arc_soc, LV_OPA_TRANSP, 0);
    lv_arc_set_range(g_arc_soc, 0, 100);
    lv_arc_set_rotation(g_arc_soc, 135);
    lv_arc_set_bg_angles(g_arc_soc, 0, 270);
    lv_arc_set_value(g_arc_soc, 80);

    g_lbl_soc = mk_label(p, "80%", FONT_14, C_GREEN, LV_ALIGN_CENTER, 50, 8);
    mk_label(p, "batt", FONT_14, C_TEXT2, LV_ALIGN_CENTER, 50, 24);

    /* ---- 底部状态栏 ---- */
    mk_divider(p, 173);

    g_lbl_temp = mk_label(p, "85C",  FONT_14, C_TEXT2, LV_ALIGN_BOTTOM_LEFT, 14, -6);
    g_lbl_odo  = mk_label(p, "0km",  FONT_14, C_TEXT2, LV_ALIGN_BOTTOM_MID, 0, -6);
    g_lbl_volt = mk_label(p, "0.0V", FONT_14, C_TEXT2, LV_ALIGN_BOTTOM_RIGHT, -14, -6);
}

/* ================================================================== */
/* P1: MOTOR                                                          */
/* ================================================================== */

static void page_motor(lv_obj_t *p)
{
    mk_label(p, "MOTOR RPM", FONT_14, C_TEXT2, LV_ALIGN_TOP_MID, 0, 4);
    g_rpm_num = mk_label(p, "0", FONT_28, C_BLUE, LV_ALIGN_TOP_MID, 0, 20);
    mk_label(p, "rpm", FONT_14, C_TEXT2, LV_ALIGN_TOP_MID, 0, 52);
    g_temp_lbl = mk_label(p, "TEMP --C", FONT_14, C_AMBER, LV_ALIGN_TOP_RIGHT, -6, 6);

    /* Chart panel */
    lv_obj_t *panel = lv_obj_create(p);
    lv_obj_set_size(panel, 228, 112);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(panel, C_PANEL, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 6, 0);

    g_chart = lv_chart_create(panel);
    lv_obj_set_size(g_chart, 218, 102);
    lv_obj_center(g_chart);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_chart, 0, 0);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(g_chart, 60);
    lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 400);
    lv_chart_set_div_line_count(g_chart, 4, 2);
    g_series = lv_chart_add_series(g_chart, C_BLUE, LV_CHART_AXIS_PRIMARY_Y);
}

/* ================================================================== */
/* P2: CTRL                                                           */
/* ================================================================== */

static lv_obj_t *mk_btn(lv_obj_t *p, const char *t, lv_coord_t y)
{
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_size(b, 200, 32);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_border_color(b, C_DIVIDER, 0);
    lv_obj_t *lab = lv_label_create(b);
    lv_label_set_text(lab, t);
    lv_obj_set_style_text_font(lab, FONT_14, 0);
    lv_obj_set_style_text_color(lab, C_TEXT, 0);
    lv_obj_center(lab);
    return b;
}

static void btn_set_active(lv_obj_t *b, int act)
{
    if (act) {
        lv_obj_set_style_bg_color(b, C_BLUE, 0);
        lv_obj_set_style_text_color(b, C_WHITE, 0);
    } else {
        lv_obj_set_style_bg_color(b, C_PANEL, 0);
        lv_obj_set_style_text_color(b, C_TEXT, 0);
    }
}

static void update_mode_btns(void)
{
    DriveMode_t m = FSM_GetDriveMode();
    btn_set_active(g_btn_eco,   m == DRIVE_MODE_ECO);
    btn_set_active(g_btn_comf,  m == DRIVE_MODE_COMFORT);
    btn_set_active(g_btn_sport, m == DRIVE_MODE_SPORT);
}

static void page_control(lv_obj_t *p)
{
    mk_label(p, "DRIVE MODE", FONT_14, C_TEXT2, LV_ALIGN_TOP_MID, 0, 2);
    g_btn_eco   = mk_btn(p, "ECO",      24);
    g_btn_comf  = mk_btn(p, "COMFORT",  62);
    g_btn_sport = mk_btn(p, "SPORT",   100);
    update_mode_btns();

    /* A/C */
    mk_label(p, "A/C", FONT_14, C_TEXT2, LV_ALIGN_TOP_LEFT, 14, 146);
    g_sw_ac = lv_switch_create(p);
    lv_obj_align(g_sw_ac, LV_ALIGN_TOP_LEFT, 50, 142);
    lv_obj_set_style_bg_color(g_sw_ac, C_BLUE, LV_PART_INDICATOR);
    lv_obj_set_size(g_sw_ac, 40, 22);
    g_lbl_ac = mk_label(p, "OFF", FONT_14, C_TEXT2, LV_ALIGN_TOP_LEFT, 98, 146);

    /* Regen */
    mk_label(p, "REGEN", FONT_14, C_TEXT2, LV_ALIGN_TOP_LEFT, 14, 178);
    g_slider_regen = lv_slider_create(p);
    lv_obj_set_size(g_slider_regen, 120, 14);
    lv_obj_align(g_slider_regen, LV_ALIGN_TOP_LEFT, 70, 176);
    lv_slider_set_range(g_slider_regen, 0, 3);
    lv_slider_set_value(g_slider_regen, 1, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_slider_regen, C_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_slider_regen, C_DIVIDER, LV_PART_MAIN);
    g_lbl_regen = mk_label(p, "Lv.1", FONT_14, C_BLUE, LV_ALIGN_TOP_LEFT, 200, 176);
}

/* ================================================================== */
/* P3: BODY                                                           */
/* ================================================================== */

static lv_obj_t *mk_card(lv_obj_t *p, const char *title, const char *val, lv_coord_t y)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_size(c, 220, 40);
    lv_obj_align(c, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(c, C_PANEL, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 6, 0);
    mk_label(c, title, FONT_14, C_TEXT2, LV_ALIGN_LEFT_MID, 12, -6);
    lv_obj_t *vl = mk_label(c, val, FONT_14, C_TEXT, LV_ALIGN_LEFT_MID, 12, 8);
    return vl;
}

static void page_body(lv_obj_t *p)
{
    mk_label(p, "BODY STATUS", FONT_14, C_TEXT2, LV_ALIGN_TOP_MID, 0, 3);
    g_v_light = mk_card(p, "Light",  "OFF",    24);
    g_v_lock  = mk_card(p, "Lock",   "LOCKED", 72);
    g_v_win   = mk_card(p, "Window", "100%",  120);
}

/* ================================================================== */
/* Bottom bar + popup                                                  */
/* ================================================================== */

static void bar_init(void)
{
    g_bar_bot = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_bar_bot, 240, 18);
    lv_obj_align(g_bar_bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(g_bar_bot, C_PANEL, 0);
    lv_obj_set_style_border_width(g_bar_bot, 0, 0);
    lv_obj_set_style_radius(g_bar_bot, 0, 0);
    lv_obj_set_style_pad_all(g_bar_bot, 0, 0);
    g_lbl_bar = lv_label_create(g_bar_bot);
    lv_obj_set_style_text_font(g_lbl_bar, FONT_14, 0);
    lv_obj_set_style_text_color(g_lbl_bar, C_TEXT, 0);
    lv_obj_align(g_lbl_bar, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(g_lbl_bar, "READY");
}

static void bar_set(const char *t, lv_color_t c)
{
    lv_label_set_text(g_lbl_bar, t);
    lv_obj_set_style_text_color(g_lbl_bar, c, 0);
}

static void popup(const char *t, const char *m)
{
    if (g_popup) lv_msgbox_close(g_popup);
    g_popup = lv_msgbox_create(NULL, t, m, NULL, false);
    lv_obj_set_style_bg_color(g_popup, C_PANEL, 0);
    lv_obj_set_style_text_color(lv_msgbox_get_title(g_popup), C_RED, 0);
    lv_obj_center(g_popup);
}

/* ================================================================== */
/* P4: MEDIA — 文件浏览器 (照搬youxiji)                                */
/* ================================================================== */

#define MEDIA_MAX 30
static lv_obj_t   *g_media_status;
static lv_group_t *g_media_group;
static lv_obj_t   *g_cont_img;
static lv_obj_t   *g_cont_txt;
static lv_obj_t   *g_cont_vid;

static lv_obj_t *g_btns_img[MEDIA_MAX], *g_btns_txt[MEDIA_MAX], *g_btns_vid[MEDIA_MAX];
static char   g_names_img[MEDIA_MAX][48], g_names_txt[MEDIA_MAX][48], g_names_vid[MEDIA_MAX][48];
static int    g_cnt_img, g_cnt_txt, g_cnt_vid;
static int    g_focus_img, g_focus_txt, g_focus_vid;
static int    g_media_tab = 0;

#define V_NONE  0
#define V_RAW   1
#define V_TXT   2
#define V_VID   3
static int g_view = V_NONE;

static uint8_t  g_txt_buf[4100]; static unsigned int g_txt_len;
static int g_txt_lines[256], g_txt_lc, g_txt_scroll;
static FIL g_vid_f; static int g_vid_frames, g_vid_fidx, g_vid_delay;
static lv_style_t g_style_focus;

/* ---- helpers ---- */

static int str_end(const char *s, const char *ext)
{
    int sl = (int)strlen(s), el = (int)strlen(ext);
    if (sl < el) return 0;
    for (int i = 0; i < el; i++)
        if (s[sl - el + i] != ext[i]) return 0;
    return 1;
}

static void btn_focus_set(lv_obj_t *btn, int on)
{
    if (on) lv_obj_add_style(btn, &g_style_focus, 0);
    else    lv_obj_remove_style(btn, &g_style_focus, 0);
}

/* ---- build file list ---- */

static int media_build_list(lv_obj_t *parent, lv_obj_t **btns, char names[][48],
                             const char *ext)
{
    for (int i = 0; i < MEDIA_MAX; i++) {
        if (btns[i]) { lv_obj_del(btns[i]); btns[i] = NULL; }
    }
    DIR dir; FILINFO fno;
    int cnt = 0;
    if (f_opendir(&dir, "0:") != FR_OK) return 0;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] && cnt < MEDIA_MAX) {
        if (fno.fattrib & AM_DIR) continue;
        if (!str_end(fno.fname, ext)) continue;
        strncpy(names[cnt], fno.fname, 47); names[cnt][47] = '\0';
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 236, 34);
        lv_obj_set_style_bg_color(btn, C_PANEL, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, C_DIVIDER, 0);
        lv_obj_t *lb = lv_label_create(btn);
        lv_label_set_text_fmt(lb, "%s  (%lu KB)", fno.fname, (unsigned long)(fno.fsize / 1024));
        lv_obj_set_style_text_color(lb, C_TEXT, 0);
        lv_obj_center(lb);
        btns[cnt] = btn; cnt++;
    }
    f_closedir(&dir);
    return cnt;
}

/* ---- tab switching (0=img, 1=txt, 2=vid) ---- */

static void media_show_tab(int tab)
{
    g_media_tab = tab;
    lv_group_remove_all_objs(g_media_group);
    lv_obj_add_flag(g_cont_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_txt, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_cont_vid, LV_OBJ_FLAG_HIDDEN);

    if (tab == 0) {
        lv_obj_clear_flag(g_cont_img, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_img; i++) {
            lv_group_add_obj(g_media_group, g_btns_img[i]);
            btn_focus_set(g_btns_img[i], i == g_focus_img);
        }
        if (g_cnt_img > 0) lv_group_focus_obj(g_btns_img[g_focus_img]);
        lv_label_set_text_fmt(g_media_status, "%d images  L/R:sel  U/D:tab  ENTER:open", g_cnt_img);
    } else if (tab == 1) {
        lv_obj_clear_flag(g_cont_txt, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_txt; i++) {
            lv_group_add_obj(g_media_group, g_btns_txt[i]);
            btn_focus_set(g_btns_txt[i], i == g_focus_txt);
        }
        if (g_cnt_txt > 0) lv_group_focus_obj(g_btns_txt[g_focus_txt]);
        lv_label_set_text_fmt(g_media_status, "%d texts  L/R:sel  U/D:tab  ENTER:open", g_cnt_txt);
    } else {
        lv_obj_clear_flag(g_cont_vid, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < g_cnt_vid; i++) {
            lv_group_add_obj(g_media_group, g_btns_vid[i]);
            btn_focus_set(g_btns_vid[i], i == g_focus_vid);
        }
        if (g_cnt_vid > 0) lv_group_focus_obj(g_btns_vid[g_focus_vid]);
        lv_label_set_text_fmt(g_media_status, "%d videos  L/R:sel  U/D:tab  ENTER:open", g_cnt_vid);
    }
}

static void media_nav(int right)
{
    int cnt; int *fidx; lv_obj_t **btns;
    if (g_media_tab == 0)      { cnt = g_cnt_img; fidx = &g_focus_img; btns = g_btns_img; }
    else if (g_media_tab == 1) { cnt = g_cnt_txt; fidx = &g_focus_txt; btns = g_btns_txt; }
    else                        { cnt = g_cnt_vid; fidx = &g_focus_vid; btns = g_btns_vid; }
    if (cnt < 2) return;
    btn_focus_set(btns[*fidx], 0);
    *fidx = right ? (*fidx + 1) % cnt : (*fidx - 1 + cnt) % cnt;
    btn_focus_set(btns[*fidx], 1);
    lv_group_focus_obj(btns[*fidx]);
}

/* ---- viewers ---- */

static void view_raw(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "0:%s", fname);
    FIL f;
    if (f_open(&f, path, FA_READ) != FR_OK) { g_view = V_NONE; return; }
    TFT_FillScreen(COLOR_BLACK);
    TFT_SetAddrWindow(0, 0, 239, 239);
    uint16_t buf[240];
    for (int row = 0; row < 240; row++) {
        UINT br; f_read(&f, buf, sizeof(buf), &br); TFT_PushColors(buf, 240);
    }
    f_close(&f);
    g_view = V_RAW;
}

static void txt_lines(void)
{
    g_txt_lc = 0; g_txt_lines[0] = 0;
    unsigned int pos = 0; int cur_w = 0;
    while (pos < g_txt_len && g_txt_lc < 255) {
        uint8_t c = g_txt_buf[pos];
        uint32_t cp = 0; int char_w;
        if (c < 0x80) { cp = c; pos++; char_w = (cp == '\n' || cp == '\r') ? 0 : 8; }
        else if ((c & 0xE0) == 0xC0 && pos + 1 < g_txt_len) {
            cp = ((c & 0x1F) << 6) | (g_txt_buf[pos+1] & 0x3F); pos += 2; char_w = 16;
        } else if ((c & 0xF0) == 0xE0 && pos + 2 < g_txt_len) {
            cp = ((c & 0x0F) << 12) | ((g_txt_buf[pos+1] & 0x3F) << 6) | (g_txt_buf[pos+2] & 0x3F); pos += 3; char_w = 16;
        } else { pos++; char_w = 0; }
        if (cp == '\r') continue;
        if (cp == '\n' || cur_w + char_w > (int)TFT_WIDTH) {
            g_txt_lines[++g_txt_lc] = (cp == '\n') ? (int)pos : (int)(pos - (char_w > 0 ? (c < 0x80 ? 1 : 3) : 0));
            cur_w = 0; if (cp == '\n') continue;
        }
        cur_w += char_w;
    }
}

static void txt_render(void)
{
    TFT_FillScreen(COLOR_BLACK); TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
    int lines = (int)TFT_HEIGHT / 16;
    for (int i = 0; i < lines && (g_txt_scroll + i) < g_txt_lc; i++) {
        int off = g_txt_lines[g_txt_scroll + i];
        int end = (g_txt_scroll + i + 1 < g_txt_lc) ? g_txt_lines[g_txt_scroll + i + 1] : (int)g_txt_len;
        if (end > off + 1 && g_txt_buf[end-1] == '\n') end--;
        if (end > off + 1 && g_txt_buf[end-1] == '\r') end--;
        if (end > off) TFT_DrawUTF8(0, i * 16, g_txt_buf + off, (unsigned int)(end - off));
    }
}

static void view_txt(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "0:%s", fname);
    FIL f; g_txt_len = 0;
    if (f_open(&f, path, FA_READ) == FR_OK) {
        unsigned int sz = (unsigned int)f_size(&f);
        if (sz > 4095) sz = 4095;
        f_read(&f, g_txt_buf, sz, &g_txt_len); f_close(&f);
    }
    if (g_txt_len == 0) {
        TFT_FillScreen(COLOR_BLACK); TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
        TFT_DrawString(0, 0, "(empty)"); g_view = V_RAW; return;
    }
    txt_lines(); g_txt_scroll = 0; txt_render();
    g_view = V_TXT;
}

/* ---- video player ---- */

static void vid_frame(void)
{
    unsigned int offset = 8 + (unsigned int)g_vid_fidx * 240 * 240 * 2;
    f_lseek(&g_vid_f, offset);

    TFT_StreamStart();

#define VID_CHUNK 20400  /* 20400 pixels = 85 lines x 240px */
    static uint16_t vid_buf[VID_CHUNK];
    int remain = 240 * 240;

    while (remain > 0) {
        int n = (remain < VID_CHUNK) ? remain : VID_CHUNK;
        f_read(&g_vid_f, vid_buf, (UINT)(n * 2), NULL);
        TFT_StreamPush(vid_buf, n);
        remain -= n;
    }

    TFT_StreamEnd();

    /* Frame counter every 8 frames */
    if ((g_vid_fidx & 7) == 0) {
        TFT_SetTextColor(COLOR_WHITE, COLOR_BLACK);
        char fstr[16];
        snprintf(fstr, sizeof(fstr), " %d/%d ", g_vid_fidx + 1, g_vid_frames);
        TFT_DrawString(0, 0, fstr);
    }
}

static void view_vid(const char *fname)
{
    char path[128]; snprintf(path, sizeof(path), "0:%s", fname);
    if (f_open(&g_vid_f, path, FA_READ) != FR_OK) { g_view = V_NONE; return; }

    /* Read header: 4-byte frame count + 4-byte fps*100 */
    uint8_t hdr[8]; UINT br;
    f_read(&g_vid_f, hdr, 8, &br);
    g_vid_frames = (int)(hdr[0] | ((uint32_t)hdr[1] << 8)
                       | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24));
    uint32_t fps100 = hdr[4] | ((uint32_t)hdr[5] << 8)
                    | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (fps100 >= 100 && fps100 <= 10000)
        g_vid_delay = (int)(100000 / fps100);
    else
        g_vid_delay = 125;
    g_vid_fidx = 0;

    TFT_FillScreen(COLOR_BLACK);
    vid_frame();
    g_view = V_VID;
}

/* ---- init ---- */

static void page_media(lv_obj_t *p)
{
    lv_obj_set_style_bg_color(p, C_PANEL, 0);

    g_media_status = lv_label_create(p);
    lv_obj_set_style_text_color(g_media_status, C_TEXT2, 0);
    lv_obj_set_style_text_font(g_media_status, FONT_14, 0);
    lv_obj_align(g_media_status, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* Focus style */
    lv_style_init(&g_style_focus);
    lv_style_set_bg_color(&g_style_focus, C_BLUE);
    lv_style_set_bg_opa(&g_style_focus, LV_OPA_70);

    /* Create containers */
    g_cont_img = lv_obj_create(p);
    lv_obj_set_size(g_cont_img, 240, 200); lv_obj_set_pos(g_cont_img, 0, 0);
    lv_obj_set_style_border_width(g_cont_img, 0, 0);
    lv_obj_set_style_pad_all(g_cont_img, 2, 0);
    lv_obj_set_style_bg_opa(g_cont_img, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(g_cont_img, LV_FLEX_FLOW_COLUMN);

    g_cont_txt = lv_obj_create(p);
    lv_obj_set_size(g_cont_txt, 240, 200); lv_obj_set_pos(g_cont_txt, 0, 0);
    lv_obj_set_style_border_width(g_cont_txt, 0, 0);
    lv_obj_set_style_pad_all(g_cont_txt, 2, 0);
    lv_obj_set_style_bg_opa(g_cont_txt, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(g_cont_txt, LV_FLEX_FLOW_COLUMN);

    g_cont_vid = lv_obj_create(p);
    lv_obj_set_size(g_cont_vid, 240, 200); lv_obj_set_pos(g_cont_vid, 0, 0);
    lv_obj_set_style_border_width(g_cont_vid, 0, 0);
    lv_obj_set_style_pad_all(g_cont_vid, 2, 0);
    lv_obj_set_style_bg_opa(g_cont_vid, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(g_cont_vid, LV_FLEX_FLOW_COLUMN);

    /* Build file lists */
    memset(g_btns_img, 0, sizeof(g_btns_img));
    memset(g_names_img, 0, sizeof(g_names_img));
    g_cnt_img = media_build_list(g_cont_img, g_btns_img, g_names_img, ".raw");

    memset(g_btns_txt, 0, sizeof(g_btns_txt));
    memset(g_names_txt, 0, sizeof(g_names_txt));
    g_cnt_txt = media_build_list(g_cont_txt, g_btns_txt, g_names_txt, ".txt");

    memset(g_btns_vid, 0, sizeof(g_btns_vid));
    memset(g_names_vid, 0, sizeof(g_names_vid));
    g_cnt_vid = media_build_list(g_cont_vid, g_btns_vid, g_names_vid, ".vid");

    /* Set up input group */
    g_media_group = lv_group_get_default();
    if (!g_media_group) g_media_group = lv_group_create();
    lv_group_set_default(g_media_group);

    g_focus_img = 0; g_focus_txt = 0; g_focus_vid = 0;
    media_show_tab(0);
}

/* ================================================================== */
/* Init                                                               */
/* ================================================================== */

void lvgl_ui_init(void)
{
    lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE),
                          lv_palette_main(LV_PALETTE_RED),
                          false, FONT_14);

    lv_obj_set_style_bg_color(lv_scr_act(), C_BG, 0);

    g_tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 20);
    lv_obj_set_size(g_tabview, 240, 220);
    lv_obj_align(g_tabview, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(g_tabview, C_BG, 0);
    lv_obj_set_style_border_width(g_tabview, 0, 0);
    lv_obj_set_style_pad_column(g_tabview, 0, 0);

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(g_tabview);
    lv_obj_set_style_bg_color(tab_btns, C_PANEL, 0);
    lv_obj_set_style_border_width(tab_btns, 0, 0);

    lv_obj_t *t0 = lv_tabview_add_tab(g_tabview, "DASH");
    lv_obj_t *t1 = lv_tabview_add_tab(g_tabview, "MOTOR");
    lv_obj_t *t2 = lv_tabview_add_tab(g_tabview, "CTRL");
    lv_obj_t *t3 = lv_tabview_add_tab(g_tabview, "BODY");
    lv_obj_t *t4 = lv_tabview_add_tab(g_tabview, "MEDIA");

    lv_obj_set_style_bg_color(t0, C_BG, 0);
    lv_obj_set_style_bg_color(t1, C_BG, 0);
    lv_obj_set_style_bg_color(t2, C_BG, 0);
    lv_obj_set_style_bg_color(t3, C_BG, 0);
    lv_obj_set_style_bg_color(t4, C_BG, 0);

    page_dashboard(t0);
    page_motor(t1);
    page_control(t2);
    page_body(t3);
    page_media(t4);

    bar_init();
    lv_tabview_set_act(g_tabview, 0, LV_ANIM_OFF);
    g_cur_page = 0;
    page_focus_init(0);
}

/* ================================================================== */
/* 摇杆操作 — 简洁直接, 无复杂状态机                                   */
/*   LEFT/RIGHT: 永远切页 (最高优先级)                                 */
/*   DASH: UP/DOWN=驾驶模式, ENTER=点火                                 */
/*   CTRL: UP/DOWN=焦点导航, ENTER=操作焦点项                            */
/*   MOTOR/BODY: 纯显示                                                */
/*   ESC: 关弹窗                                                        */
/* ================================================================== */

void lvgl_ui_handle_key(uint32_t key)
{
    uint8_t pg = g_cur_page;

    /* ═══ MEDIA查看模式: 最高优先级, 覆盖全局切页/ESC ═══ */
    if (g_view == V_TXT) {
        if (key == LV_KEY_ESC) { g_view=V_NONE; lv_obj_invalidate(lv_scr_act()); media_show_tab(1); }
        else if (key == LV_KEY_NEXT && g_txt_scroll+15<g_txt_lc) { g_txt_scroll++; txt_render(); }
        else if (key == LV_KEY_PREV && g_txt_scroll>0) { g_txt_scroll--; txt_render(); }
        else if ((key==LV_KEY_LEFT||key==LV_KEY_RIGHT) && g_cnt_txt>1) {
            g_focus_txt=(key==LV_KEY_RIGHT)?(g_focus_txt+1)%g_cnt_txt:(g_focus_txt-1+g_cnt_txt)%g_cnt_txt;
            if(g_names_txt[g_focus_txt][0])view_txt(g_names_txt[g_focus_txt]);
        }
        return;
    }
    if (g_view == V_RAW) {
        if (key == LV_KEY_ESC) { g_view=V_NONE; lv_obj_invalidate(lv_scr_act()); media_show_tab(0); }
        else if ((key==LV_KEY_LEFT||key==LV_KEY_RIGHT) && g_cnt_img>1) {
            g_focus_img=(key==LV_KEY_RIGHT)?(g_focus_img+1)%g_cnt_img:(g_focus_img-1+g_cnt_img)%g_cnt_img;
            if(g_names_img[g_focus_img][0])view_raw(g_names_img[g_focus_img]);
        }
        return;
    }
    if (g_view == V_VID) {
        if (key == LV_KEY_ESC) { f_close(&g_vid_f); g_view=V_NONE; lv_obj_invalidate(lv_scr_act()); media_show_tab(2); }
        else if (key==LV_KEY_RIGHT && g_vid_fidx+1<g_vid_frames){g_vid_fidx++;vid_frame();}
        else if (key==LV_KEY_LEFT && g_vid_fidx>0){g_vid_fidx--;vid_frame();}
        return;
    }

    /* ═══ 全局 LEFT/RIGHT 切页 (MEDIA页除外) ═══ */
    if (pg != 4) {
        if (key == LV_KEY_LEFT && pg > 0) {
            g_edit_mode = 0;
            lv_tabview_set_act(g_tabview, --g_cur_page, LV_ANIM_ON);
            page_focus_init(g_cur_page);
            return;
        }
        if (key == LV_KEY_RIGHT && pg < 4) {
            g_edit_mode = 0;
            lv_tabview_set_act(g_tabview, ++g_cur_page, LV_ANIM_ON);
            page_focus_init(g_cur_page);
            return;
        }
    }

    /* ═══ ESC: 关弹窗 (MEDIA查看模式已在上面处理) ═══ */
    if (key == LV_KEY_ESC) {
        if (g_popup) { lv_msgbox_close(g_popup); g_popup = NULL; }
        return;
    }

    /* ═══ DASH页: UP/DOWN=驾驶模式, ENTER=点火 ═══ */
    /* DASH: ENTER点火 */
    if (pg == 0) {
        if (key == LV_KEY_ENTER) { FSM_IgnitionPress(); return; }
        return;
    }

    /* CTRL: UP/DOWN=切换驾驶模式, ENTER=A/C开关 */
    if (pg == 2) {
        if (key == LV_KEY_NEXT || key == LV_KEY_PREV) {
            DriveMode_t m = FSM_GetDriveMode();
            FSM_SetDriveMode(key == LV_KEY_NEXT ? (m > 0 ? (DriveMode_t)(m - 1) : DRIVE_MODE_SPORT)
                                               : (m < 2 ? (DriveMode_t)(m + 1) : DRIVE_MODE_ECO));
            update_mode_btns();
            return;
        }
        if (key == LV_KEY_ENTER) {
            FSM_ToggleAC();
            if (FSM_GetACState()) { lv_obj_add_state(g_sw_ac, LV_STATE_CHECKED); lv_label_set_text(g_lbl_ac, "ON"); }
            else                  { lv_obj_clear_state(g_sw_ac, LV_STATE_CHECKED); lv_label_set_text(g_lbl_ac, "OFF"); }
            return;
        }
        return;
    }
    if (pg == 4) {
        /* LEFT/RIGHT选文件 */
        if (key == LV_KEY_LEFT) {
            /* 首个文件→切到BODY页; 否则选上一个 */
            int cnt=0, *pf=NULL;
            if(g_media_tab==0){cnt=g_cnt_img;pf=&g_focus_img;}
            else if(g_media_tab==1){cnt=g_cnt_txt;pf=&g_focus_txt;}
            else{cnt=g_cnt_vid;pf=&g_focus_vid;}
            if(cnt>0&&*pf==0){lv_tabview_set_act(g_tabview,3,LV_ANIM_ON);g_cur_page=3;page_focus_init(3);return;}
            media_nav(0); return;
        }
        if (key == LV_KEY_RIGHT) { media_nav(1); return; }
        /* UP/DOWN切换子页面 IMG↔TXT↔VID */
        if (key == LV_KEY_PREV) { media_show_tab((g_media_tab+2)%3); return; }
        if (key == LV_KEY_NEXT) { media_show_tab((g_media_tab+1)%3); return; }
        /* ENTER打开 */
        if (key == LV_KEY_ENTER) {
            int idx; const char *name;
            if (g_media_tab==0) { idx=g_focus_img; name=g_names_img[idx]; }
            else if (g_media_tab==1) { idx=g_focus_txt; name=g_names_txt[idx]; }
            else { idx=g_focus_vid; name=g_names_vid[idx]; }
            if (name&&name[0]) {
                if (g_media_tab==0) view_raw(name);
                else if (g_media_tab==1) view_txt(name);
                else view_vid(name);
            }
            return;
        }
        return;
    }
}

void lvgl_ui_set_page(uint8_t id) { if(id<5){ lv_tabview_set_act(g_tabview,id,LV_ANIM_ON); g_cur_page=id; page_focus_init(id); } }
uint8_t lvgl_ui_get_page(void) { return g_cur_page; }
int lvgl_ui_is_viewing(void) { return (g_view==V_RAW||g_view==V_TXT||g_view==V_VID); }

/* ================================================================== */
/* CAN data update                                                    */
/* ================================================================== */

void lvgl_ui_on_can_update(void)
{
    const BMS_Status_t *bms = CAN_GetBMSStatus();
    const Motor_Status_t *mot = CAN_GetMotorStatus();
    const BCM_Status_t *bcm = CAN_GetBCMStatus();

    if (bms) {
        char b[20];
        if (g_arc_soc) { lv_arc_set_value(g_arc_soc, bms->soc); }
        if (g_lbl_soc) { snprintf(b,20,"%d%%",bms->soc); lv_label_set_text(g_lbl_soc,b); }
        if (g_lbl_volt){ snprintf(b,20,"%.1fV",bms->total_voltage/10.0f); lv_label_set_text(g_lbl_volt,b); }
        if (bms->fault_code) {
            char t[32]; snprintf(t,32,"BMS FAULT 0x%02X",bms->fault_code);
            popup(t,"Check Battery"); FSM_TriggerFault(bms->fault_code);
            Logger_FreezeEvent("BMS");
        }
    }

    /* ---- CAN电机数据 (来自F103, 优先) ---- */
    if (mot) {
        char b[20];
        int spd = mot->speed;
        if(g_speed_num){snprintf(b,20,"%d",spd/10);lv_label_set_text(g_speed_num,b);}
        if(g_arc_rpm){lv_arc_set_value(g_arc_rpm,spd>2000?2000:spd);}
        if(g_lbl_rpm){snprintf(b,20,"%d",spd);lv_label_set_text(g_lbl_rpm,b);}
        if(g_rpm_num){snprintf(b,20,"%d",spd);lv_label_set_text(g_rpm_num,b);}
        if(g_temp_lbl){snprintf(b,20,"TEMP %dC",mot->temp_controller-40);lv_label_set_text(g_temp_lbl,b);}
        if(g_lbl_temp){snprintf(b,20,"%dC",mot->temp_controller-40);lv_label_set_text(g_lbl_temp,b);}
        if(g_series)lv_chart_set_next_value(g_chart,g_series,(lv_coord_t)spd);
        if(mot->fault_status){
            char t[32];snprintf(t,32,"MOTOR FAULT 0x%02X",mot->fault_status);
            popup(t,"Check Motor");FSM_TriggerFault(mot->fault_status);Logger_FreezeEvent("MOTOR");
        }
    }
    /* ---- UART电机数据 (来自ESP32-S3, CAN无数据时用) ---- */
    else if (UART_IsMotorUpdated()) {
        uint16_t us = UART_GetMotorRPM();
        uint8_t  ut = UART_GetMotorTemp();
        char b[20];
        if(g_speed_num){snprintf(b,20,"%d",us/10);lv_label_set_text(g_speed_num,b);}
        if(g_arc_rpm){lv_arc_set_value(g_arc_rpm,us>2000?2000:us);}
        if(g_lbl_rpm){snprintf(b,20,"%d",us);lv_label_set_text(g_lbl_rpm,b);}
        if(g_rpm_num){snprintf(b,20,"%d",us);lv_label_set_text(g_rpm_num,b);}
        if(g_temp_lbl){snprintf(b,20,"TEMP %dC",ut-40);lv_label_set_text(g_temp_lbl,b);}
        if(g_lbl_temp){snprintf(b,20,"%dC",ut-40);lv_label_set_text(g_lbl_temp,b);}
        if(g_series)lv_chart_set_next_value(g_chart,g_series,(lv_coord_t)us);
    }

    if (bcm) {
        char b[32];
        if (g_v_light) { snprintf(b,32,"%s",(bcm->light_status&1)?"LOW BEAM":"OFF"); lv_label_set_text(g_v_light,b); }
        if (g_v_lock)  {
            snprintf(b,32,"%s",(bcm->lock_status&3)?"LOCKED":"UNLOCK");
            lv_label_set_text(g_v_lock,b);
            lv_obj_set_style_text_color(g_v_lock,(bcm->lock_status&3)?C_GREEN:C_AMBER,0);
        }
        if (g_v_win)   { snprintf(b,32,"%d%%",bcm->window_status); lv_label_set_text(g_v_win,b); }
    }

    update_mode_btns();
    {
        /* DASH点火按钮: 显示当前状态 */
        const char *sn[]={"SLEEP","ACC","IGN","DRIVE"};
        if(g_dash_btn[0]) lv_label_set_text(lv_obj_get_child(g_dash_btn[0],0), sn[FSM_GetState()]);
    }

    {
        const char *sn[] = {"SLEEP","ACC","IGN","DRIVE","CHARGE","FAULT"};
        const char *mn[] = {"ECO","COMFORT","SPORT"};
        uint32_t can1_err = CAN_GetError(CAN_BUS_1);
        char bar_s[48];
        extern volatile uint32_t g_can1_rx_cnt;
        extern volatile uint32_t g_can1_tx_cnt;
        uint16_t urpm = UART_GetMotorRPM();
        if (bms||mot) {
            snprintf(bar_s,sizeof(bar_s),"CAN OK %lurx U:%drpm %dkm/h",
                     g_can1_rx_cnt, urpm, (int)(mot?mot->speed/10:0));
            bar_set(bar_s, C_GREEN);
        } else if (urpm > 0) {
            snprintf(bar_s,sizeof(bar_s),"UART motor:%drpm | %s %s",
                     urpm, sn[FSM_GetState()], mn[FSM_GetDriveMode()]);
            bar_set(bar_s, C_BLUE);
        } else if (UART_IsMotorUpdated()) {
            snprintf(bar_s,sizeof(bar_s),"UART recv! RPM:%d", urpm);
            bar_set(bar_s, C_BLUE);
        } else if (can1_err) {
            snprintf(bar_s,sizeof(bar_s),"CAN ERR:0x%08lX RX:%lu TX:%lu",
                     can1_err, g_can1_rx_cnt, g_can1_tx_cnt);
            bar_set(bar_s, C_RED);
        } else {
            snprintf(bar_s,sizeof(bar_s),"idle CAN:%lurx U:%drpm",
                     g_can1_rx_cnt, urpm);
            bar_set(bar_s, C_AMBER);
        }
    }

    {
        const Fault_t *f = CAN_GetLastFault();
        if (f) { char t[32]; snprintf(t,32,"Node %d FAULT",f->node_id); popup(t,"CAN Bus Fault"); }
    }
}
