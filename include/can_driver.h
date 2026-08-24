/**
 * can_driver.h — STM32F407 双路 CAN 总线驱动
 *
 * CAN1: PD0(RX) / PD1(TX) — 动力CAN网段 500kbps
 * CAN2: PB12(RX) / PB13(TX) — 车身CAN网段 250kbps
 */

#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* 类型定义                                                           */
/* ================================================================== */

/** CAN 消息帧 */
typedef struct {
    uint32_t id;        /**< CAN ID (标准帧 11-bit 或 扩展帧 29-bit) */
    uint8_t  data[8];   /**< 数据场 (0-8 字节) */
    uint8_t  dlc;       /**< 数据长度 (0-8) */
    uint8_t  ide;       /**< 0=标准帧, 1=扩展帧 */
    uint8_t  rtr;       /**< 0=数据帧, 1=远程帧 */
} CAN_Msg_t;

/** CAN 接收回调函数类型 */
typedef void (*CAN_RxCallback_t)(CAN_Msg_t *msg);

/** CAN 总线索引 */
typedef enum {
    CAN_BUS_1 = 0,
    CAN_BUS_2 = 1,
    CAN_BUS_MAX
} CAN_Bus_t;

/* ================================================================== */
/* API 函数                                                           */
/* ================================================================== */

/**
 * @brief 初始化 CAN1 (PD0/PD1, 500kbps)
 * @note  先进入回环模式测试，通过后改回正常模式
 */
void CAN1_Init(void);

/**
 * @brief 初始化 CAN2 (PB12/PB13, 250kbps)
 */
void CAN2_Init(void);

/**
 * @brief 发送 CAN 消息
 * @param bus  CAN 总线索引
 * @param msg  消息指针
 * @return 0=成功, 非0=失败
 */
int CAN_SendMsg(CAN_Bus_t bus, const CAN_Msg_t *msg);

/**
 * @brief 便捷发送: 标准数据帧
 * @param bus  CAN 总线索引
 * @param id   11-bit CAN ID
 * @param data 数据指针
 * @param len  数据长度 (0-8)
 * @return 0=成功, 非0=失败
 */
int CAN_SendStdMsg(CAN_Bus_t bus, uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief 注册接收回调
 * @param bus  CAN 总线索引
 * @param cb   回调函数, NULL=取消回调
 */
void CAN_RegisterRxCallback(CAN_Bus_t bus, CAN_RxCallback_t cb);

/**
 * @brief CAN 总线错误计数查询
 * @param bus  CAN 总线索引
 * @return HAL_CAN_GetError() 返回值
 */
uint32_t CAN_GetError(CAN_Bus_t bus);

/**
 * @brief 获取 CAN 句柄 (供 HAL 中断使用)
 */
CAN_HandleTypeDef *CAN_GetHandle(CAN_Bus_t bus);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DRIVER_H */
