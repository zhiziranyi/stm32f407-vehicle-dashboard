# 基于STM32的车载信息与车身网关系统 — 项目总结

## 项目概述

本项目构建了一套完整的电动车车载电子系统，基于 **双MCU异构架构**（STM32F407ZGT6 + STM32F103C8T6），通过 **CAN总线** 实现跨芯片通信，真实模拟了车辆电子电气架构中的核心功能。系统涵盖车载仪表显示、CAN网络管理、整车状态机、电机FOC控制、影音娱乐、数据记录等多个子系统。

**开发周期**：约2周 | **代码量**：约3500行 C/C++ | **平台**：PlatformIO + STM32Cube HAL

---

## 硬件清单

| 硬件 | 型号 | 角色 |
|---|---|---|
| 主控板 | STM32F407ZGT6 鹿小班版 | 车载主机：仪表显示 + CAN网关 + 影音娱乐 |
| ECU板 | STM32F103C8T6 常见版 | 动力ECU：BMS模拟 + 电机控制器 |
| 屏幕 | 1.54寸 TFT (ST7789V) | SPI接口, 240×240, RGB565 |
| 存储 | MicroSD卡 | SDIO 4-bit, FatFs文件系统 |
| CAN收发器 | TJA1050 ×2 | 动力CAN网段 500kbps |
| 电机驱动 | SimpleFOC Mini | DRV8313, 3路PWM |
| 电机 | 2804无刷电机 | 7对极, 100KV, 12V |
| 编码器 | AS5600 | 12-bit磁编码器, I2C接口 |
| 输入 | 双轴按键摇杆 | ADC双轴 + 数字按键 |

---

## 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│                    STM32F407ZGT6 车载主机                     │
│                                                              │
│  ┌─────────┐ ┌──────────┐ ┌────────┐ ┌─────────┐ ┌───────┐ │
│  │ DASH    │ │ MOTOR    │ │ CTRL   │ │ BODY    │ │ MEDIA │ │
│  │ 车速/SOC│ │ RPM折线图│ │驾驶模式│ │灯光/门锁│ │图片/文本│ │
│  │ 点火按钮│ │ 温度显示 │ │空调开关│ │车窗状态│ │/视频   │ │
│  └─────────┘ └──────────┘ └────────┘ └─────────┘ └───────┘ │
│                                                              │
│  LVGL 8.2  |  FatFs R0.13  |  六级状态机  |  摇杆导航       │
│  暗色主题  |  SDIO+DMA     |  SLEEP→DRIVE |  LEFT/RIGHT切页 │
└──────────────────┬───────────────────────────────────────────┘
                   │ CAN 500kbps (动力网段)
┌──────────────────┴───────────────────────────────────────────┐
│                  STM32F103C8T6 动力ECU                        │
│                                                              │
│  ┌──────────────────┐  ┌────────────────────────────────┐   │
│  │ 虚拟BMS节点       │  │ 电机控制器 (SVPWM开环FOC)       │   │
│  │ SOC 80%→15%递减  │  │ 2804无刷电机 · AS5600编码器    │   │
│  │ 370V / 15A / 75°C│  │ ECO 300 / COMFORT 600 / SPORT 1000│ │
│  │ CAN 0x100 每秒   │  │ CAN 0x200 每100ms              │   │
│  └──────────────────┘  └────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

---

## CAN通信矩阵 (动力网段 500kbps)

| 帧ID | 发送者 | 周期 | 字节 | 内容 |
|---|---|---|---|---|
| **0x100** | F103 BMS | 1000ms | 8B | 总电压(u16,0.1V) + 总电流(i16,0.1A) + SOC(u8) + 最高温(u8) + 最低温(u8) + 故障码(u8) |
| **0x200** | F103 MCU | 100ms | 4B | 电机转速(u16,rpm) + 控制器温度(u8) + 故障状态(u8) |
| **0x300** | F407 主控 | 100ms | 4B | 整车状态(u8) + 驾驶模式(u8,0/1/2) + 车速(u16,rpm) |

F103 接收 0x300 中的驾驶模式字段，根据 0=ECO / 1=COMFORT / 2=SPORT 切换电机目标转速（300/600/1000 RPM）。

---

## 软件架构

### STM32F407ZGT6 固件 (`cheji407/`)

```
cheji407/
├── platformio.ini              # PlatformIO, STM32Cube, 168MHz
├── src/
│   ├── main.c                  # 主程序: 时钟168MHz, TFT, SD, CAN, LVGL主循环
│   ├── tft_display.c           # ST7789V SPI驱动 (RGB565, 240x240)
│   ├── lvgl_ui.c               # LVGL GUI: 5页仪表 + 摇杆处理 (~900行)
│   ├── lv_port_disp.c          # LVGL显示接口 (flush回调→TFT_PushColors)
│   ├── lv_port_fs.c            # LVGL FatFs文件系统桥 (open/read/write/seek)
│   ├── joystick.c              # 双轴摇杆 (ADC CH4/CH5 + GPIO按键消抖)
│   ├── can_driver.c            # 双路CAN (CAN1 500k / CAN2 250k)
│   ├── can_parser.c            # CAN报文解析 (0x100/0x200/0x300/0x400/0x500)
│   ├── vehicle_fsm.c           # 六级状态机 (SLEEP→ACC→IGN→DRIVE→CHARGE→FAULT)
│   ├── data_logger.c           # SD卡CSV记录 + 事件冻结
│   ├── sdio_sd.c / sd_diskio.c # SDIO底层 + FatFs磁盘IO层
│   ├── cn_font.c               # 中文字库 (407个16x16 bitmap)
│   └── jpeg_display.c          # tjpgd JPEG解码 (备用)
├── include/                    # 头文件
├── lib/
│   ├── lvgl/                   # LVGL 8.2.0
│   ├── fatfs/                  # FatFs R0.13c
│   └── tjpgd/                  # Tiny JPEG Decoder
└── FOR_NEW_AI.md               # 项目接手指南
```

**Flash占用**：22.6% (237KB / 1MB) | **RAM占用**：81.7% (107KB / 128KB)

### STM32F103C8T6 固件 (`cheji103/`)

```
cheji103/
├── platformio.ini              # STM32Cube, 72MHz, ST-Link
├── src/
│   ├── main.c                  # 主循环: BMS_CAN + Motor_FOC + 驾驶模式接收
│   ├── can_driver.c            # CAN驱动 (PB8/PB9, AFIO重映射, 动态波特率)
│   └── motor_ctrl.c            # SVPWM开环FOC + AS5600 I2C测速
└── include/
    └── can_driver.h / motor_ctrl.h
```

**Flash占用**：22.1% (14.5KB / 64KB) | **RAM占用**：1.4% (284B / 20KB)

---

## 核心功能详情

### 1. 仪表盘显示 (LVGL GUI)

5个Tab页面，摇杆 LEFT/RIGHT 切换：

| 页面 | 内容 | 操作 |
|---|---|---|
| **DASH** | 中央大数字车速 + RPM弧 + SOC弧 + 点火按钮 | ENTER 点火 |
| **MOTOR** | 大字RPM + 温度 + 60点折线图 | 纯显示 |
| **CTRL** | ECO/COMFORT/SPORT按钮 + A/C开关 + 回收滑块 | UP/DOWN切模式, ENTER开关A/C |
| **BODY** | 车灯/门锁/车窗状态卡片 | 纯显示 |
| **MEDIA** | 文件浏览器 (IMG/TXT/VID 3子页) | UP/DOWN切子页, LEFT/RIGHT选文件, ENTER打开 |

### 2. 摇杆操作逻辑

```
左右 → 切页面 (最高优先级, 任何模式都可用)
上下 → DASH页: 无操作  |  CTRL页: 切换驾驶模式  |  MEDIA页: 切IMG/TXT/VID
ENTER → DASH: 点火  |  CTRL: 开关A/C  |  MEDIA: 打开文件
ESC → 关闭弹窗 / 退出MEDIA查看模式
```

摇杆死区500（ADC范围0-4095，中心2048），边沿触发+长按500ms重复。

### 3. 电机FOC控制

采用**开环SVPWM**驱动2804无刷电机，原理与SimpleFOC的 `velocity_openloop` 一致：

1. **对齐阶段**（1.5秒）：施加3V直流到A相，转子归位
2. **开环旋转**：电角度 = Σ(target_speed × dt × 极对数)，磁场匀速旋转
3. **SVPWM输出**：Clarke反变换 + 三次谐波注入，电压利用率提升15%
4. **速度反馈**：AS5600 I2C读取角度 → 差分计算实际转速（仅用于显示）

PWM频率25kHz（TIM3, PA6/PA7/PB0），I2C 400kHz（PB6/PB7）。

### 4. 影音娱乐系统

基于FatFs + SDIO 4-bit DMA，直接从SD卡读取文件：

- **图片查看**：`.raw`格式（240×240 RGB565原始像素，115KB/张），PC端Python预转换JPG→raw
- **文本阅读**：`.txt` UTF-8文件，TFT_DrawUTF8渲染中文，支持翻页
- **视频播放**：`.vid`格式（帧数+fps头 + 原始帧数据），TFT_StreamPush高速推屏

查看时暂停LVGL渲染（`lvgl_ui_is_viewing()` → 跳过 `lv_timer_handler()`），避免覆盖直接TFT输出。

### 5. 数据记录

SD卡CSV格式循环记录：`timestamp_ms, speed, soc, voltage, current, rpm, temp, state, fault`
- 每100ms写入一行
- 故障事件触发时冻结前后各10秒（100条）数据，存为独立事件文件

---

## CAN通信关键Bug记录

| Bug | 现象 | 根因 | 修复 |
|---|---|---|---|
| F103↔F407 CAN不通 | 数据全0 | F103 HSI时钟→CAN波特率不匹配 | HSE优先+动态波特率计算 |
| F407屏幕黑屏 | 完全黑屏 | 电机PWM噪声→CAN SCE中断泛滥 | 关闭SCE中断 |
| CAN引脚未映射 | 103 CAN不工作 | `AFIO_MAPR_CAN_REMAP_REMAP1`值错误 | 改用`(2U<<13)`=0x4000 |
| 电机不转 | 蜂鸣不动 | I2C在TIM2 ISR中阻塞SysTick | FOC移至主循环, TIM2仅设标志 |
| RPM显示6万+ | 数值异常 | dt=0.001时`>`过滤掉等于0.001的值 | 改`>=`条件 |
| MEDIA页无法切回 | LEFT被拦截 | `if(pg!=4)`保护了全局LEFT/RIGHT | 边界检测+首个文件LEFT切页 |

---

## 资源占用

| 板子 | Flash使用 | RAM使用 | 框架 |
|---|---|---|---|
| STM32F407ZGT6 | 237KB / 1MB (22.6%) | 107KB / 128KB (81.7%) | STM32Cube HAL |
| STM32F103C8T6 | 14.5KB / 64KB (22.1%) | 284B / 20KB (1.4%) | STM32Cube HAL |

---

## 编译与烧录

```bash
# 编译
cd cheji407 && pio run     # F407仪表
cd cheji103 && pio run     # F103 ECU

# 烧录
# F407: 进bootloader (BOOT0+RESET), 串口COM3 115200
# F103: ST-Link一键烧录
```

---

## 技术亮点（简历可用）

1. **嵌入式GUI开发**：240×240资源受限屏上运行LVGL 8.2，5页暗色主题仪表盘 + 折线图 + 文件浏览器
2. **CAN总线工程实践**：通信矩阵设计、双节点500kbps、电机PWM噪声下的EMC抗干扰、波特率动态适配
3. **电机FOC控制**：开环SVPWM磁场定向、AS5600编码器测速反馈、三档驾驶模式（300/600/1000RPM）
4. **整车电子电气架构**：六级车辆状态机建模、CAN驾驶模式远程控制、故障弹窗与数据冻结
5. **双MCU异构系统**：STM32F4 + STM32F1跨芯片CAN通信、资源差异化分配、独立供电共地设计
6. **文件系统应用**：FatFs + SDIO DMA、CSV循环记录、.raw图片/.vid视频解码、中文文本渲染
7. **硬件资源极致优化**：F407 Flash仅用23%、F103仅用22%，预留充足空间给后期扩展

---

## 后期扩展方向

1. **ESP32 + MCP2515 CAN监听**：WiFi/MQTT云上报、BLE手机虚拟仪表
2. **音频播放**：I2S DAC + WAV/MP3解码
3. **车身CAN网段**：接入真实车身模块（灯光/门锁/车窗）
4. **电流传感器**：INA240采样，闭环FOC扭矩控制
5. **OTA固件升级**：CAN/UART固件分包传输 + CRC校验 + 回滚
