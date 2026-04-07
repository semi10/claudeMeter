# Spec for claudemeter-usb-serial-display
branch: claude/feature/claudemeter-usb-serial-display

## Summary

ClaudeMeter is a physical usage meter for the Claude Pro Plan, displayed on a Waveshare RP2350-Geek development board. A Chrome extension intercepts API responses from `claude.ai/settings/usage` and transmits four usage values to the board over USB serial via a Python native messaging host. The board renders two arc gauges (session usage and weekly usage) with reset timers on its built-in 1.14" LCD (ST7789 driver).

The system has five layers:
1. Chrome extension content script — intercepts API response on `claude.ai/settings/usage`
2. Chrome extension service worker — manages native messaging connection
3. Python native messaging host — bridges Chrome to the serial port
4. Serial protocol — newline-terminated JSON messages
5. RP2350-Geek firmware — parses serial input and drives LVGL display

---

## Functional Requirements

### Chrome Extension (Layers 1 & 2)

- Must intercept the internal API response from `claude.ai/settings/usage` (not scrape the DOM)
- Must inject a script into the page's own JS context to override `window.fetch` and capture the API response
- Must extract four values: session percentage used, session reset time, weekly percentage used, weekly reset time
- Must forward intercepted data to the service worker via `chrome.runtime.sendMessage()`
- Must use Manifest V3 with permissions: `nativeMessaging`, `scripting`, `storage`, `alarms`, `tabs`
- Must restrict host permissions to `https://claude.ai/*`
- Service worker must maintain a persistent native messaging connection to the Python host
- Service worker must use `chrome.alarms` (firing every ~25 seconds) to prevent termination and reconnect if dropped
- Service worker must poll for fresh data at a sensible interval (e.g. every 60 seconds)
- If `claude.ai/settings/usage` is not open in any tab, the extension may open it in a background tab or wait for the next opportunity

### Python Native Messaging Host (Layer 3)

- Must communicate with Chrome via the Native Messaging stdio protocol (4-byte little-endian length prefix + JSON body)
- Must forward received JSON data to the RP2350-Geek over a USB CDC serial port (115200 baud)
- Must read the COM port name from a local config file (`claudemeter_config.json`) at startup
- Must be registered in the Windows registry so Chrome can launch it automatically
- Must be distributable as a standalone executable (PyInstaller) with no Python installation required on the user's machine

### Serial Protocol (Layer 4)

- Messages must be newline-terminated JSON strings
- Message format: `{"sp":<int>,"sr":"<string>","wp":<int>,"wr":"<string>"}\n`
  - `sp`: session percentage used (0–100)
  - `sr`: session reset time in short form (e.g. `"37m"`)
  - `wp`: weekly percentage used (0–100)
  - `wr`: weekly reset time in short form (e.g. `"Thu 10:00"`)

### Firmware (Layer 5)

- Must receive newline-terminated JSON strings over USB CDC (TinyUSB / Pico SDK stdio USB)
- Must parse the JSON message and extract `sp`, `sr`, `wp`, `wr`
- Must update LVGL widgets without blocking the display task handler
- Must render two `lv_arc` gauges stacked vertically on the 1.14" LCD (135×240 px):
  - Top half: session gauge — arc (sp%), percentage label, reset time label (sr)
  - Bottom half: weekly gauge — arc (wp%), percentage label, reset time label (wr)
- Must poll serial input non-blocking in the main loop alongside `lv_timer_handler()`

### Auto-start (PC side)

- Chrome must start with no visible window on PC startup via a startup shortcut
- "Continue running background apps when Google Chrome is closed" must be enabled
- The extension service worker must start automatically with Chrome and connect to the native host on first data event

---

## Possible Edge Cases

- **Service worker termination**: MV3 service workers can be killed after ~30 seconds of inactivity; the keep-alive alarm must reliably prevent this or reconnect gracefully
- **Native host crash or disconnect**: The service worker must detect `onDisconnect` and re-establish the connection on the next alarm tick
- **COM port not available**: If the board is unplugged or the COM port changes, the Python host must surface a clear error; the extension should not silently drop data
- **COM port name changes**: On Windows, the COM port number may change after replug; the config file approach requires the user to manually update it in this case
- **API endpoint change**: If Anthropic changes the internal API URL or response schema, interception will silently fail; the extension should detect empty/unexpected payloads
- **Tab not open**: If `claude.ai/settings/usage` is never opened, no data flows; the extension should handle this gracefully (e.g., log a warning, optionally open the tab)
- **Serial buffer overflow on board**: If the host sends malformed or oversized messages, the firmware's fixed buffer could overflow; input must be validated or buffer bounds enforced
- **Board not yet ready**: The board may reset when the COM port is first opened; the host should include a short delay before sending data after port open
- **Extension ID mismatch**: The native host manifest `allowed_origins` must match the loaded extension ID exactly; a mismatch silently blocks native messaging
- **PyInstaller AV false positive**: Antivirus software may flag the packaged host executable; this is a known PyInstaller issue and should be documented

---

## Acceptance Criteria

- Opening `claude.ai/settings/usage` in Chrome (while the extension is active) causes data to appear on the RP2350-Geek display within one poll cycle (~60 seconds)
- The session arc gauge accurately reflects the session percentage shown on the usage page
- The weekly arc gauge accurately reflects the weekly percentage shown on the usage page
- Both reset time labels display the correct short-form reset strings
- Closing and reopening the `claude.ai/settings/usage` tab does not break the serial connection
- Unplugging and replugging the board (with the correct COM port restored) resumes display updates without restarting Chrome
- The Python host launches automatically when the extension calls `connectNative()` — no manual startup required
- The packaged `.exe` host runs on a machine without Python installed
- PC reboot results in Chrome starting in the background and the extension reconnecting automatically

---

## Open Questions

- What is the exact confirmed API endpoint URL for usage data, and what does the full response JSON look like? (Spec says "already confirmed" but does not document the exact URL or schema.). Usage Data located at https://claude.ai/settings/usage XHR name is usage the exact json is:
{
    "five_hour": {
        "utilization": 6.0,
        "resets_at": "2026-04-07T19:00:00.226230+00:00"
    },
    "seven_day": {
        "utilization": 12.0,
        "resets_at": "2026-04-09T07:00:01.226247+00:00"
    },
    "seven_day_oauth_apps": null,
    "seven_day_opus": null,
    "seven_day_sonnet": null,
    "seven_day_cowork": null,
    "iguana_necktie": null,
    "extra_usage": {
        "is_enabled": true,
        "monthly_limit": 5000,
        "used_credits": 0.0,
        "utilization": null
    }
}
- Should the extension open `claude.ai/settings/usage` in a background tab automatically, or only passively wait until the user opens it? Yes it should open in a background tab automatically.
- Is there a preferred approach for notifying the user when the serial connection is lost (e.g., display an idle screen on the board, a Chrome notification, or nothing)? Display an idle screen on the board.
- How should COM port discovery be handled if the port number changes after replug — manual config update, or auto-detect? Manual config.
- Should the board display a "waiting for data" or "no connection" state on boot before the first serial message arrives? "Waiting for data".
- What error state should the board render if no message is received for an extended period? The board should show "Connection lost" message.
- Is PyInstaller the final packaging approach, or should a pure Python distribution with a `requirements.txt` be provided as an alternative? PyInstaller.

---

## Testing Guidelines

- **API interception**: Load the extension in developer mode, open `claude.ai/settings/usage`, and inspect the service worker console to confirm usage values are logged correctly
- **Native messaging**: Use `chrome.runtime.connectNative()` from the service worker and verify that JSON messages appear in the Python host's output log
- **Serial output**: Connect the RP2350-Geek and confirm that the Python host writes newline-terminated JSON to the COM port (use a serial monitor such as PuTTY to verify)
- **Firmware JSON parsing**: Send test messages directly to the board's COM port (via a terminal) to verify correct arc and label updates for boundary values (0%, 50%, 100%)
- **Keep-alive**: Leave Chrome idle for 2+ minutes and confirm the native messaging connection is re-established on the next alarm tick
- **Disconnect recovery**: Unplug the board mid-session and confirm the host handles the serial error without crashing, then reconnects on replug
- **End-to-end**: Run the full stack (claude.ai → extension → host → board → display) and verify all four values update correctly after each poll cycle
- **PyInstaller package**: Run the packaged `.exe` on a machine without Python and confirm the host starts, receives a test message from Chrome, and writes to the serial port
