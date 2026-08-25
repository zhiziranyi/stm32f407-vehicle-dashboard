# STM32F407 车辆仪表与 CAN 网关交接文档

更新时间：2026-08-25  
目标硬件：STM32F407ZGT6 + ST7789 240×240 TFT + SD 卡 + 双路 CAN

## 1. 架构

```text
CAN1 powertrain (500 kbps) ─┐
                            ├─> CAN parser -> vehicle FSM -> LVGL dashboard / logger
CAN2 body (250 kbps) ───────┘
Joystick -> LVGL key input ─> UI navigation
SDIO -> FatFs -> media / log storage
```

系统启动 168 MHz 时钟，初始化 TFT、SD、LVGL、双 CAN、车辆状态机和数据记录器；主循环处理 CAN FIFO、UI 更新、输入事件和状态机控制帧。

## 2. 接线

| STM32F407 | 外设 | 说明 |
|---|---|---|
| PD0 / PD1 | CAN1 RX / TX | 动力 CAN，500 kbps，通过 CAN 收发器接入 |
| PB12 / PB13 | CAN2 RX / TX | 车身 CAN，250 kbps，通过 CAN 收发器接入 |
| PD5 / PD6 | UART TX / RX | 串口通信接口 |
| PB3 / PB5 / PD11 / PD12 / PD13 | TFT SCK / MOSI / CS / DC / BL | ST7789 SPI 显示 |
| PC8–PC12、PD2 | SDIO D0–D3、CK、CMD | SD 卡 4-bit 模式 |
| PA4 / PA5 / PA6 | 摇杆 VRx / VRy / SW | ADC1 输入和按键输入 |

双 CAN 网络须各自使用收发器、正确共地并在**每条物理总线的两端**提供 120 Ω 终端；不要把 CAN TX/RX 直接接到车载 CANH/CANL。

## 3. 协议和功能边界

- CAN1 使用 Filter Bank 0，CAN2 使用 Filter Bank 14；两路报文进入统一解析和 UI 刷新链路。
- UI 仅展示/模拟车况和控制逻辑；接入真实车辆前需要独立完成 DBC、故障策略、网络安全、EMC 和功能安全评审。
- SD 挂载失败时系统以错误显示提示但继续基础运行；日志和媒体功能依赖可用 SD 卡。

## 4. 构建与烧录

```powershell
pio run -e black_f407zg
pio run -e black_f407zg -t upload --upload-port COM3
```

串口下载使用 STM32 ROM Bootloader；烧录时按开发板要求设置 BOOT0/复位。不要同时打开串口监视器和上传任务。
