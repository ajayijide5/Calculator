# STM32F103 Calculator

Bare-metal C calculator running on the STM32F103C8T6 (Blue Pill).
Direct HAL register access — no Arduino, no RTOS.

---

## Hardware

| Component              | Details                        |
|------------------------|--------------------------------|
| MCU                    | STM32F103C8T6 (Blue Pill)      |
| Display                | 16x2 HD44780 LCD, 4-bit mode   |
| Input                  | 4x4 Matrix Keypad              |
| Programmer             | ST-Link V2                     |
| Clock                  | 72 MHz (HSE 8 MHz × PLL ×9)    |

---

## Wiring

### LCD → STM32 (Port B)
| LCD Pin | STM32 Pin | Notes                        |
|---------|-----------|------------------------------|
| RS      | PB0       |                              |
| EN      | PB1       |                              |
| D4      | PB4       |                              |
| D5      | PB5       |                              |
| D6      | PB6       |                              |
| D7      | PB7       |                              |
| RW      | GND       | Always write mode            |
| V0      | Pot wiper | Contrast adjust              |
| VDD/VSS | 3.3V/GND  |                              |

### Keypad → STM32 (Port A)
| Keypad | STM32 Pin | Mode              |
|--------|-----------|-------------------|
| R1     | PA0       | Output, push-pull |
| R2     | PA1       | Output, push-pull |
| R3     | PA2       | Output, push-pull |
| R4     | PA3       | Output, push-pull |
| C1     | PA4       | Input, pull-up    |
| C2     | PA5       | Input, pull-up    |
| C3     | PA6       | Input, pull-up    |
| C4     | PA7       | Input, pull-up    |

### Keypad Layout
```
[ 1 ][ 2 ][ 3 ][ + ]
[ 4 ][ 5 ][ 6 ][ - ]
[ 7 ][ 8 ][ 9 ][ * ]
[ C ][ 0 ][ = ][ / ]
```

> **Note:** Add a `.` (decimal point) key by remapping one of the unused
> operator cells in `KP_MAP` and handling `key == '.'` — already
> stubbed in `Calc_HandleKey()`.

---

## Project Structure

```
stm32-calculator/
├── main.c                      ← Application logic
├── main.h                      ← Header / HAL include
├── Makefile                    ← Build, flash, debug targets
├── STM32F103C8Tx_FLASH.ld      ← Linker script (from CubeMX)
├── startup_stm32f103xb.s       ← Startup file (from CubeMX)
└── Drivers/
    ├── CMSIS/
    └── STM32F1xx_HAL_Driver/
```

> The `Drivers/` folder, linker script, and startup file come from
> STM32CubeMX or the STM32CubeF1 firmware package. They are not
> included here — copy them from your CubeMX-generated project.

---

## Toolchain & Dependencies

```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi openocd

# macOS (Homebrew)
brew install --cask gcc-arm-embedded
brew install open-ocd
```

---

## Build & Flash

```bash
# Build
make

# Flash via ST-Link V2 + OpenOCD
make flash

# Debug with GDB
make debug
```

---

## Architecture Notes

| Component        | Approach                                       |
|------------------|------------------------------------------------|
| Clock            | 72 MHz via HSE + PLL, configured in `SystemClock_Config()` |
| LCD              | Bit-banged HD44780, 4-bit mode, direct GPIO    |
| Keypad           | Row-scan with pull-up columns, debounced on key-release |
| Delay            | `HAL_Delay()` for ms, `__NOP()` loop for µs   |
| Number format    | `snprintf` + trailing-zero strip               |
| State machine    | `CalcState` struct, FSM in `Calc_HandleKey()`  |

---

## Version History

| Version | Description                                 |
|---------|---------------------------------------------|
| v1.0.0  | +  -  *  /  operators, decimal input, chained ops, error handling |

---

## Planned Features

- [ ] `%` modulo operator
- [ ] Square root via `math.h sqrtf()`
- [ ] Calculation history (ring buffer in SRAM)
- [ ] Memory store/recall (M+, MR, MC) via EEPROM emulation on flash
- [ ] I2C LCD backpack support (reduce pin count)
- [ ] Low-power sleep between keystrokes (`WFI`)
