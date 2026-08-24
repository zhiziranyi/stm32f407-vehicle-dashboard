/**
 * can_parser.h — CAN 报文解析器 (cheji407)
 */

#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include "can_protocol.h"
#include "can_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* API                                                                */
/* ================================================================== */

/** 解析一帧 CAN 报文 */
void CAN_ParseMessage(const CAN_Msg_t *msg);

/** 获取最新数据 (返回 NULL 表示尚无数据) */
const BMS_Status_t      *CAN_GetBMSStatus(void);
const Motor_Status_t    *CAN_GetMotorStatus(void);
const Vehicle_Status_t  *CAN_GetVehicleStatus(void);
const BCM_Status_t      *CAN_GetBCMStatus(void);
const Fault_t           *CAN_GetLastFault(void);

/** 更新状态标志 */
uint8_t CAN_IsBMSUpdated(void);
uint8_t CAN_IsMotorUpdated(void);
uint8_t CAN_IsVehicleUpdated(void);
uint8_t CAN_IsBCMUpdated(void);

void CAN_ClearBMSFlag(void);
void CAN_ClearMotorFlag(void);
void CAN_ClearVehicleFlag(void);
void CAN_ClearBCMFlag(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_PARSER_H */
