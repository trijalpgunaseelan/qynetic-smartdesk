# macOS deployment — ESP32-C3 SuperMini

## Install toolchain

Use Espressif's official ESP-IDF installation instructions for macOS, choosing a supported ESP-IDF 5.5 release. The installation must provide the RISC-V ESP32-C3 compiler, Python environment, CMake, Ninja and `idf.py`. Run `source ~/esp/esp-idf/export.sh` if you installed at that location. Confirm with `idf.py --version`.

## Build

```sh
cd qynetic-smartdesk
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

**Before flashing:** Confirm actual flash capacity with `esptool.py --chip esp32c3 -p PORT flash_id`. The included partition table assumes >= 2 MB flash and must be changed if the board is smaller. The Bluetooth-enabled firmware builds with ESP-IDF 5.5.6; see `TESTING.md`.

## USB serial

```sh
ls /dev/cu.usb* /dev/cu.wch* 2>/dev/null
idf.py -p /dev/cu.usbmodemYOUR_DEVICE flash monitor
```

Some SuperMini variants use native USB Serial/JTAG and others use a USB-UART bridge. Select the actual port. If necessary, hold BOOT while tapping RESET to enter download mode. Disconnect other serial-monitor applications before provisioning.

## Provision iPhone hotspot

On iPhone 14: Settings > Personal Hotspot > Allow Others to Join; enable Maximize Compatibility if shown. Set a strong hotspot password. Connect the ESP32-C3 to the iPhone hotspot (2.4 GHz compatible mode).

```sh
python3 -m pip install pyserial
python3 tools/provision.py --port /dev/cu.usbmodemYOUR_DEVICE
```

**Caution:** The serial provisioning protocol is plaintext and unauthenticated. Only provision over a trusted physical USB connection, and avoid connecting the device to an untrusted host. It currently remains active after boot and should be disabled or physically restricted before deployment in a shared environment. The firmware uses the native USB Serial/JTAG driver when the configured console includes USB Serial/JTAG; it falls back to UART0 for UART console builds. Native USB provisioning is supported on the connected ESP32-C3 SuperMini. Send `REBOOT` after a successful `SAVE`, or reset the board, to apply Wi-Fi settings.

Restart the board. The IP address should appear in the device serial logs from ESP-IDF networking, or inspect the iPhone hotspot's connected clients if available. Test local reachability from Shortcuts on the iPhone. Hotspot client isolation or iOS local-network restrictions may prevent the phone from reaching the ESP32 HTTP server; this must be physically tested.

## API testing

```sh
python3 tools/test_api.py --host ESP32_IP --token YOUR_TOKEN
```

The HTTP API uses plaintext transport. The bearer token and synchronized private data can be observed by parties able to intercept local traffic. Use only on a trusted hotspot and do not forward ports. HTTPS or an authenticated encrypted transport is required for stronger production privacy.

## Bluetooth pairing

After flashing, follow [Bluetooth setup](BLUETOOTH.md). The default configuration enables NimBLE, GATT client support, NVS bonds, and Wi-Fi/Bluetooth coexistence. No additional component downloads are required.

## SuperMini Wi-Fi diagnostics

The verified board could see the iPhone hotspot but timed out at 802.11 authentication with the default transmit power (reason 2, AUTH_EXPIRE). Setting **SmartDesk → Wi-Fi transmit power cap** to 34 quarter-dBm units resolved this on the attached board. This is now the configurable default in `main/Kconfig.projbuild`; the driver rounds to a supported power level. Reassess the setting if operating at longer distances or on different hardware.

The serial `SCAN` command temporarily disconnects Wi-Fi, pauses automatic retries, reports up to sixteen visible 2.4 GHz SSIDs with channel/signal, then reconnects. It does not print passwords. The `REBOOT` command restarts the board to apply saved provisioning values. Network disconnect logs include ESP-IDF reason codes.
