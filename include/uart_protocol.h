/**
 * uart_protocol.h — F407 ↔ ESP32-S3 串口私有协议 (两项目共用)
 *
 * 帧格式:
 *   | 帧头1 | 帧头2 | 命令字 | 数据长度 | 数据(N) | 校验和 |
 *   | 0xAA  | 0x55  | 1B     | 1B       | 0-255B  | 1B     |
 *
 * 校验和 = (CMD + LEN + 全部DATA字节) & 0xFF
 */

#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* 帧常量                                                             */
/* ================================================================== */

#define FRAME_HEADER1   0xAA
#define FRAME_HEADER2   0x55
#define FRAME_MAX_DATA  128         /**< 最大数据长度 */
#define FRAME_MAX_SIZE  (FRAME_MAX_DATA + 5)  /**< 最大帧长 */

/* ================================================================== */
/* 命令字定义                                                         */
/* ================================================================== */

#define UART_CMD_VEHICLE_REPORT   0x01  /**< 车况数据上报 (F407→ESP32) */
#define UART_CMD_REMOTE_CONTROL   0x02  /**< 远程控制指令 (ESP32→F407) */
#define UART_CMD_CONTROL_RESP     0x03  /**< 控制执行结果 (F407→ESP32) */
#define UART_CMD_HEARTBEAT        0x04  /**< 心跳 (F407→ESP32, 每秒) */
#define UART_CMD_OTA_PACKET       0x10  /**< OTA 固件包 (ESP32→F407) */
#define UART_CMD_OTA_RESP         0x11  /**< OTA 应答 (F407→ESP32) */
#define UART_CMD_ACK              0xF0  /**< 通用 ACK */
#define UART_CMD_NAK              0xF1  /**< 通用 NAK */

/* ================================================================== */
/* 车况上报数据结构 (CMD=0x01, LEN=18)                                 */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t speed;           /**< Byte0-1: 车速, 0.1km/h */
    uint8_t  soc;             /**< Byte2:   SOC, 0-100% */
    uint32_t odometer;        /**< Byte3-6: 总里程, km */
    uint8_t  fault_code;      /**< Byte7:   故障码 */
    uint8_t  drive_mode;      /**< Byte8:   驾驶模式 0/1/2 */
    uint8_t  vehicle_state;   /**< Byte9:   整车状态 0-5 */
    uint16_t motor_rpm;       /**< Byte10-11: 电机转速 */
    uint8_t  ac_status;       /**< Byte12:  AC状态 bit7=开关, bit6-0=温度 */
    uint8_t  regen_level;     /**< Byte13:  能量回收等级 */
    uint16_t bms_voltage;     /**< Byte14-15: 电池电压 0.1V */
    int16_t  bms_current;     /**< Byte16-17: 电池电流 0.1A */
} VehicleReport_t;

/* ================================================================== */
/* 远程控制指令 (CMD=0x02, LEN=4)                                      */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;        /**< Byte0: 0=空调, 1=门锁, 2=限速, 3=驾驶模式 */
    uint8_t  param1;          /**< Byte1: 参数1 */
    uint8_t  param2;          /**< Byte2: 参数2 */
    uint8_t  param3;          /**< Byte3: 参数3 */
} RemoteCmd_t;

/* 远程指令类型 */
#define REMOTE_CMD_AC         0   /**< param1=开关(0/1), param2=温度(16-30) */
#define REMOTE_CMD_LOCK       1   /**< param1=0解锁/1上锁 */
#define REMOTE_CMD_SPEED_LIMIT 2  /**< param1-2=限速值(km/h) */
#define REMOTE_CMD_DRIVE_MODE 3   /**< param1=模式(0/1/2) */

/* ================================================================== */
/* 控制响应 (CMD=0x03, LEN=2)                                         */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;        /**< 原指令类型 */
    uint8_t  result;          /**< 0=成功, 1=失败 */
} ControlResp_t;

/* ================================================================== */
/* 心跳 (CMD=0x04, LEN=1)                                             */
/* ================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  state;           /**< 当前整车状态 */
} Heartbeat_t;

/* ================================================================== */
/* 帧结构体 (内部使用)                                                 */
/* ================================================================== */

typedef struct {
    uint8_t  cmd;
    uint8_t  len;
    uint8_t  data[FRAME_MAX_DATA];
    uint8_t  checksum;
} UART_Frame_t;

#ifdef __cplusplus
}
#endif

#endif /* UART_PROTOCOL_H */
