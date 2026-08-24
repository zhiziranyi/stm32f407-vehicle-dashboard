/**
 * uart_comm.h — F407 USART1 串口通信模块
 *
 * USART1: PA9(TX) / PA10(RX), 115200bps
 * 协议: uart_protocol.h 定义的私有帧
 * 接收: DMA + IDLE 中断实现不定长帧接收
 */

#ifndef UART_COMM_H
#define UART_COMM_H

#include "stm32f4xx_hal.h"
#include "uart_protocol.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* API                                                                */
/* ================================================================== */

/** 初始化 USART1 (PA9/PA10, 115200, DMA+IDLE接收) */
void UART_Init(void);

/**
 * @brief 发送一帧协议数据
 * @param cmd     命令字
 * @param data    数据指针
 * @param len     数据长度
 * @return 0=成功
 */
int UART_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);

/**
 * @brief 发送车况上报帧 (简化API)
 */
int UART_SendVehicleReport(void);

/**
 * @brief 发送心跳帧
 */
int UART_SendHeartbeat(void);

/**
 * @brief 检查是否有新收到的远程控制指令
 * @param cmd  输出: 收到的远程指令
 * @return 0=无新指令, 1=有新指令
 */
int UART_GetRemoteCmd(RemoteCmd_t *cmd);

/**
 * @brief 周期处理 (接收解析 + 超时检测)
 */
void UART_Process(void);

/** UART 电机数据 (来自ESP32) */
uint16_t UART_GetMotorRPM(void);
uint8_t  UART_GetMotorTemp(void);
uint8_t  UART_GetMotorFault(void);
uint8_t  UART_IsMotorUpdated(void);

/** 发送驾驶模式到ESP32 */
void UART_SendDriveMode(uint8_t mode);

/** 获取 USART1 句柄 (供 HAL 中断) */
UART_HandleTypeDef *UART_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_COMM_H */
