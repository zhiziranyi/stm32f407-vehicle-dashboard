# STM32F407 Vehicle Dashboard and CAN Gateway

Vehicle-instrument firmware for STM32F407ZGT6. It combines an LVGL TFT
dashboard, CAN gateway, SD-card data handling, joystick navigation and vehicle
state presentation. It works with the companion `stm32f103-vehicle-ecu`
project over CAN.

## Build

```powershell
platformio run -e black_f407zg
```

Serial download is configured on `COM3` at 115200 baud. See
`PROJECT_SUMMARY.md` and `FOR_NEW_AI.md` for the architecture and current
development state.
