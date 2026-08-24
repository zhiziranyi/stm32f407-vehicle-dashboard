/**
 * vehicle_fsm.c — 整车状态机实现
 *
 * 状态流转:
 *   SLEEP → ACCESSORY → IGNITION → DRIVING
 *   FAULT 可介入任何状态
 *   CHARGING 可在 DRIVING/SLEEP 时触发
 *
 * CAN 发送:
 *   0x300 (整车状态) 每 100ms: vehicle_state + drive_mode + speed
 *   0x301 (控制指令) 每 100ms: AC开关+温度 + 能量回收等级
 */

#include "vehicle_fsm.h"
#include "can_driver.h"
#include "can_protocol.h"
#include "can_parser.h"
#include "uart_comm.h"
#include <string.h>

/* ================================================================== */
/* 静态变量                                                           */
/* ================================================================== */

static VehicleState_t g_state      = VEHICLE_STATE_SLEEP;
static VehicleState_t g_prev_state = VEHICLE_STATE_SLEEP;
static DriveMode_t    g_drive_mode = DRIVE_MODE_COMFORT;
static uint8_t        g_ac_on      = 0;
static uint8_t        g_ac_temp    = 24;
static uint8_t        g_regen_lv   = 1;
static uint16_t       g_vehicle_speed = 0;
static uint32_t       g_last_can_tick = 0;

/* ================================================================== */
/* 状态名称                                                           */
/* ================================================================== */

static const char *g_state_names[] = {
    "SLEEP",
    "ACCESSORY",
    "IGNITION",
    "DRIVING",
    "CHARGING",
    "FAULT"
};

const char *FSM_GetStateName(VehicleState_t state)
{
    if (state < 6) return g_state_names[state];
    return "UNKNOWN";
}

/* ================================================================== */
/* 状态管理                                                           */
/* ================================================================== */

void FSM_Init(void)
{
    g_state       = VEHICLE_STATE_SLEEP;
    g_prev_state  = VEHICLE_STATE_SLEEP;
    g_drive_mode  = DRIVE_MODE_COMFORT;
    g_vehicle_speed = 0;
}

VehicleState_t FSM_GetState(void)
{
    return g_state;
}

int FSM_Transition(VehicleState_t new_state)
{
    if (new_state == g_state) return 0;
    if (new_state >= 6) return -1;

    /* 合法转换检查 */
    int valid = 0;
    switch (g_state) {
    case VEHICLE_STATE_SLEEP:
        valid = (new_state == VEHICLE_STATE_ACCESSORY ||
                 new_state == VEHICLE_STATE_FAULT);
        break;
    case VEHICLE_STATE_ACCESSORY:
        valid = (new_state == VEHICLE_STATE_IGNITION ||
                 new_state == VEHICLE_STATE_SLEEP ||
                 new_state == VEHICLE_STATE_FAULT);
        break;
    case VEHICLE_STATE_IGNITION:
        valid = (new_state == VEHICLE_STATE_DRIVING ||
                 new_state == VEHICLE_STATE_ACCESSORY ||
                 new_state == VEHICLE_STATE_FAULT);
        break;
    case VEHICLE_STATE_DRIVING:
        valid = (new_state == VEHICLE_STATE_IGNITION ||
                 new_state == VEHICLE_STATE_CHARGING ||
                 new_state == VEHICLE_STATE_FAULT);
        break;
    case VEHICLE_STATE_CHARGING:
        valid = (new_state == VEHICLE_STATE_ACCESSORY ||
                 new_state == VEHICLE_STATE_FAULT);
        break;
    case VEHICLE_STATE_FAULT:
        valid = (new_state != VEHICLE_STATE_FAULT);  /* 可转任何状态 */
        break;
    }

    if (!valid) return -1;

    g_prev_state = g_state;
    g_state = new_state;

    /* 状态联动 */
    switch (new_state) {
    case VEHICLE_STATE_SLEEP:
        g_vehicle_speed = 0;
        break;
    case VEHICLE_STATE_DRIVING:
        /* 根据驾驶模式设定基准车速 */
        switch (g_drive_mode) {
        case DRIVE_MODE_ECO:     g_vehicle_speed = 300; break;
        case DRIVE_MODE_COMFORT: g_vehicle_speed = 600; break;
        case DRIVE_MODE_SPORT:   g_vehicle_speed = 900; break;
        }
        break;
    default:
        break;
    }

    return 0;
}

/* ================================================================== */
/* 用户操作                                                           */
/* ================================================================== */

void FSM_IgnitionPress(void)
{
    switch (g_state) {
    case VEHICLE_STATE_SLEEP:
        FSM_Transition(VEHICLE_STATE_ACCESSORY);
        break;
    case VEHICLE_STATE_ACCESSORY:
        FSM_Transition(VEHICLE_STATE_IGNITION);
        break;
    case VEHICLE_STATE_IGNITION:
        FSM_Transition(VEHICLE_STATE_DRIVING);
        break;
    default:
        break;
    }
}

void FSM_StartCharging(void)
{
    if (g_state == VEHICLE_STATE_DRIVING || g_state == VEHICLE_STATE_SLEEP)
        FSM_Transition(VEHICLE_STATE_CHARGING);
}

void FSM_StopCharging(void)
{
    if (g_state == VEHICLE_STATE_CHARGING)
        FSM_Transition(VEHICLE_STATE_ACCESSORY);
}

void FSM_TriggerFault(uint8_t fault_code)
{
    FSM_Transition(VEHICLE_STATE_FAULT);
    (void)fault_code;
}

void FSM_ClearFault(void)
{
    if (g_state == VEHICLE_STATE_FAULT)
        FSM_Transition(g_prev_state);
}

/* ================================================================== */
/* 驾驶模式 / 空调 / 回收                                             */
/* ================================================================== */

DriveMode_t FSM_GetDriveMode(void)     { return g_drive_mode; }

void FSM_SetDriveMode(DriveMode_t mode)
{
    g_drive_mode = mode;
    UART_SendDriveMode((uint8_t)mode);  /* 通知ESP32切换电机转速 */
    if (g_state == VEHICLE_STATE_DRIVING) {
        switch (mode) {
        case DRIVE_MODE_ECO:     g_vehicle_speed = 300; break;
        case DRIVE_MODE_COMFORT: g_vehicle_speed = 600; break;
        case DRIVE_MODE_SPORT:   g_vehicle_speed = 1000; break;
        }
    }
}

uint8_t FSM_GetACState(void)     { return g_ac_on; }
void    FSM_ToggleAC(void)       { g_ac_on = !g_ac_on; }
uint8_t FSM_GetRegenLevel(void)  { return g_regen_lv; }

void FSM_SetRegenLevel(uint8_t level)
{
    if (level <= 3) g_regen_lv = level;
}

uint16_t FSM_GetVehicleSpeed(void)
{
    return g_vehicle_speed;
}

/* ================================================================== */
/* 周期性处理 — 发送 CAN 报文                                          */
/* ================================================================== */

void FSM_Process(void)
{
    uint32_t now = HAL_GetTick();

    /* 每 100ms 发送整车状态 + 控制指令 */
    if (now - g_last_can_tick < 100) return;
    g_last_can_tick = now;

    /* ---- 0x300: 整车状态 ---- */
    if (g_state >= VEHICLE_STATE_IGNITION) {
        uint8_t data[8] = {0};
        data[0] = (uint8_t)g_state;
        data[1] = (uint8_t)g_drive_mode;
        data[2] = (uint8_t)(g_vehicle_speed >> 8);
        data[3] = (uint8_t)(g_vehicle_speed);
        CAN_SendStdMsg(CAN_BUS_1, CAN_ID_VEHICLE_STATUS, data, 4);
    }

    /* ---- 0x301: 控制指令 ---- */
    {
        uint8_t data[8] = {0};
        data[0] = (g_ac_on << 7) | (g_ac_temp & 0x7F);
        data[1] = g_regen_lv;
        CAN_SendStdMsg(CAN_BUS_1, CAN_ID_VEHICLE_CONTROL, data, 2);
    }

    /* ---- 0x400: BCM 车身状态 (CAN2) ---- */
    {
        uint8_t data[8] = {0};
        data[0] = 0x01;  /* 灯光: 近光 */
        data[1] = 0x03;  /* 门锁: 全锁 */
        data[2] = 100;   /* 车窗: 100% */
        CAN_SendStdMsg(CAN_BUS_2, CAN_ID_BCM_STATUS, data, 3);
    }
}
