/**
 * cheji407 — STM32F407ZGT6 车载主控与车身网关
 *
 * 系统初始化: 时钟 168MHz, TFT屏, SD卡, LVGL, CAN, UART
 */

#include "stm32f4xx_hal.h"
#include "tft_display.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_fs.h"
#include "joystick.h"
#include "vehicle_fsm.h"
#include "data_logger.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "lvgl_ui.h"
#include "can_driver.h"
#include "can_parser.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* 全局变量                                                           */
/* ------------------------------------------------------------------ */

static FATFS g_fs;
volatile uint32_t g_can1_rx_cnt = 0;
volatile uint32_t g_can1_tx_cnt = 0;

/* ------------------------------------------------------------------ */
/* CAN 接收回调                                                        */
/* ------------------------------------------------------------------ */

static void CAN1_RxCallback(CAN_Msg_t *msg)
{
    g_can1_rx_cnt++;
    CAN_ParseMessage(msg);
    lvgl_ui_on_can_update();
}

static void CAN2_RxCallback(CAN_Msg_t *msg)
{
    /* 解析车身CAN报文 + 更新GUI */
    CAN_ParseMessage(msg);
    lvgl_ui_on_can_update();
}

/* ------------------------------------------------------------------ */
/* 系统时钟 168MHz (HSE 8MHz → PLL: M=8, N=336, P=2, Q=7)            */
/* ------------------------------------------------------------------ */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* ------------------------------------------------------------------ */
/* SysTick — 驱动 HAL 和 LVGL 的节拍时钟                              */
/* ------------------------------------------------------------------ */

void SysTick_Handler(void)
{
    HAL_IncTick();
    lv_tick_inc(1);
}

/* ------------------------------------------------------------------ */
/* SD 卡初始化                                                        */
/* ------------------------------------------------------------------ */

static int SD_Mount(void)
{
    char path[4];

    if (FATFS_LinkDriver(&SD_Driver, path) != 0) {
        return -1;
    }

    FRESULT fres = f_mount(&g_fs, "0:", 1);
    if (fres != FR_OK) {
        BYTE work[4096];
        fres = f_mkfs("0:", FM_FAT32, 0, work, sizeof(work));
        if (fres != FR_OK) return -2;
        fres = f_mount(&g_fs, "0:", 1);
        if (fres != FR_OK) return -3;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* 主函数                                                             */
/* ------------------------------------------------------------------ */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* 初始化 TFT 屏幕 */
    TFT_Init();
    HAL_Delay(200);

    /* 挂载 SD 卡 */
    if (SD_Mount() != 0) {
        /* SD 卡初始化失败 — 红灯常亮，继续运行（无 SD 也可显示） */
        TFT_FillScreen(COLOR_RED);
        HAL_Delay(1000);
    }

    /* 初始化 LVGL */
    lv_init();
    lv_port_disp_init();
    lv_port_fs_init();
    joystick_init();

    /* 初始化 GUI 界面 */
    lvgl_ui_init();

    /* 初始化状态机 */
    FSM_Init();

    /* 初始化双路 CAN */
    CAN1_Init();   /* 动力CAN 500kbps */
    CAN2_Init();   /* 车身CAN 250kbps */
    CAN_RegisterRxCallback(CAN_BUS_1, CAN1_RxCallback);
    CAN_RegisterRxCallback(CAN_BUS_2, CAN2_RxCallback);

    /* 初始化数据记录器 */
    Logger_Init();

    /* 主循环 */
    uint32_t last_diag     = 0;
    while (1) {
        uint32_t now = HAL_GetTick();

        /* 每200ms: 轮询CAN RX + 刷新UI(电机数据来自UART) */
        if (now - last_diag >= 200) {
            last_diag = now;
            /* 轮询接收 (绕过中断, 直接读FIFO) */
            CAN_HandleTypeDef *h = CAN_GetHandle(CAN_BUS_1);
            CAN_RxHeaderTypeDef rh;
            uint8_t d[8];
            while (HAL_CAN_GetRxFifoFillLevel(h, CAN_RX_FIFO0) > 0) {
                if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &rh, d) == HAL_OK) {
                    g_can1_rx_cnt++;  /* 轮询收到 */
                    CAN_Msg_t m;
                    m.id = rh.IDE==CAN_ID_STD ? rh.StdId : rh.ExtId;
                    m.dlc = rh.DLC;
                    m.ide = (rh.IDE==CAN_ID_EXT) ? 1 : 0;
                    m.rtr = (rh.RTR==CAN_RTR_REMOTE) ? 1 : 0;
                    memcpy(m.data, d, m.dlc>8?8:m.dlc);
                    CAN_ParseMessage(&m);
                }
            }
            lvgl_ui_on_can_update();
        }

        /* LVGL 定时器处理 (媒体看图模式跳过) */
        if (!lvgl_ui_is_viewing()) lv_timer_handler();

        /* 摇杆输入 → UI 导航 */
        uint8_t joy = joystick_read();
        if (joy != JOY_NONE) {
            uint32_t key = 0;
            switch (joy) {
            case JOY_UP:    key = LV_KEY_PREV;  break;
            case JOY_DOWN:  key = LV_KEY_NEXT;  break;
            case JOY_LEFT:  key = LV_KEY_LEFT;  break;
            case JOY_RIGHT: key = LV_KEY_RIGHT; break;
            case JOY_SHORT: key = LV_KEY_ENTER; break;
            case JOY_LONG:  key = LV_KEY_ESC;   break;
            }
            lvgl_ui_handle_key(key);

            if (joy == JOY_SHORT && lvgl_ui_get_page() == 0) {
                FSM_IgnitionPress();
            }
        }

        /* 状态机处理 (发送 CAN 控制帧) */
        FSM_Process();

        HAL_Delay(5);
    }
}
