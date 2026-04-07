# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**firmware** is an embedded C application for the **Raspberry Pi Pico 2 (RP2350)** that drives a Waveshare 1.14-inch TFT LCD (240×135, ST7789 controller) using the **LVGL v8.1.0** graphics library. It implements a multi-screen UI with DMA-accelerated rendering.

## Build Commands

This project uses CMake + Ninja. The build directory is `./build/`.

```bash
# Build the project
ninja -C build

# Flash via OpenOCD (CMSIS-DAP debugger)
openocd.exe -s <openocd-scripts> -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "adapter speed 5000; program \"build/RP2350-GEEK-LVGL.elf\" verify reset exit"

# Flash via picotool (UF2 bootloader mode)
picotool load uf2/RP2350-GEEK-LVGL.uf2 -fx
```

CMake must be configured before building (typically handled by the VSCode Pico extension). The `.vscode/tasks.json` contains pre-configured tasks for Compile, Run, and reset operations.

There are no unit tests — this is hardware-only embedded firmware.

## Architecture

### Layer Stack (top to bottom)

```
examples/src/LCD_1in14_LVGL_test.c   ← Application: UI screens, widgets, button handlers
examples/src/LVGL_example.c           ← LVGL integration: DMA flush callback, 5ms tick timer
lib/LCD/LCD_1in14.c                   ← LCD driver: ST7789 commands, window/pixel operations
lib/Config/DEV_Config.c               ← HAL: GPIO, SPI, I2C, DMA, PWM, ADC abstractions
Pico SDK / RP2350 hardware
```

### Entry Point

`main.c` → `LCD_1in14_test()` (in `examples/src/LCD_1in14_LVGL_test.c`) → init hardware → init LVGL → create widgets → infinite loop calling `lv_task_handler()` every 5ms.

### Key Modules

- **`lib/Config/DEV_Config.c/h`** — Hardware abstraction. GPIO, SPI1 (LCD data), I2C0 (sensors), DMA (SPI→LCD), PWM (backlight on GPIO25), ADC. All hardware pin assignments are defined here.
- **`lib/LCD/LCD_1in14.c/h`** — ST7789 LCD driver. Handles reset, initialization sequence, window region (`SetWindows`), and bulk pixel writes (`Display`, `DisplayWindows`).
- **`examples/src/LVGL_example.c`** — Bridges LVGL and hardware. `LVGL_Init()` registers the display driver. `disp_flush_cb()` triggers DMA transfers to the LCD; `dma_handler()` signals flush completion to LVGL. A repeating timer fires every 5ms to call `lv_tick_inc()`.
- **`examples/src/LCD_1in14_LVGL_test.c`** — Application logic. Currently creates 1 LVGL screen (`scr[0]`) with a centered image from `ImageData.c`. The `lvgl_data_struct` has capacity for 4 screens and declares `handle_key_press()`/`switch_to_next_screen()` for button-driven navigation, but these are not yet implemented — the multi-screen and key-handling code is stubbed.
- **`lib/lvgl/`** — LVGL v8.1.0 compiled as a static library, configured via `examples/inc/lv_conf.h`.

### Hardware Pin Mapping

| Signal | GPIO |
|--------|------|
| I2C SDA | 6 |
| I2C SCL | 7 |
| LCD DC | 8 |
| LCD CS | 9 |
| LCD CLK (SPI1) | 10 |
| LCD MOSI (SPI1) | 11 |
| LCD RST | 12 |
| LCD Backlight (PWM) | 25 |

### Key Configuration

- **`examples/inc/lv_conf.h`** — LVGL settings: 16-bit RGB565 color depth, byte-swap enabled for SPI, 32KB memory pool, 10ms display refresh, 130 DPI.
- **`CMakeLists.txt`** — Targets `pico2` board (RP2350), C11, C++17, `-O2` optimization, 200MHz system clock.
- **Build output:** `build/RP2350-GEEK-LVGL.{elf,uf2,bin,map}` — UF2 is copied to `uf2/`.

### DMA Rendering Flow

`lv_task_handler()` → LVGL detects dirty region → calls `disp_flush_cb()` → `LCD_1IN14_SetWindows()` sets pixel region → DMA transfer sends RGB565 data over SPI1 to LCD → `dma_handler()` ISR fires → calls `lv_disp_flush_ready()` to unblock LVGL.
