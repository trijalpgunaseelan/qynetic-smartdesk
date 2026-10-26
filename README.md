> **Development prototype — build and host tests verified.** ANCS and its interface are implemented; physical iPhone/ESP32 acceptance testing remains pending. See `docs/BLUETOOTH.md`.

# Qynetic SmartDesk — ESP32-C3 source prototype

**Status: DEVELOPMENT PROTOTYPE / BUILD VERIFIED (ESP-IDF 5.5.6).** Implements an SSD1306 interface, a NimBLE Apple ANCS client with passkey pairing and persistent bonds, a local dashboard and authenticated HTTP API, cached reminders/calendar/battery in NVS, Wi-Fi retry, and SNTP clock. **Secure Wi-Fi provisioning, SH1106 support, and production-grade storage remain incomplete.** See `docs/TESTING.md`.

## macOS toolchain

Install the official ESP-IDF 5.5 release from Espressif's installation guide and activate the environment (`. $HOME/esp/esp-idf/export.sh`, adjusting location). Check `idf.py --version`. Use `idf.py set-target esp32c3`, `idf.py menuconfig`, `idf.py build`. Identify serial port with `ls /dev/cu.*` before/after plugging in the board, then `idf.py -p /dev/cu.usbmodemXXXX flash monitor` using the actual port. Some SuperMini variants require BOOT held while tapping RESET to enter download mode.

**Wi-Fi provisioning uses the serial tool; a wireless setup flow is not implemented.** NVS keys expected in namespace `smartdesk`: `ssid`, `password`, and `token` (up to 64 characters). A production device needs a safe first-boot provisioning method. Do not flash personal passwords in plaintext binaries. Without pre-provisioned NVS, Wi-Fi and authenticated POST requests cannot work.

For iPhone Personal Hotspot, use the iPhone's hotspot SSID/password as the station network. Keep hotspot discovery enabled during initial pairing; use Maximize Compatibility if required. Verify the ESP32 and iPhone can communicate locally. See `docs/IPHONE_SETUP.md` and `docs/SECURITY.md`.

Copyright Qynetic Labs. Development prototype, not certified hardware.


## Deployment
See [macOS deployment](docs/DEPLOY_MACOS.md) for build, flashing, and serial provisioning.

## SmartDesk dashboard

Open [main/web/index.html](main/web/index.html) in your browser for an interactive preview, or flash the firmware and visit `http://<device-ip>/` on the same network. No npm install, internet connection, CDN, or external fonts are needed.

The responsive dashboard includes a live local clock, agenda, reminders, iPhone battery, device details, and iPhone notifications. Click **Connect device** on the device-hosted page and enter your provisioned bearer token for real data. The token stays in tab memory. You can add or complete reminders (five stored items maximum), refresh device data, and send a ten-second OLED message. Preview data is illustrative and changes reset on reload. Automatic refresh runs every five seconds while the tab is visible.

The OLED uses a large clock, six rotating pages, progress indicators, wrapped notifications, and a pairing-code screen. The browser clock uses your computer's local timezone; the firmware currently uses IST.

See [UI verification](docs/UI.md) for screenshots and checks. Hardware display and iPhone integration remain unverified.

## iPhone notifications over Bluetooth

In iPhone Bluetooth settings, pair with **Qynetic SmartDesk**, enter the random code shown on the OLED, and allow system notification sharing. The **iPhone alerts** dashboard view shows Bluetooth state and notifications, with confirmed actions labeled by iOS. Bond keys persist across restarts. See [pairing, recovery, and hardware checks](docs/BLUETOOTH.md). Calendar, reminders, and battery still sync using Shortcuts over HTTP.

## Battery, today's Calendar, and buttons

Use the supplied [Scriptable sync script](main/web/iphone_sync.js) on your iPhone to send its real battery percentage and up to 16 events from today's Apple Calendar. Download it from `http://<device-ip>/iphone_sync.js`, or use the dashboard Settings link. First run asks for the desk URL/access token and Calendar permissions. See [iPhone setup](docs/IPHONE_SETUP.md). The firmware cannot read iPhone battery/calendar automatically through ANCS; the script must run to refresh them.

The default uses the built-in BOOT button: tap for Next, hold 0.6–2 seconds for Previous, or hold at least two seconds to resume automatic rotation (actions apply on release). RESET remains a hardware restart. Optional external GPIO3/4 buttons are available by disabling built-in BOOT mode in menuconfig. See [wiring and gestures](docs/HARDWARE.md).
