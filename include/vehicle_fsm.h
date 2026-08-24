/**
 * vehicle_fsm.h — 整车状态机 (6级)
 *
 * 状态流转: SLEEP → ACCESSORY → IGNITION → DRIVING
 *              ↑         ↓           ↓
 *            FAULT ←  ←  ←  ←  ←  ←
 *              ↓
 *          CHARGING ↔ DRIVING
 */

#ifndef VEHICLE_FSM_H
#define VEHICLE_FSM_H

#include <stdint.h>
#include "can_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* 类型别名 (复用 can_protocol.h 的定义)                                */
/* ================================================================== */

typedef Vehicle_State_e VehicleState_t;
typedef Drive_Mode_e    DriveMode_t;

/* ================================================================== */
/* API                                                                */
/* ================================================================== */

/** 初始化状态机 */
void FSM_Init(void);

/** 获取当前状态 */
VehicleState_t FSM_GetState(void);

/** 获取状态名称字符串 */
const char *FSM_GetStateName(VehicleState_t state);

/** 状态转换请求 (返回 0=成功, -1=非法转换) */
int FSM_Transition(VehicleState_t new_state);

/** 点火按钮 (切状态: SLEEP→ACC→IGN→DRV) */
void FSM_IgnitionPress(void);

/** 进入充电模式 */
void FSM_StartCharging(void);

/** 退出充电 */
void FSM_StopCharging(void);

/** 触发故障 (自动进入 FAULT 状态) */
void FSM_TriggerFault(uint8_t fault_code);

/** 清除故障 (恢复到之前的正常状态) */
void FSM_ClearFault(void);

/** 获取驾驶模式 */
DriveMode_t FSM_GetDriveMode(void);

/** 设置驾驶模式 */
void FSM_SetDriveMode(DriveMode_t mode);

/** 获取空调状态 */
uint8_t FSM_GetACState(void);

/** 切换空调开关 */
void FSM_ToggleAC(void);

/** 获取能量回收等级 (0-3) */
uint8_t FSM_GetRegenLevel(void);

/** 设置能量回收等级 */
void FSM_SetRegenLevel(uint8_t level);

/** 获取车速 (模拟) */
uint16_t FSM_GetVehicleSpeed(void);

/** 周期性处理 (发送 CAN 报文等) */
void FSM_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_FSM_H */
