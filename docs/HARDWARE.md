# Built-in button navigation (current default)

The connected ESP32-C3 SuperMini uses **BOOT on GPIO9**. No external buttons are needed:

| Gesture (action happens on release) | Result |
| --- | --- |
| Tap BOOT | Next screen |
| Hold BOOT for 0.6–2 seconds | Previous screen |
| Hold BOOT for 2 seconds or longer | Resume automatic screen rotation |
| Press RESET | Hardware restart; it cannot be a navigation input |

Do not hold BOOT while pressing RESET or powering up unless you intend to enter the ESP32 download bootloader. The input is debounced for 40 ms. Change **menuconfig → SmartDesk → Use built-in BOOT button** to disable this mode and restore external GPIO3/4 navigation.

# ESP32-C3 SuperMini wiring

SSD1306 128x64 OLED: VCC -> 3V3, GND -> GND, SDA -> GPIO6, SCL -> GPIO7. Confirm board pin labels and OLED voltage tolerance before applying power. I2C address 0x3C. A genuine SH1106 is **not supported** by the included driver. I2C pins are currently hardcoded and must be made configurable before production. ESP32-C3 boards vary in flash capacity, USB-serial implementation and boot-mode access: check `esptool.py flash_id` and the board schematic.

## Two-button navigation

Use two normally-open momentary push buttons. With power disconnected, wire one button between **GPIO3 and GND** (Previous), and the other between **GPIO4 and GND** (Next). Internal pull-ups are enabled; do not connect the buttons to 5 V. GPIO6/7 remain assigned to the OLED; GPIO18/19 are native USB. Do not use the board's BOOT/RESET buttons for this navigation.

| Gesture | Behavior |
| --- | --- |
| Tap Previous / Next | Previous / next OLED page; pauses automatic page rotation |
| Hold Previous / Next for 0.6 seconds | Browse entries on the reminders, calendar, or notifications page; repeats while held |
| Hold both for 1 second | Resume automatic page rotation every three seconds |

Button inputs are debounced for 40 ms and polled every 20 ms. The OLED refreshes at up to 10 Hz, keeping the clock and navigation responsive. Pairing codes take priority over navigation. Other timed overlays can be suppressed temporarily by using the buttons.

Pin assignments are configurable under **menuconfig → SmartDesk**. Defaults GPIO3/4 avoid the OLED and USB pins; review board pin usage before choosing other pins. Physical button operation requires the buttons to be wired and has not been confirmed remotely.

## Clock without a network

Once set, the ESP32 system clock continues while the board is powered, even with Wi-Fi and Internet unavailable. No network is required for normal ticking. To initialize time using the Mac over USB instead of NTP:

```sh
python3 tools/set_clock.py --port /dev/cu.usbmodem101
```

This requires Python's pyserial package and a correctly set Mac clock. Close any serial monitor first. The USB command is `TIME <UTC epoch seconds>`. It does not change Wi-Fi credentials or reboot the board.

A full power loss removes the ESP32's time reference. No saved timestamp can account for time spent unplugged. For correct time after an offline cold start, add a battery-backed RTC such as a DS3231 and firmware support; the current board has no such module configured. Until time is set, the OLED prompts `SET TIME: USB / WIFI` rather than showing a false date.

Wi-Fi credentials are loaded from saved settings at boot. If the hotspot is unavailable, retries continue automatically with 1, 2, 4, then 8-second delays between attempts. No button press or script is required to reconnect when the saved network becomes available. Actual connection time includes the Wi-Fi scan/authentication time. The iPhone must allow hotspot connections; firmware cannot turn on the iPhone hotspot. Clock rendering does not depend on Wi-Fi status.
