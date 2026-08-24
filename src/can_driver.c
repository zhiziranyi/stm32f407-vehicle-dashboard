/**
 * can_driver.c — STM32F407 双路 CAN 总线驱动实现
 *
 * CAN1: PD0(RX)/PD1(TX), AF9, 500kbps — 动力CAN网段
 * CAN2: PB12(RX)/PB13(TX), AF9, 250kbps — 车身CAN网段
 *
 * 时钟基准: APB1 = 42MHz (HCLK/4 = 168/4)
 *
 * 波特率计算 (Baud = CANCLK / (Prescaler * (1 + TS1 + TS2))):
 *   500k: Prescaler=7,  TS1=8, TS2=3 → 42M / (7*12) = 500k
 *   250k: Prescaler=14, TS1=8, TS2=3 → 42M / (14*12) = 250k
 */

#include "can_driver.h"
#include <string.h>

/* ================================================================== */
/* 静态变量                                                           */
/* ================================================================== */

static CAN_HandleTypeDef  g_hcan1;
static CAN_HandleTypeDef  g_hcan2;
static CAN_RxCallback_t   g_rx_cb[CAN_BUS_MAX] = {NULL, NULL};

/* ================================================================== */
/* CAN MSP 初始化 — GPIO 引脚配置                                      */
/* ================================================================== */

/**
 * HAL_CAN_MspInit — HAL 层回调，由 HAL_CAN_Init 调用
 * 根据 hcan->Instance 区分 CAN1 (PD0/PD1) 和 CAN2 (PB12/PB13)
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hcan->Instance == CAN1) {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;

        GPIO_InitStruct.Pin = GPIO_PIN_0;  /* CAN1_RX */
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_1;  /* CAN1_TX */
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
        /* SCE不启用 — 电机PWM噪声会导致错误中断泛滥 */

    } else if (hcan->Instance == CAN2) {
        __HAL_RCC_CAN2_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_CAN2;

        GPIO_InitStruct.Pin = GPIO_PIN_12;  /* CAN2_RX */
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_13;  /* CAN2_TX */
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(CAN2_RX0_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(CAN2_RX0_IRQn);
        /* CAN2 SCE同样不启用 */
    }
}

/* ================================================================== */
/* CAN 过滤器配置 — 所有报文全部接收                                    */
/* ================================================================== */

static void CAN_ConfigFilter(CAN_HandleTypeDef *hcan)
{
    CAN_FilterTypeDef sFilterConfig = {0};

    if (hcan->Instance == CAN1) {
        /* CAN1 使用 Filter Bank 0 */
        sFilterConfig.FilterBank           = 0;
    } else {
        /* CAN2 使用 Filter Bank 14 */
        sFilterConfig.FilterBank           = 14;
    }

    sFilterConfig.FilterMode               = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale              = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh             = 0x0000;
    sFilterConfig.FilterIdLow              = 0x0000;
    sFilterConfig.FilterMaskIdHigh         = 0x0000;
    sFilterConfig.FilterMaskIdLow          = 0x0000;
    sFilterConfig.FilterFIFOAssignment     = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation         = ENABLE;
    sFilterConfig.SlaveStartFilterBank     = 14;  /* CAN1 用 0-13, CAN2 用 14-27 */

    HAL_CAN_ConfigFilter(hcan, &sFilterConfig);
}

/* ================================================================== */
/* CAN 外设初始化                                                      */
/* ================================================================== */

/**
 * @brief 通用 CAN 初始化
 * @param hcan        CAN 句柄
 * @param prescaler   预分频器
 * @param ts1         时间段1
 * @param ts2         时间段2
 */
static void CAN_InitCommon(CAN_HandleTypeDef *hcan,
                           uint32_t prescaler,
                           uint32_t ts1,
                           uint32_t ts2)
{
    hcan->Init.Prescaler         = prescaler;
    hcan->Init.Mode              = CAN_MODE_NORMAL;
    hcan->Init.SyncJumpWidth     = CAN_SJW_1TQ;
    hcan->Init.TimeSeg1          = ts1;
    hcan->Init.TimeSeg2          = ts2;
    hcan->Init.TimeTriggeredMode = DISABLE;
    hcan->Init.AutoBusOff        = ENABLE;
    hcan->Init.AutoWakeUp        = DISABLE;
    hcan->Init.AutoRetransmission = ENABLE;
    hcan->Init.ReceiveFifoLocked  = DISABLE;
    hcan->Init.TransmitFifoPriority = DISABLE;

    HAL_CAN_Init(hcan);
    CAN_ConfigFilter(hcan);
    HAL_CAN_Start(hcan);
}

void CAN1_Init(void)
{
    g_hcan1.Instance = CAN1;
    /* 500kbps: 42MHz / (7 * 12) = 500k */
    CAN_InitCommon(&g_hcan1, 7, CAN_BS1_8TQ, CAN_BS2_3TQ);
}

void CAN2_Init(void)
{
    g_hcan2.Instance = CAN2;
    /* 250kbps: 42MHz / (14 * 12) = 250k */
    CAN_InitCommon(&g_hcan2, 14, CAN_BS1_8TQ, CAN_BS2_3TQ);
}

/* ================================================================== */
/* 发送消息                                                           */
/* ================================================================== */

extern volatile uint32_t g_can1_tx_cnt;

int CAN_SendMsg(CAN_Bus_t bus, const CAN_Msg_t *msg)
{
    CAN_HandleTypeDef *hcan;
    CAN_TxHeaderTypeDef txHeader;
    uint32_t txMailbox;

    if (bus >= CAN_BUS_MAX || msg == NULL || msg->dlc > 8)
        return -1;

    hcan = (bus == CAN_BUS_1) ? &g_hcan1 : &g_hcan2;
    if (bus == CAN_BUS_1) g_can1_tx_cnt++;

    txHeader.StdId = msg->id;
    txHeader.ExtId = msg->id;
    txHeader.IDE   = msg->ide ? CAN_ID_EXT : CAN_ID_STD;
    txHeader.RTR   = msg->rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    txHeader.DLC   = msg->dlc;
    txHeader.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_AddTxMessage(hcan, &txHeader, (uint8_t *)msg->data, &txMailbox) != HAL_OK)
        return -2;

    return 0;
}

int CAN_SendStdMsg(CAN_Bus_t bus, uint32_t id, const uint8_t *data, uint8_t len)
{
    CAN_Msg_t msg;
    msg.id  = id;
    msg.dlc = len;
    msg.ide = 0;
    msg.rtr = 0;
    if (data && len) memcpy(msg.data, data, len);
    return CAN_SendMsg(bus, &msg);
}

/* ================================================================== */
/* 接收回调                                                           */
/* ================================================================== */

void CAN_RegisterRxCallback(CAN_Bus_t bus, CAN_RxCallback_t cb)
{
    if (bus < CAN_BUS_MAX)
        g_rx_cb[bus] = cb;
}

/* ================================================================== */
/* 错误查询                                                           */
/* ================================================================== */

uint32_t CAN_GetError(CAN_Bus_t bus)
{
    CAN_HandleTypeDef *hcan = (bus == CAN_BUS_1) ? &g_hcan1 : &g_hcan2;
    if (bus >= CAN_BUS_MAX) return 0;
    return HAL_CAN_GetError(hcan);
}

CAN_HandleTypeDef *CAN_GetHandle(CAN_Bus_t bus)
{
    if (bus >= CAN_BUS_MAX) return NULL;
    return (bus == CAN_BUS_1) ? &g_hcan1 : &g_hcan2;
}

/* ================================================================== */
/* CAN 接收中断处理 (IRQ Handler)                                      */
/* ================================================================== */

/**
 * @brief 通用 CAN RX FIFO0 中断回调
 *
 * HAL 库在收到 CAN 消息后调用此函数，我们取出消息并转发给用户回调
 */
static void CAN_RxCallback(CAN_HandleTypeDef *hcan, CAN_Bus_t bus)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
        return;

    if (g_rx_cb[bus] == NULL)
        return;

    CAN_Msg_t msg;
    msg.id  = rxHeader.IDE == CAN_ID_STD ? rxHeader.StdId : rxHeader.ExtId;
    msg.dlc = rxHeader.DLC;
    msg.ide = (rxHeader.IDE == CAN_ID_EXT) ? 1 : 0;
    msg.rtr = (rxHeader.RTR == CAN_RTR_REMOTE) ? 1 : 0;
    memcpy(msg.data, rxData, msg.dlc > 8 ? 8 : msg.dlc);

    g_rx_cb[bus](&msg);
}

/**
 * HAL CAN RX FIFO0 回调 — 由 HAL_CAN_IRQHandler 调用
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
        CAN_RxCallback(hcan, CAN_BUS_1);
    else if (hcan->Instance == CAN2)
        CAN_RxCallback(hcan, CAN_BUS_2);
}

/* ================================================================== */
/* IRQ 中断服务函数                                                    */
/* ================================================================== */

void CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&g_hcan1);
}

void CAN1_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&g_hcan1);
}

void CAN2_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&g_hcan2);
}

void CAN2_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&g_hcan2);
}
