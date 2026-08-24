/**
 * can_protocol.h — CAN 通信矩阵定义 (cheji407 和 cheji103 共用)
 *
 * 信号矩阵表:
 *   0x100: BMS状态 (1000ms周期, 动力CAN)
 *   0x200: 电机状态 (100ms周期, 动力CAN)
 *   0x300: 整车状态 (100ms周期, 动力CAN)
 *   0x301: 整车控制 (100ms周期, 动力CAN)
 *   0x400: BCM车身状态 (500ms周期, 车身CAN)
 *   0x401: 车身控制指令 (500ms周期, 车身CAN)
 *   0x500: 故障码 (事件触发, 两网段)
 *   0x7XX: 诊断 (按需, 两网段)
 */

#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* CAN ID 定义                                                        */
/* ================================================================== */

#define CAN_ID_BMS_STATUS       0x100   /**< BMS 电池状态 */
#define CAN_ID_MOTOR_STATUS     0x200   /**< 电机控制器状态 */
#define CAN_ID_VEHICLE_STATUS   0x300   /**< 整车状态 */
#define CAN_ID_VEHICLE_CONTROL  0x301   /**< 整车控制指令 */
#define CAN_ID_BCM_STATUS       0x400   /**< BCM 车身状态 */
#define CAN_ID_BCM_CONTROL      0x401   /**< 车身控制指令 */
#define CAN_ID_FAULT            0x500   /**< 故障码 */

/* ================================================================== */
/* 0x100 — BMS 电池状态 (1000ms周期)                                   */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t total_voltage;     /**< Byte0-1: 总电压, 0.1V/bit */
    uint16_t total_current;     /**< Byte2-3: 总电流, 0.1A/bit, 充电为负 */
    uint8_t  soc;               /**< Byte4:   SOC, 0-100% */
    uint8_t  temp_max;          /**< Byte5:   最高单体温度, °C, offset -40 */
    uint8_t  temp_min;          /**< Byte6:   最低单体温度, °C, offset -40 */
    uint8_t  fault_code;        /**< Byte7:   故障码, 0=正常 */
} BMS_Status_t;

/* ================================================================== */
/* 0x200 — 电机控制器状态 (100ms周期)                                   */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t speed;             /**< Byte0-1: 电机转速, rpm */
    uint8_t  temp_controller;   /**< Byte2:   控制器温度, °C, offset -40 */
    uint8_t  fault_status;      /**< Byte3:   故障状态, 0=正常 */
} Motor_Status_t;

/* ================================================================== */
/* 0x300 — 整车状态 (100ms周期)                                        */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  vehicle_state;     /**< Byte0: 整车状态 0-5 */
    uint8_t  drive_mode;        /**< Byte1: 驾驶模式 0=经济 1=舒适 2=运动 */
    uint16_t speed;             /**< Byte2-3: 车速, 0.1km/h/bit */
} Vehicle_Status_t;

/** 整车状态枚举 */
typedef enum {
    VEHICLE_STATE_SLEEP      = 0,
    VEHICLE_STATE_ACCESSORY  = 1,
    VEHICLE_STATE_IGNITION   = 2,
    VEHICLE_STATE_DRIVING    = 3,
    VEHICLE_STATE_CHARGING   = 4,
    VEHICLE_STATE_FAULT      = 5
} Vehicle_State_e;

/** 驾驶模式枚举 */
typedef enum {
    DRIVE_MODE_ECO     = 0,
    DRIVE_MODE_COMFORT = 1,
    DRIVE_MODE_SPORT   = 2
} Drive_Mode_e;

/* ================================================================== */
/* 0x301 — 整车控制指令 (100ms周期)                                     */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  ac_switch_temp;    /**< Byte0: bit7=AC开关, bit6-0=温度(16-30°C) */
    uint8_t  regen_level;       /**< Byte1: 能量回收等级 0-3 */
} Vehicle_Control_t;

/* ================================================================== */
/* 0x400 — BCM 车身状态 (500ms周期)                                    */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  light_status;      /**< Byte0: bit0=近光, bit1=远光, bit2=转向灯 */
    uint8_t  lock_status;       /**< Byte1: bit0=主驾锁, bit1=全车锁 */
    uint8_t  window_status;     /**< Byte2: 车窗位置 0-100% */
} BCM_Status_t;

/* ================================================================== */
/* 0x401 — 车身控制指令 (500ms周期)                                     */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  cmd;               /**< Byte0: bit0=灯光, bit1=门锁, bit2=车窗 */
} BCM_Control_t;

/* ================================================================== */
/* 0x500 — 故障码 (事件触发)                                           */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  node_id;           /**< Byte0: 故障节点ID */
    uint16_t fault_code;        /**< Byte1-2: 故障码 */
    uint8_t  fault_data[5];     /**< Byte3-7: 故障数据 */
} Fault_t;

#ifdef __cplusplus
}
#endif

#endif /* CAN_PROTOCOL_H */
