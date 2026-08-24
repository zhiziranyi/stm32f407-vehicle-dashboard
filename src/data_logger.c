/**
 * data_logger.c — 车载数据记录器实现
 *
 * CSV 格式: timestamp_ms, speed, soc, voltage, current, rpm, state, fault
 * 文件命名: log_YYYYMMDD_HHMMSS.csv
 * 事件文件: event_YYYYMMDD_HHMMSS_type.csv (含前后各 10s 数据)
 *
 * 循环缓冲区: 保留最新的 100 条记录 (10s @ 100ms) 用于事件冻结
 */

#include "data_logger.h"
#include "can_parser.h"
#include "vehicle_fsm.h"
#include "ff.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ================================================================== */
/* 循环缓冲区 (10秒 × 10Hz = 100条)                                    */
/* ================================================================== */

#define RING_BUF_SIZE   100

typedef struct {
    uint32_t timestamp_ms;
    uint16_t speed;
    uint8_t  soc;
    uint16_t voltage;
    int16_t  current;
    uint16_t motor_rpm;
    uint8_t  motor_temp;
    uint8_t  vehicle_state;
    uint8_t  fault_code;
} LogEntry_t;

static LogEntry_t g_ring[RING_BUF_SIZE];
static uint8_t    g_ring_idx = 0;
static uint8_t    g_ring_full = 0;

static FIL        g_log_file;
static uint8_t    g_file_open = 0;
static uint32_t   g_last_write = 0;

/* ================================================================== */
/* 初始化 — 创建日志文件                                               */
/* ================================================================== */

int Logger_Init(void)
{
    char fname[32];
    /* 使用序号命名，简化 (无 RTC) */
    snprintf(fname, sizeof(fname), "0:log_001.csv");

    /* 尝试创建新文件 */
    FRESULT res = f_open(&g_log_file, fname,
                         FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) return -1;

    /* 写入 CSV 表头 */
    f_printf(&g_log_file,
        "ts_ms,speed,soc,voltage,current,rpm,motor_temp,state,fault\n");
    f_sync(&g_log_file);

    g_file_open = 1;
    g_last_write = HAL_GetTick();
    return 0;
}

/* ================================================================== */
/* 采集当前数据                                                        */
/* ================================================================== */

static void collect_entry(LogEntry_t *e)
{
    const BMS_Status_t *bms = CAN_GetBMSStatus();
    const Motor_Status_t *mot = CAN_GetMotorStatus();

    e->timestamp_ms  = HAL_GetTick();
    e->speed         = FSM_GetVehicleSpeed();
    e->soc           = bms ? bms->soc : 0;
    e->voltage       = bms ? bms->total_voltage : 0;
    e->current       = bms ? bms->total_current : 0;
    e->motor_rpm     = mot ? mot->speed : 0;
    e->motor_temp    = mot ? mot->temp_controller : 0;
    e->vehicle_state = (uint8_t)FSM_GetState();
    e->fault_code    = bms ? bms->fault_code : 0;
}

/* ================================================================== */
/* 写入一行                                                            */
/* ================================================================== */

void Logger_WriteRecord(void)
{
    uint32_t now = HAL_GetTick();

    if (!g_file_open) return;
    if (now - g_last_write < 100) return;  /* 100ms 周期 */
    g_last_write = now;

    LogEntry_t e;
    collect_entry(&e);

    /* 写入 SD 卡 */
    f_printf(&g_log_file,
        "%lu,%u,%u,%u,%d,%u,%u,%u,%u\n",
        e.timestamp_ms, e.speed, e.soc,
        e.voltage, e.current, e.motor_rpm,
        e.motor_temp, e.vehicle_state, e.fault_code);

    /* 每 10 行 sync 一次，防止断电丢数据 */
    static int line = 0;
    if (++line % 10 == 0) f_sync(&g_log_file);

    /* 存入选转缓冲区 */
    memcpy(&g_ring[g_ring_idx], &e, sizeof(LogEntry_t));
    g_ring_idx = (g_ring_idx + 1) % RING_BUF_SIZE;
    if (g_ring_idx == 0) g_ring_full = 1;
}

/* ================================================================== */
/* 事件冻结 — 保存前后各 10 秒 (100 条)                                */
/* ================================================================== */

void Logger_FreezeEvent(const char *event_type)
{
    if (!g_file_open) return;

    char fname[40];
    snprintf(fname, sizeof(fname), "0:event_%lu.csv", HAL_GetTick());

    FIL fevent;
    FRESULT res = f_open(&fevent, fname,
                         FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) return;

    f_printf(&fevent,
        "ts_ms,speed,soc,voltage,current,rpm,motor_temp,state,fault\n");

    int count = g_ring_full ? RING_BUF_SIZE : g_ring_idx;
    int start = g_ring_full ? g_ring_idx : 0;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % RING_BUF_SIZE;
        LogEntry_t *e = &g_ring[idx];
        f_printf(&fevent,
            "%lu,%u,%u,%u,%d,%u,%u,%u,%u\n",
            e->timestamp_ms, e->speed, e->soc,
            e->voltage, e->current, e->motor_rpm,
            e->motor_temp, e->vehicle_state, e->fault_code);
    }

    f_close(&fevent);
}

/* ================================================================== */
/* 存储管理 — 占位 (完整实现需要遍历目录)                               */
/* ================================================================== */

void Logger_ManageStorage(void)
{
    /* 简化: 不自动删除, 由用户手动管理 SD 卡 */
    /* 完整版: f_opendir + f_readdir 找到最旧文件并删除 */
}
