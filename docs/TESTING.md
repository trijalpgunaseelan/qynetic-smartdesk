# Verification report — 2026-10-26

## Firmware

`idf.py build` passes with ESP-IDF 5.5.6 on macOS for ESP32-C3, with NimBLE, GATT client support, persistent bonds, Wi-Fi coexistence, the embedded dashboard, and OLED interface enabled. The final application is `0x14e870` bytes; the configured `0x1e0000` application partition has 34% free. The final build has no compiler warnings. The obsolete HTTP handler Kconfig default was removed; `main/api.c` configures the handler limit directly.

The binary is generated at `build/qynetic_smartdesk.bin`. Flashed successfully to the ESP32-C3 SuperMini at `/dev/cu.usbmodem101` on 2026-10-26; esptool verified all written hashes. Boot monitoring confirmed Bluetooth advertising (`code 0`), Wi-Fi driver startup, and return from `app_main` without an observed panic. OLED initialization reported no error, but the visible display was not inspected. Native USB Wi-Fi provisioning and saving were subsequently verified. The board joined the configured iPhone hotspot after enabling compatibility mode and applying the configurable transmit-power cap of 34 quarter-dBm units. The dashboard returned HTTP 200, and the authenticated status API reported Wi-Fi online plus Bluetooth ready, encrypted, and bonded. No credentials are included in this report. The chip reports 4 MB of flash; the firmware uses its configured 2 MB layout. Bonded iPhone reconnection and ANCS discovery/subscriptions were observed reaching ready. Notification contents and call-action outcomes still require end-to-end checks.

## ANCS protocol checks

Run from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -Imain main/ancs_protocol.c tests/test_ancs_protocol.c \
  -o /tmp/qynetic-ancs-test
/tmp/qynetic-ancs-test
```

Passed with AddressSanitizer and UndefinedBehaviorSanitizer:

- Exact Control Point request encoding, including little-endian UID and length limits.
- Every two-fragment split of a complete response and byte-by-byte delivery.
- Empty attributes and long attributes consumed without corrupting the next tuple.
- UTF-8 preservation, truncated code points, embedded NUL/control bytes, malformed bytes.
- Wrong command/UID/attribute order, excessive lengths, trailing data after completion.
- 10,000 deterministic malformed input cases.

These are host protocol tests. They do not validate iOS radio, authentication, ATT authorization, descriptor discovery, or bond restoration on hardware.

## Browser checks

`tests/dashboard.cjs` passes in a headless Chromium-based browser using mocked API responses. It checks preview data, rejected/accepted access tokens, reminder creation/completion, OLED message submission, Bluetooth states, six-digit pairing codes including leading zeros, hidden pairing codes after completion, safely rendered notification text, action confirmation and cancellation, UID/session submission, expired-action feedback, empty lists after disconnect, and return to preview. All six views fit at widths 320, 390, 768, 1024, and 1440 pixels without horizontal overflow. No JavaScript runtime errors were observed. See [UI.md](UI.md) for invocation and screenshots.

## Remaining hardware checks

First-time iPhone pairing and permissions; authentication failures; NVS bond restoration; service publication and Service Changed; real added/modified/removed notifications and bursts; call-action outcomes; iPhone lock-screen privacy; Wi-Fi/hotspot/BLE coexistence; heap and stack headroom; OLED timing; UART0 routing; Shortcuts integration; cache freshness after reboot. See [BLUETOOTH.md](BLUETOOTH.md) for the pairing and acceptance procedure.

## Battery/Calendar and button update

The updated firmware builds and flashes with native GPIO3/4 navigation and a credential-free Scriptable helper served at `/iphone_sync.js`. Calendar capacity is now 16 events; HTTP bodies up to 8192 bytes are allocated on the heap. Cached model values no longer acquire a false fresh-sync timestamp during boot.

`tests/test_navigation.c` passes under ASan/UBSan for debounce, page wrapping, long presses, automatic timing, manual pause, and holding both buttons to resume. `node tests/iphone_sync.cjs` passes with mocked iPhone APIs for real battery conversion, today's events, all-day events, bounded UTF-8 titles, a 16-event limit, empty-day clearing, and independent Calendar-permission failures. These checks do not grant iOS permissions or run the script on the phone. Physical button operation requires user wiring.

On-device checks after flashing: dashboard HTTP 200, Scriptable helper download HTTP 200, authenticated status Wi-Fi online and Bluetooth ready. Battery remained -1 and calendar count zero before the first iPhone script run. No artificial battery or calendar records were injected.

## Notification visibility and motion update

Firmware builds and flashes with 5×7 OLED body text, 240 ms page slides, wrapped/scrolled messages, and notification counters. `tests/test_display.c` runs the actual display renderer with host drawing stubs, checks all page geometry/text bounds, and verifies new alerts remain visible after navigation, older alerts can be dismissed, and pairing retains priority. It passes under ASan/UBSan. ANCS parser and navigation sanitizer tests and the dashboard's five-width regression check also pass. Physical appearance and a fresh iPhone notification require on-device observation.

After the motion update, the live API reported Wi-Fi online, Bluetooth ready, battery 35%, seven calendar events, and zero Notification Source events/decoded previews. This confirms the desk connection and sync; fresh iOS notification delivery remains awaiting an on-phone sharing/test check.

Charging/speed update: automatic page dwell is three seconds and slides take 120 ms. Scriptable tests include charging and discharging payloads; navigation debounce now prevents automatic page changes while a button press is settling. Navigation and OLED bounds sanitizer checks pass. On-phone charging automation and physical charge detection need an updated Scriptable run on the iPhone.

Importable shortcuts: Apple Shortcuts CLI successfully signed both binary workflows. Their embedded modes/actions were inspected, and the underlying sync logic passes the mocked iPhone API tests. Shortcut action execution and iOS permission prompts still require an iPhone test; signing alone does not verify them.

## Current agenda and built-in BOOT update

`tests/test_agenda.c` passes under ASan/UBSan for active events, exact end/start boundaries, overlaps, all-day fallback, upcoming events, and legacy records without durations. Navigation checks include BOOT tap, medium hold, long hold, and automatic resume. OLED renderer bounds tests pass with the agenda selector linked. The sync test checks serialized event end times. Signed shortcuts were regenerated to include end times.

The default navigation input is now built-in BOOT GPIO9; RESET remains a hardware restart. Physical gestures require user confirmation after flashing.
