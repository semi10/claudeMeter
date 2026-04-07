# ClaudeMeter Implementation Plan

## Context
ClaudeMeter is a physical usage meter for Claude Pro, displayed on a Waveshare RP2350-GEEK board. The project has a working LVGL example in `firmware/` but empty `host/` and `widget/` folders. This plan implements all three layers to complete the end-to-end system: Chrome extension → Python native messaging host → USB serial → RP2350 firmware → LVGL display.

---

## Layer Overview

```
widget/   → Chrome extension (MV3) — intercepts claude.ai usage API
host/     → Python native messaging bridge — relays JSON to USB serial
firmware/ → RP2350 firmware — parses serial JSON, drives LVGL arc gauges
```

---

## Firmware Changes (`firmware/`)

### Files to modify

**`firmware/CMakeLists.txt`** — swap stdio:
```
pico_enable_stdio_uart(RP2350-GEEK-LVGL 0)
pico_enable_stdio_usb(RP2350-GEEK-LVGL  1)
```
Enables USB CDC so `getchar_timeout_us()` reads from the same port the host writes to.

**`firmware/examples/inc/LVGL_example.h`** — append to `lvgl_data_struct`:
```c
lv_obj_t *arc_session;
lv_obj_t *arc_weekly;
lv_obj_t *lbl_sp;      // session percentage
lv_obj_t *lbl_sr;      // session reset time
lv_obj_t *lbl_wp;      // weekly percentage
lv_obj_t *lbl_wr;      // weekly reset time
lv_obj_t *lbl_status;  // "Waiting for data" / "Connection lost"
```
Existing struct members (`scr[4]`, `cur`, `btn`, etc.) are kept untouched.

**`firmware/examples/src/LCD_1in14_LVGL_test.c`** — three targeted changes:

1. **Change orientation** — replace:
   ```c
   LCD_1IN14_Init(HORIZONTAL);
   DISP_HOR_RES = LCD_1IN14_HEIGHT;
   DISP_VER_RES = LCD_1IN14_WIDTH;
   ```
   with:
   ```c
   LCD_1IN14_Init(HORIZONTAL_FLIP);
   DISP_HOR_RES = 240;
   DISP_VER_RES = 135;
   ```
   This gives landscape 270° layout (240×135 px) for two side-by-side gauges.

2. **Replace `Widgets_Init()`** — remove the image demo screen; create:
   - Single screen `dat->scr[0]` with black background
   - Left panel (120×135 px, x=0): session arc + `"Session"` title + `lbl_sp` + `lbl_sr`
   - Right panel (120×135 px, x=120): weekly arc + `"Weekly"` title + `lbl_wp` + `lbl_wr`
   - Each arc: `lv_arc_set_range(arc, 0, 100)`, value=0, sized ~80px, centered in panel
   - Percentage labels (`"--"`) centered on arcs using `lv_font_montserrat_16`
   - Reset time labels (`""`) below arcs using `lv_font_montserrat_14`
   - `dat->lbl_status` — centered overlay label: `"Waiting for data"`, visible at boot

3. **Extend the main loop** — after `lv_task_handler()`:
   ```c
   serial_poll(dat);
   // Check 2min timeout → show "Connection lost" on dat->lbl_status
   if (serial_data_received &&
       (to_ms_since_boot(get_absolute_time()) - serial_last_rx_ms > 120000)) {
       lv_label_set_text(dat->lbl_status, "Connection lost");
       lv_obj_clear_flag(dat->lbl_status, LV_OBJ_FLAG_HIDDEN);
   }
   ```

**`firmware/lib/LCD/LCD_1in14.h`** — add orientation constant:
```c
#define HORIZONTAL_FLIP 2
```

**`firmware/lib/LCD/LCD_1in14.c`** — add 270° rotation support in `LCD_1IN14_SetAttributes()`:
- New `HORIZONTAL_FLIP` branch: MADCTL `0xA0` (MY+MV), dimensions swapped to 240×135
- Window offsets: `x=40, y=52` for the flipped landscape orientation

### New files to create

**`firmware/examples/src/serial_input.c`**

Key logic — non-blocking serial poll with static line buffer:
```c
static char buf[128];
static int buf_pos = 0;
static bool data_received = false;
static uint32_t last_rx_ms = 0;

void serial_poll(lvgl_data_struct *dat) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (buf_pos >= 127) { buf_pos = 0; continue; } // overflow guard
        if (c == '\n') {
            buf[buf_pos] = '\0';
            parse_and_update(buf, buf_pos, dat);
            buf_pos = 0;
        } else {
            buf[buf_pos++] = (char)c;
        }
    }
}
```

JSON parsing via `strstr`/`sscanf` (no external library):
```c
static void parse_and_update(const char *json, int len, lvgl_data_struct *dat) {
    int sp = 0, wp = 0;
    char sr[32] = {0}, wr[32] = {0};

    char *p;
    p = strstr(json, "\"sp\":"); if (p) sscanf(p+5, "%d", &sp);
    p = strstr(json, "\"wp\":"); if (p) sscanf(p+5, "%d", &wp);

    p = strstr(json, "\"sr\":\"");
    if (p) { p+=6; char *e = strchr(p,'"'); if(e){ int n=e-p; if(n<32){memcpy(sr,p,n);sr[n]=0;} } }

    p = strstr(json, "\"wr\":\"");
    if (p) { p+=6; char *e = strchr(p,'"'); if(e){ int n=e-p; if(n<32){memcpy(wr,p,n);wr[n]=0;} } }

    sp = sp < 0 ? 0 : sp > 100 ? 100 : sp;
    wp = wp < 0 ? 0 : wp > 100 ? 100 : wp;

    lv_arc_set_value(dat->arc_session, sp);
    lv_arc_set_value(dat->arc_weekly, wp);
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%d%%", sp); lv_label_set_text(dat->lbl_sp, tmp);
    snprintf(tmp, sizeof(tmp), "%d%%", wp); lv_label_set_text(dat->lbl_wp, tmp);
    lv_label_set_text(dat->lbl_sr, sr);
    lv_label_set_text(dat->lbl_wr, wr);
    lv_obj_add_flag(dat->lbl_status, LV_OBJ_FLAG_HIDDEN);  // hide overlay

    last_rx_ms = to_ms_since_boot(get_absolute_time());
    data_received = true;
}
```

**`firmware/examples/inc/serial_input.h`** — declares `serial_input_init()`, `serial_poll(lvgl_data_struct*)`.

No `CMakeLists.txt` changes needed in `examples/` — `aux_source_directory` auto-picks up `serial_input.c`.

---

## Python Host (`host/`)

### Files to create

```
host/
  claudemeter_host.py       — entry point (Chrome NM protocol → serial)
  serial_writer.py          — serial port wrapper with reconnect
  claudemeter_config.json   — user config: { "com_port": "COM3" }
  native_manifest.json      — Chrome NM manifest template
  install.py                — writes manifest + registry key (uses winreg)
  claudemeter.spec          — PyInstaller spec (onefile, include config JSON)
  requirements.txt          — pyserial>=3.5, pyinstaller>=6.0
```

### `claudemeter_host.py` flow
1. Locate `claudemeter_config.json` via `sys._MEIPASS` (PyInstaller) or `__file__` (dev).
2. Open serial port from config; sleep 2s for board DTR reset.
3. Loop: read 4-byte LE length + JSON body from `sys.stdin.buffer` (Chrome NM protocol).
4. Forward received JSON as `{"sp":..,"sr":"..","wp":..,"wr":".."}` + `\n` to serial.
5. Write back `{"ok":true}` acknowledgement to Chrome (4-byte prefix + JSON).
6. On `SerialException`: log to stderr, retry port open after 3s.

### `native_manifest.json`
```json
{
  "name": "com.example.claudemeter",
  "description": "ClaudeMeter serial bridge",
  "path": "<absolute path to claudemeter_host.exe>",
  "type": "stdio",
  "allowed_origins": ["chrome-extension://EXTENSION_ID/"]
}
```

### `install.py`
- Writes manifest to `%APPDATA%\ClaudeMeter\` (updating `path` and `allowed_origins`).
- Creates registry key `HKCU\Software\Google\Chrome\NativeMessagingHosts\com.example.claudemeter` pointing to the manifest file.
- Accepts extension ID and exe path as CLI args.

### PyInstaller spec key settings
- `onefile=True`, `console=True`
- `datas=[('claudemeter_config.json', '.')]` — config shipped alongside exe
- `hiddenimports=['serial', 'serial.tools.list_ports']`

---

## Chrome Extension (`widget/`)

### Files to create

```
widget/
  manifest.json       — MV3 manifest
  service-worker.js   — native messaging + alarms + polling
  content.js          — injects inject.js into page main world
  inject.js           — overrides window.fetch, posts intercepted data
```

### `manifest.json`
```json
{
  "manifest_version": 3,
  "name": "ClaudeMeter",
  "version": "1.0",
  "permissions": ["nativeMessaging", "scripting", "storage", "alarms", "tabs"],
  "host_permissions": ["https://claude.ai/*"],
  "background": { "service_worker": "service-worker.js" },
  "content_scripts": [{ "matches": ["https://claude.ai/*"], "js": ["content.js"], "run_at": "document_start" }],
  "web_accessible_resources": [{ "resources": ["inject.js"], "matches": ["https://claude.ai/*"] }]
}
```

### `inject.js` (runs in page main world)
- Wraps `window.fetch`; after each response, checks if URL includes `usage`, `rate_limit`, or `billing`.
- Logs all `/api/` calls for debugging (`ClaudeMeter: fetch intercepted`).
- Clones response, parses JSON, logs response keys, validates `five_hour` key presence.
- Extracts `sp = Math.round(data.five_hour.utilization)`, `wp = Math.round(data.seven_day.utilization)`.
- Computes `sr`: delta from `Date.now()` to `five_hour.resets_at`, formatted as `"37m"` or `"2h 10m"`.
- Computes `wr`: day-of-week + HH:MM from `seven_day.resets_at` local time, e.g. `"Thu 10:00"`.
- Posts `{ type: 'CLAUDEMETER_DATA', sp, sr, wp, wr }` via `window.postMessage`.

### `content.js` (isolated world bridge)
- Logs load confirmation (`ClaudeMeter: content script loaded on <url>`).
- Injects `inject.js` into page main world via `<script src=chrome.runtime.getURL('inject.js')>` with `onload`/`onerror` logging.
- Listens for `window.message` events with `type: 'CLAUDEMETER_DATA'`.
- Logs and forwards to service worker via `chrome.runtime.sendMessage({ type: 'USAGE_DATA', payload })`.

### `service-worker.js`
- **`port`** variable (module-level): native messaging connection.
- **`connectNative()`**: calls `chrome.runtime.connectNative('com.example.claudemeter')`, sets `port.onDisconnect` to null `port`.
- **Keep-alive alarm** (`'keepalive'`, `periodInMinutes: 1`): on each tick, reconnect if `port === null`.
- **Poll alarm** (`'poll'`, `periodInMinutes: 1`): calls `pollUsage()` which queries for any `claude.ai` tab; if none found, opens `claude.ai/settings/usage` as an inactive background tab (`active: false`); if found, reloads the tab via `chrome.tabs.reload()` to trigger a fresh fetch intercept.
- **`onMessage`**: if `type === 'USAGE_DATA'` and port is alive, call `port.postMessage(payload)`. Logs all received messages for debugging.
- Both alarms created on `onInstalled` and `onStartup`, with an immediate `pollUsage()` call after 3s delay.
- **Initial boot**: calls `connectNative()` and `setTimeout(pollUsage, 5000)` at service worker load.
- Debug logging throughout (`ClaudeMeter: ...` prefixed console.log at every stage).

---

## Build & Install Steps

### Firmware
1. `firmware/CMakeLists.txt`: swap stdio lines (uart 0, usb 1).
2. Modify/create source files as described above.
3. `ninja -C build` from `firmware/`.
4. Flash `build/RP2350-GEEK-LVGL.uf2` via BOOTSEL mode.

### Host
1. `cd host && pip install -r requirements.txt`
2. `pyinstaller claudemeter.spec` → `dist/claudemeter_host.exe`
3. Load extension in Chrome, copy its ID.
4. `python install.py --exe dist\claudemeter_host.exe --extension-id <ID>`

### Extension
1. `chrome://extensions` → Developer Mode → Load unpacked → select `widget/`.
2. Copy extension ID → use in `install.py`.

---

## Verification

| Layer | Test |
|-------|------|
| Firmware alone | Send `{"sp":50,"sr":"37m","wp":25,"wr":"Thu 10:00"}\n` via PuTTY at 115200; expect arcs to update |
| Timeout | Send nothing for >2min after first message; expect "Connection lost" |
| Boot | Power cycle board; expect "Waiting for data" on screen |
| Host alone | Pipe NM-framed JSON to host stdin; verify serial output on board |
| Extension alone | Open SW inspector; navigate to `claude.ai/settings/usage`; confirm `USAGE_DATA` logged |
| End-to-end | Full stack: tab opens automatically (or background tab created), data flows, display updates within ~1min |

---

## Critical Files

| Path | Action |
|------|--------|
| `firmware/CMakeLists.txt` | Swap stdio lines |
| `firmware/examples/inc/LVGL_example.h` | Add 7 widget pointers to struct |
| `firmware/lib/LCD/LCD_1in14.h` | Add `HORIZONTAL_FLIP` orientation constant |
| `firmware/lib/LCD/LCD_1in14.c` | Add 270° rotation (MADCTL `0xA0`) and window offsets |
| `firmware/examples/src/LCD_1in14_LVGL_test.c` | Change to HORIZONTAL_FLIP, landscape Widgets_Init, extend loop |
| `firmware/examples/src/serial_input.c` | **New** — serial polling + JSON parsing |
| `firmware/examples/inc/serial_input.h` | **New** — header |
| `host/claudemeter_host.py` | **New** — NM host entry point |
| `host/serial_writer.py` | **New** — serial wrapper |
| `host/install.py` | **New** — registry registration |
| `host/claudemeter_config.json` | **New** — COM port config |
| `host/native_manifest.json` | **New** — Chrome NM manifest |
| `host/claudemeter.spec` | **New** — PyInstaller spec |
| `widget/manifest.json` | **New** — MV3 manifest |
| `widget/inject.js` | **New** — fetch interceptor |
| `widget/content.js` | **New** — isolated world bridge |
| `widget/service-worker.js` | **New** — NM + alarms + polling |
