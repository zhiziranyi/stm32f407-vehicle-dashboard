# STM32F407 Vehicle Dashboard & Dual-CAN Gateway

> An STM32F407ZGT6 vehicle-HMI project that turns CAN data into an interactive
> dashboard, while bridging a 500 kbit/s powertrain network and a 250 kbit/s
> body network. It is designed to be used with the companion
> [STM32F103 Vehicle ECU](https://github.com/zhiziranyi/stm32f103-vehicle-ecu).

## What this project demonstrates

- LVGL-based TFT instrument UI with periodic vehicle-state rendering.
- Dual CAN topology: CAN1 is the 500 kbit/s ECU/powertrain side; CAN2 is the
  250 kbit/s body-network side.
- SDIO/FatFs storage for configuration and data logging.
- Joystick-driven local UI navigation and a vehicle-state-machine layer.
- UART diagnostics, status visualization, and a modular driver/application
  split suitable for continued development.

## System architecture

```text
STM32F103 ECU simulation ── CAN1, 500 kbit/s ──> STM32F407 dashboard
                                                     │
                      TFT + LVGL <── UI/state layer ┤── SDIO + FatFs log storage
                      joystick  ────────────────────┤── UART diagnostics
                                                     │
                    body-network devices <── CAN2, 250 kbit/s ──>
```

The dashboard receives and presents vehicle information, applies the UI/state
logic locally, and keeps the two CAN roles explicit rather than treating the
project as a single-bus display demo.

## Hardware interface

All MCU logic is **3.3 V**. CAN transceivers and TFT/SD modules must share a
common ground with the F407 board.

| Function | STM32F407 pin(s) | Notes |
| --- | --- | --- |
| CAN1 | PD0 (RX), PD1 (TX) | ECU/powertrain network, 500 kbit/s |
| CAN2 | PB12 (RX), PB13 (TX) | Body network, 250 kbit/s |
| TFT SPI | PB3 (SCK), PB5 (MOSI) | Display data bus |
| TFT control | PD11 (CS), PD12 (DC), PD13 (BL) | Chip-select, data/command, backlight |
| SD card (SDIO) | PC8–PC12, PD2 | Storage and log path |
| Joystick | PA4, PA5, PA6 | Two axes plus press switch |
| UART diagnostics | PD5, PD6 | Application serial interface |

For CAN, connect TX/RX through an appropriate 3.3 V CAN transceiver; use a
single 120 Ω terminator at each physical end of the bus, not at every node.
Do not connect the STM32 pins directly to CANH/CANL.

## Build and flash

### Requirements

- PlatformIO
- `black_f407zg` board support
- 8 MHz external HSE as configured by the project

```powershell
cd C:\Users\13957\Documents\PlatformIO\Projects\cheji407
C:\Users\13957\.platformio\penv\Scripts\platformio.exe run -e black_f407zg
```

The project configuration uses serial upload on `COM3` at 115200 baud. Put the
board in STM32 ROM boot mode before flashing, then restore its normal boot
configuration and reset it. Adjust `upload_port` in `platformio.ini` if Windows
assigns a different COM port.

## Recommended bring-up order

1. Build the firmware and verify that the generated ELF/BIN appears in
   `.pio/build/black_f407zg/`.
2. Power the F407, TFT, SD card, and CAN transceiver from a stable supply with
   a shared ground.
3. Confirm the display and joystick before adding either CAN bus.
4. Connect CAN1 to the F103 ECU node at 500 kbit/s and observe decoded state
   changes on the dashboard.
5. Add the CAN2/body-network side at 250 kbit/s and verify that no bus uses
   the other's bitrate.
6. Insert a FAT-formatted SD card only after basic UI operation is stable, then
   exercise the logging/storage path.

## Verification evidence to collect

Record the following in a bench-test log or short demo video:

- LVGL dashboard reaches its normal screen after power-on.
- Joystick navigation changes the expected UI state.
- CAN1 receives ECU-side state changes at 500 kbit/s.
- CAN2 operates independently at 250 kbit/s.
- SD-card mount/logging behavior is visible and repeatable.
- A reset leaves the firmware in a recoverable, known UI state.

The detailed handover and test checklist are kept separate so that the public
README stays usable as an engineering entry point:

- [Hardware wiring, CAN topology and handover](docs/HANDOVER.md)
- [Bench/HIL verification checklist](docs/VALIDATION.md)

## Repository layout

```text
src/       Application, display, CAN, input and storage implementation
include/   Public headers and board/application configuration
lib/       Local libraries and drivers
docs/      Handover and repeatable validation procedures
test/      Test assets
```

`.pio/`, editor settings, and machine-local files are intentionally excluded
from version control.

## Resume description

**STM32F407 vehicle dashboard and CAN gateway** — Developed a 168 MHz STM32F4
vehicle HMI integrating LVGL/TFT rendering, dual-rate CAN communication,
SDIO/FatFs logging, joystick interaction, vehicle state-machine logic, and an
STM32F103-based ECU simulation node.
