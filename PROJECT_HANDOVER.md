# Project Handover

## Build and upload

- Build: `platformio run -e black_f407zg`
- Upload: STM32 ROM serial bootloader, `COM3`, 115200 baud

## Integration

This dashboard is paired with `stm32f103-vehicle-ecu` through CAN. See
`PROJECT_SUMMARY.md` before altering the cross-MCU protocol.
