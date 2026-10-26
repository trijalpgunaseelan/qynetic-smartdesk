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
