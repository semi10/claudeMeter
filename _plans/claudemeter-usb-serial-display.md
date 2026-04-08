# ClaudeMeter Implementation Plan

## Context
ClaudeMeter is a physical usage meter for Claude Pro, displayed on a Waveshare RP2350-GEEK board. The project has a working LVGL example in `firmware/` but empty `host/` and `widget/` folders. This plan implements all three layers to complete the end-to-end system.

```
widget/   → Chrome extension (MV3) — intercepts claude.ai usage API
host/     → Python native messaging bridge — relays JSON to USB serial
firmware/ → RP2350 firmware — parses serial JSON, drives LVGL arc gauges
```

---

## Firmware (`firmware/`)

### Build configuration

USB CDC stdio must be enabled and UART stdio disabled so that `getchar_timeout_us()` reads from the same USB port the host writes to. In `CMakeLists.txt`, set the UART stdio target to 0 and the USB stdio target to 1.

### Data structures (`firmware/examples/inc/LVGL_example.h`)

Add seven LVGL object pointers to `lvgl_data_struct`: two arc gauges (`arc_session`, `arc_weekly`), two percentage labels (`lbl_sp`, `lbl_wp`), two reset-time labels (`lbl_sr`, `lbl_wr`), and one status label (`lbl_status`). Existing struct members are kept untouched.

### LCD orientation (`firmware/lib/LCD/`)

The display must run in a 270° flipped landscape orientation (240 px wide × 135 px tall). Add an `HORIZONTAL_FLIP` constant (value `2`) to `LCD_1in14.h`. In `LCD_1in14.c`, add a branch for this constant in `LCD_1IN14_SetAttributes()`: send MADCTL byte `0xA0` (MY+MV bits), swap the dimension registers to 240×135, and set window offsets to x=40, y=52.

### UI layout (`firmware/examples/src/LCD_1in14_LVGL_test.c`)

**Initialisation** — Init the LCD with `HORIZONTAL_FLIP` and set `DISP_HOR_RES=240`, `DISP_VER_RES=135` before calling `LVGL_Init()`.

**`Widgets_Init()`** — Build the full UI on a single black screen (`scr[0]`):

- Divide the screen into two equal panels, each 120×135 px. The Session panel sits at x=0, the Weekly panel at x=120.
- Each panel contains, top to bottom: a title label ("Session" / "Weekly") at the top center; an arc gauge 80×80 px centered in the panel; a percentage label and a reset-time label.
- Arc gauges span 270° of travel (background arc from 135° to 45°, going clockwise). Background arc color is dark gray (40, 40, 40). Indicator arc color is percentage-based: green (0, 255, 0) at 0%, interpolating to yellow (255, 255, 0) at 50%, then to red (255, 0, 0) at 100%. Initial indicator color is green (0% value). Arc width is 8 px on both parts. The knob is removed and the arc is not clickable. Value range is 0–100.
- The percentage label is a **child of the arc object** (not the panel), aligned to the arc's center. This ensures the entire text including the `%` sign stays visually centered regardless of digit count. Initial text is `"---"`. Font: montserrat 16.
- The reset-time label is anchored to the bottom center of its panel with a small margin. Initial text is empty. Font: montserrat 14, light gray color.
- The status label (`lbl_status`) is a child of the screen, anchored to `BOTTOM_MID` with a small bottom margin — the same vertical position as the reset-time labels. Initial text: `"Waiting for data"`. Font: montserrat 14. This label is visible at boot and must cover both panels when shown.

**Main loop** — After each `lv_task_handler()` call, run `serial_poll()`, then evaluate two timed conditions using the current millisecond timestamp from `to_ms_since_boot()`:

1. **Heartbeat fade** — Once data has ever been received, every 5 seconds recalculate each arc's indicator color independently. Compute the base color from the arc's current percentage value using the green→yellow→red gradient, then linearly interpolate toward background gray (40, 40, 40) based on the fraction of 120 seconds that has elapsed since the last received packet. At exactly 120 s the color equals background gray, making the gauges invisible against their track.

2. **Timeout reset** — Once data has ever been received and 120 seconds have elapsed since the last packet: reset both arc values to 0, set both percentage labels back to `"---"`, clear both reset-time labels, hide the reset-time labels, show the status label with text `"Connection lost"`, and clear the `serial_data_received` flag to prevent this block from firing repeatedly.

The loop delay is 5 ms.

### Serial input module (`firmware/examples/src/serial_input.c` + `.h`)

`serial_poll()` is called every loop iteration. It drains all available bytes from USB stdin using `getchar_timeout_us(0)` (zero timeout = non-blocking). Characters accumulate in a 128-byte static line buffer. When a newline `\n` is received the buffer is null-terminated and handed to `parse_and_update()`, then the buffer position resets. A buffer overflow guard resets the position if it would exceed 127.

`parse_and_update()` extracts four fields from the JSON line using `strstr` + `sscanf` / pointer arithmetic — no external JSON library. Fields: `"sp"` (int, session percentage), `"wp"` (int, weekly percentage), `"sr"` (string, session reset time), `"wr"` (string, weekly reset time). Both integer values are clamped to [0, 100]. After extraction:
- Update both arc values.
- Set percentage labels to `"N%"` format.
- Set reset-time labels and make them visible (clear `LV_OBJ_FLAG_HIDDEN`).
- Hide the status label.
- Set each arc's indicator color based on its percentage using `pct_to_color()` — a green→yellow→red gradient. This is the "heartbeat pulse" that snaps the color back on every new packet.
- Record the current timestamp in `serial_last_rx_ms` and set `serial_data_received = true`.

`pct_to_color()` is a `static inline` function declared in the header. It maps a 0–100 percentage to a green→yellow→red color: from 0–50% red ramps 0→255 while green stays 255; from 50–100% red stays 255 while green ramps 255→0. Blue is always 0.

`serial_last_rx_ms` and `serial_data_received` are module-level globals, declared `extern` in the test file. No `CMakeLists.txt` changes are needed in `examples/` because `aux_source_directory` picks up `serial_input.c` automatically.

---

## Python Host (`host/`)

### Files

| File | Purpose |
|------|---------|
| `claudemeter_host.py` | Entry point — Chrome NM protocol → serial |
| `serial_writer.py` | Serial port wrapper with auto-reconnect |
| `claudemeter_config.json` | User config: COM port name |
| `native_manifest.json` | Chrome NM manifest template |
| `install.py` | Writes manifest + Windows registry key |
| `claudemeter.spec` | PyInstaller build spec |
| `requirements.txt` | `pyserial>=3.5`, `pyinstaller>=6.0` |

### `claudemeter_host.py` behaviour

Locate `claudemeter_config.json` relative to the executable (using `sys._MEIPASS` when bundled by PyInstaller, or `__file__` in dev mode). Open the configured COM port, then wait 2 seconds for the board to complete its DTR-triggered reset.

Enter a read loop: read a 4-byte little-endian length prefix from `sys.stdin.buffer`, then read that many bytes as the JSON body (Chrome Native Messaging framing). Forward the JSON payload plus a newline character to the serial port. Write an acknowledgement back to Chrome using the same 4-byte-prefix framing with body `{"ok":true}`.

On `SerialException`, log to stderr and retry opening the port after a 3-second delay.

### `native_manifest.json` structure

A standard Chrome Native Messaging manifest: name `com.example.claudemeter`, type `stdio`, path pointing to the compiled exe, and `allowed_origins` listing the extension's chrome-extension URL.

### `install.py` behaviour

Accepts the exe path and extension ID as command-line arguments. Writes the completed manifest JSON to `%APPDATA%\ClaudeMeter\`. Creates the Windows registry key `HKCU\Software\Google\Chrome\NativeMessagingHosts\com.example.claudemeter` pointing to that manifest file.

### PyInstaller spec

Single-file executable (`onefile=True`), console mode. Bundles `claudemeter_config.json` into the executable root. Declares hidden imports for `serial` and `serial.tools.list_ports`.

---

## Chrome Extension (`widget/`)

### Files

| File | Purpose |
|------|---------|
| `manifest.json` | MV3 manifest |
| `service-worker.js` | Native messaging, alarms, polling |
| `content.js` | Isolated-world bridge |
| `inject.js` | Fetch interceptor in page main world |

### `manifest.json` structure

Manifest version 3. Permissions: `nativeMessaging`, `scripting`, `storage`, `alarms`, `tabs`. Host permission: `https://claude.ai/*`. Background service worker: `service-worker.js`. Content script `content.js` injected at `document_start` on all `claude.ai` pages. `inject.js` declared as a web-accessible resource for the same host.

### `inject.js` — fetch interceptor (page main world)

Wraps `window.fetch`. After every response, checks whether the URL contains `usage`, `rate_limit`, or `billing`. For matching URLs, clones the response and parses the JSON body without consuming the original. Validates that the `five_hour` key is present. Extracts:
- `sp`: session utilization percentage, rounded to integer from `five_hour.utilization`.
- `wp`: weekly utilization percentage, rounded to integer from `seven_day.utilization`.
- `sr`: time remaining until session reset — difference between `five_hour.resets_at` and now, formatted as `"Xm"` (under an hour) or `"Xh Ym"`.
- `wr`: weekly reset day and time from `seven_day.resets_at`, formatted as `"DDD HH:MM"` in local time (e.g. `"Thu 10:00"`).

Posts the four values as `{ type: 'CLAUDEMETER_DATA', sp, sr, wp, wr }` via `window.postMessage` to the content script. Logs all `/api/` requests and key steps with a `ClaudeMeter:` prefix for debugging.

### `content.js` — isolated-world bridge

On load, injects `inject.js` into the page's main world by creating a `<script>` tag whose `src` is the extension URL of `inject.js`. Listens for `window.message` events of type `CLAUDEMETER_DATA` and forwards the payload to the service worker via `chrome.runtime.sendMessage` with type `USAGE_DATA`. Logs load confirmation, injection success/failure, and every forwarded message.

### `service-worker.js` — orchestrator

Maintains a module-level `port` variable for the native messaging connection. `connectNative()` opens a persistent connection to `com.example.claudemeter` and nulls `port` on disconnect.

On `onInstalled` and `onStartup`: create a `keepalive` alarm (every 1 minute) and a `poll` alarm (every 1 minute), then schedule `pollUsage()` after a 3-second delay.

On each `keepalive` alarm tick: reconnect if `port` is null. On each `poll` alarm tick: call `pollUsage()`.

`pollUsage()` queries all tabs for a `claude.ai` URL. If none exists, check `lastTabOpenTime`: if fewer than 15 seconds have elapsed since the last tab was opened, skip (to prevent duplicate tabs when multiple startup triggers fire in quick succession); otherwise record the current time in `lastTabOpenTime` and open `claude.ai/settings/usage` as an inactive background tab. If a tab exists, reload it to trigger a fresh fetch interception and data post.

On `onMessage`: if type is `USAGE_DATA` and `port` is alive, forward the payload via `port.postMessage`. Log all received messages.

At service worker startup: call `connectNative()` immediately, then call `pollUsage()` after 5 seconds.

---

## Build & Install

### Firmware
1. Swap stdio lines in `CMakeLists.txt` (UART=0, USB=1).
2. Apply all source changes described above.
3. Run `ninja -C build` from `firmware/`.
4. Flash `build/RP2350-GEEK-LVGL.uf2` via BOOTSEL mode.

### Host
1. `pip install -r requirements.txt` inside `host/`.
2. `pyinstaller claudemeter.spec` produces `dist/claudemeter_host.exe`.
3. Load the extension in Chrome and copy its ID.
4. `python install.py --exe dist\claudemeter_host.exe --extension-id <ID>`.

### Extension
1. Open `chrome://extensions`, enable Developer Mode, click "Load unpacked", select `widget/`.
2. Copy the assigned extension ID for use in `install.py`.

---

## Verification

| Layer | Test |
|-------|------|
| Firmware alone | Send `{"sp":50,"sr":"37m","wp":25,"wr":"Thu 10:00"}` + newline via PuTTY at 115200; session arc turns yellow (50%), weekly arc turns green-yellow (25%) |
| Heartbeat fade | After the above, send no further data; arcs should visually dim to gray over 2 minutes |
| Timeout | After 2 minutes of silence, arcs reset to 0, labels show `"---"`, reset times hidden, "Connection lost" appears at bottom |
| Boot | Power cycle board; "Waiting for data" appears at bottom, arcs at 0 with `"---"` labels |
| Host alone | Pipe NM-framed JSON to host stdin; verify the newline-terminated JSON arrives on the serial port |
| Extension alone | Open service worker inspector; navigate to `claude.ai/settings/usage`; confirm `USAGE_DATA` message logged |
| End-to-end | Full stack running: tab opens or reloads automatically, data flows through NM host to serial, display updates within ~1 minute |

---

## Critical Files

| Path | Action |
|------|--------|
| `firmware/CMakeLists.txt` | Disable UART stdio, enable USB stdio |
| `firmware/examples/inc/LVGL_example.h` | Add 7 widget pointers to struct |
| `firmware/lib/LCD/LCD_1in14.h` | Add `HORIZONTAL_FLIP` constant (value 2) |
| `firmware/lib/LCD/LCD_1in14.c` | Add 270° rotation branch: MADCTL `0xA0`, 240×135, offsets x=40 y=52 |
| `firmware/examples/src/LCD_1in14_LVGL_test.c` | HORIZONTAL_FLIP init, full Widgets_Init, main loop with heartbeat fade and timeout reset |
| `firmware/examples/src/serial_input.c` | **New** — non-blocking serial poll and JSON parse |
| `firmware/examples/inc/serial_input.h` | **New** — exposes `serial_poll()` |
| `host/claudemeter_host.py` | **New** — NM host entry point |
| `host/serial_writer.py` | **New** — serial port wrapper |
| `host/install.py` | **New** — manifest + registry registration |
| `host/claudemeter_config.json` | **New** — COM port config |
| `host/native_manifest.json` | **New** — Chrome NM manifest template |
| `host/claudemeter.spec` | **New** — PyInstaller spec |
| `widget/manifest.json` | **New** — MV3 manifest |
| `widget/inject.js` | **New** — fetch interceptor |
| `widget/content.js` | **New** — isolated-world bridge |
| `widget/service-worker.js` | **New** — NM connection, alarms, polling |
