/**
 * can_parser.c — CAN 报文解析器实现
 */

#include "can_parser.h"
#include <string.h>

/* ================================================================== */
/* 解析结果缓存                                                       */
/* ================================================================== */

static BMS_Status_t       g_bms_status;
static Motor_Status_t     g_motor_status;
static Vehicle_Status_t   g_vehicle_status;
static BCM_Status_t       g_bcm_status;
static Fault_t            g_last_fault;

static uint8_t g_bms_updated     = 0;
static uint8_t g_motor_updated   = 0;
static uint8_t g_vehicle_updated = 0;
static uint8_t g_bcm_updated     = 0;
static uint8_t g_fault_pending   = 0;

/* ================================================================== */
/* 解析入口                                                           */
/* ================================================================== */

void CAN_ParseMessage(const CAN_Msg_t *msg)
{
    if (msg == NULL || msg->dlc == 0) return;

    switch (msg->id) {

    case CAN_ID_BMS_STATUS:
        if (msg->dlc >= 8) {
            g_bms_status.total_voltage = (uint16_t)(msg->data[0] << 8) | msg->data[1];
            g_bms_status.total_current = (int16_t)((msg->data[2] << 8) | msg->data[3]);
            g_bms_status.soc           = msg->data[4];
            g_bms_status.temp_max      = msg->data[5];
            g_bms_status.temp_min      = msg->data[6];
            g_bms_status.fault_code    = msg->data[7];
            g_bms_updated = 1;
        }
        break;

    case CAN_ID_MOTOR_STATUS:
        if (msg->dlc >= 4) {
            g_motor_status.speed           = (uint16_t)(msg->data[0] << 8) | msg->data[1];
            g_motor_status.temp_controller = msg->data[2];
            g_motor_status.fault_status    = msg->data[3];
            g_motor_updated = 1;
        }
        break;

    case CAN_ID_VEHICLE_STATUS:
        if (msg->dlc >= 4) {
            g_vehicle_status.vehicle_state = msg->data[0];
            g_vehicle_status.drive_mode    = msg->data[1];
            g_vehicle_status.speed         = (uint16_t)(msg->data[2] << 8) | msg->data[3];
            g_vehicle_updated = 1;
        }
        break;

    case CAN_ID_BCM_STATUS:
        if (msg->dlc >= 3) {
            g_bcm_status.light_status  = msg->data[0];
            g_bcm_status.lock_status   = msg->data[1];
            g_bcm_status.window_status = msg->data[2];
            g_bcm_updated = 1;
        }
        break;

    case CAN_ID_FAULT:
        if (msg->dlc >= 3) {
            g_last_fault.node_id    = msg->data[0];
            g_last_fault.fault_code = (uint16_t)(msg->data[1] << 8) | msg->data[2];
            memcpy(g_last_fault.fault_data, &msg->data[3],
                   (msg->dlc - 3) > 5 ? 5 : (msg->dlc - 3));
            g_fault_pending = 1;
        }
        break;

    default:
        break;
    }
}

/* ================================================================== */
/* 数据获取                                                           */
/* ================================================================== */

const BMS_Status_t *CAN_GetBMSStatus(void)       { return g_bms_updated ? &g_bms_status : NULL; }
const Motor_Status_t *CAN_GetMotorStatus(void)    { return g_motor_updated ? &g_motor_status : NULL; }
const Vehicle_Status_t *CAN_GetVehicleStatus(void){ return g_vehicle_updated ? &g_vehicle_status : NULL; }
const BCM_Status_t *CAN_GetBCMStatus(void)        { return g_bcm_updated ? &g_bcm_status : NULL; }
const Fault_t *CAN_GetLastFault(void) {
    if (g_fault_pending) { g_fault_pending = 0; return &g_last_fault; }
    return NULL;
}

uint8_t CAN_IsBMSUpdated(void)    { return g_bms_updated; }
uint8_t CAN_IsMotorUpdated(void)  { return g_motor_updated; }
uint8_t CAN_IsVehicleUpdated(void){ return g_vehicle_updated; }
uint8_t CAN_IsBCMUpdated(void)    { return g_bcm_updated; }

void CAN_ClearBMSFlag(void)    { g_bms_updated = 0; }
void CAN_ClearMotorFlag(void)  { g_motor_updated = 0; }
void CAN_ClearVehicleFlag(void){ g_vehicle_updated = 0; }
void CAN_ClearBCMFlag(void)    { g_bcm_updated = 0; }
