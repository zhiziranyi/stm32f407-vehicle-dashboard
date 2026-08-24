/**
 * uart_comm.c — F407 USART2 串口通信
 * PD5(TX)/PD6(RX), 115200, 8N1
 */
#include "uart_comm.h"
#include "can_driver.h"
#include "can_parser.h"
#include "vehicle_fsm.h"
#include <string.h>

static UART_HandleTypeDef g_huart2;

/* 接收缓冲 */
#define RB_SIZE 128
static volatile uint8_t g_rb[RB_SIZE];
static volatile uint8_t g_rb_wr = 0;
static volatile uint8_t g_rb_rd = 0;

/* 帧解析缓冲 */
static uint8_t  g_fbuf[32];
static uint8_t  g_fpos = 0;

/* 电机数据 */
static volatile uint16_t g_motor_rpm  = 0;
static volatile uint8_t  g_motor_temp = 0;
static volatile uint8_t  g_motor_fault= 0;
static volatile uint8_t  g_motor_upd  = 0;

/* 远程指令 */
static RemoteCmd_t g_rcmd;
static volatile uint8_t g_rcmd_rdy = 0;

/* ---- HAL MSP ---- */
void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    if (h->Instance != USART2) return;
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef s={0};
    s.Mode=GPIO_MODE_AF_PP; s.Pull=GPIO_PULLUP; s.Speed=GPIO_SPEED_FREQ_HIGH;
    s.Alternate=GPIO_AF7_USART2;
    s.Pin=GPIO_PIN_5;  HAL_GPIO_Init(GPIOD,&s);  /* PD5=TX */
    s.Pin=GPIO_PIN_6;  HAL_GPIO_Init(GPIOD,&s);  /* PD6=RX */
    HAL_NVIC_SetPriority(USART2_IRQn,5,0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void UART_Init(void)
{
    g_huart2.Instance=USART2;
    g_huart2.Init.BaudRate=115200;
    g_huart2.Init.WordLength=UART_WORDLENGTH_8B;
    g_huart2.Init.StopBits=UART_STOPBITS_1;
    g_huart2.Init.Parity=UART_PARITY_NONE;
    g_huart2.Init.Mode=UART_MODE_TX_RX;
    g_huart2.Init.HwFlowCtl=UART_HWCONTROL_NONE;
    g_huart2.Init.OverSampling=UART_OVERSAMPLING_16;
    HAL_UART_Init(&g_huart2);
    /* 不用中断, 主循环轮询 (避免悬空引脚噪声卡死CPU) */
}

UART_HandleTypeDef *UART_GetHandle(void) { return &g_huart2; }

/* ---- 发送 ---- */
int UART_SendFrame(uint8_t cmd, const uint8_t *d, uint8_t n)
{
    uint8_t buf[FRAME_MAX_SIZE],i,chk;
    if(n>FRAME_MAX_DATA)return -1;
    buf[0]=FRAME_HEADER1; buf[1]=FRAME_HEADER2;
    buf[2]=cmd; buf[3]=n;
    chk=cmd+n;
    for(i=0;i<n;i++){buf[4+i]=d[i];chk+=d[i];}
    buf[4+n]=chk;
    HAL_UART_Transmit(&g_huart2,buf,5+n,100);
    return 0;
}

int UART_SendVehicleReport(void)
{
    VehicleReport_t r={0};
    const BMS_Status_t *b=CAN_GetBMSStatus();
    const Motor_Status_t *m=CAN_GetMotorStatus();
    r.speed=FSM_GetVehicleSpeed();
    r.soc=b?b->soc:0;
    r.odometer=0;
    r.fault_code=b?b->fault_code:0;
    r.drive_mode=(uint8_t)FSM_GetDriveMode();
    r.vehicle_state=(uint8_t)FSM_GetState();
    r.motor_rpm=m?m->speed:0;
    r.ac_status=(FSM_GetACState()<<7)|24;
    r.regen_level=FSM_GetRegenLevel();
    r.bms_voltage=b?b->total_voltage:0;
    r.bms_current=b?b->total_current:0;
    return UART_SendFrame(UART_CMD_VEHICLE_REPORT,(const uint8_t*)&r,sizeof(r));
}

int UART_SendHeartbeat(void)
{
    Heartbeat_t h={.state=(uint8_t)FSM_GetState()};
    return UART_SendFrame(UART_CMD_HEARTBEAT,(const uint8_t*)&h,sizeof(h));
}

/* ---- 帧解析 ---- */
static void parse(uint8_t *buf, uint8_t len)
{
    for (int i=0; i<=(int)len-5; i++) {
        if (buf[i]!=0xAA || buf[i+1]!=0x55) continue;
        uint8_t cmd=buf[i+2], dlen=buf[i+3];
        if (i+5+dlen > len) break;
        uint8_t chk=cmd+dlen;
        for (int j=0;j<dlen;j++) chk+=buf[i+4+j];
        if (chk != buf[i+4+dlen]) continue;

        /* CMD 0xF2: 电机数据 */
        if (cmd==0xF2 && dlen>=4) {
            g_motor_rpm = ((uint16_t)buf[i+4]<<8)|buf[i+5];
            g_motor_temp = buf[i+6];
            g_motor_fault = buf[i+7];
            g_motor_upd = 1;
        }
        /* CMD 0x02: 远程指令 */
        if (cmd==0x02 && dlen>=4) {
            memcpy(&g_rcmd, &buf[i+4], 4);
            g_rcmd_rdy = 1;
        }
        i += 4 + dlen;  /* 跳过已解析帧 */
    }
}

/* ---- 中断: 每收到1字节 ---- */
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&g_huart2, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(g_huart2.Instance->DR);
        g_fbuf[g_fpos++] = b;
        if (g_fpos >= 32) {
            parse((uint8_t*)g_fbuf, g_fpos);
            g_fpos = 0;
        }
        /* 检测帧尾停顿: 如果收到0xAA且buf中有完整帧, 尝试解析 */
        if (g_fpos >= 9) {
            parse((uint8_t*)g_fbuf, g_fpos);
            if (g_fpos > 16) g_fpos = 0;  /* 超长重置 */
        }
    }
    HAL_UART_IRQHandler(&g_huart2);
}

/* ---- 远程指令 ---- */
int UART_GetRemoteCmd(RemoteCmd_t *c) {
    if(!g_rcmd_rdy)return 0;
    if(c)memcpy(c,&g_rcmd,sizeof(RemoteCmd_t));
    g_rcmd_rdy=0; return 1;
}

/* ---- 电机数据 ---- */
uint16_t UART_GetMotorRPM(void)   { return g_motor_rpm; }
uint8_t  UART_GetMotorTemp(void)  { return g_motor_temp; }
uint8_t  UART_GetMotorFault(void) { return g_motor_fault; }
uint8_t  UART_IsMotorUpdated(void){ uint8_t u=g_motor_upd; g_motor_upd=0; return u; }

void UART_SendDriveMode(uint8_t m) {
    uint8_t d[4]={REMOTE_CMD_DRIVE_MODE,m,0,0};
    UART_SendFrame(UART_CMD_REMOTE_CONTROL,d,4);
}

/* 主循环调用: 轮询收字节 */
void UART_Process(void)
{
    while (__HAL_UART_GET_FLAG(&g_huart2, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(g_huart2.Instance->DR);
        g_fbuf[g_fpos++] = b;
        if (g_fpos >= 32) { parse((uint8_t*)g_fbuf, g_fpos); g_fpos = 0; }
        if (g_fpos >= 9) { parse((uint8_t*)g_fbuf, g_fpos); }
    }
}
