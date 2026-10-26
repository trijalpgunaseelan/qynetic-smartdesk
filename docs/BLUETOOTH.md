# iPhone Bluetooth notifications (Apple ANCS)

ANCS is implemented in `main/bluetooth.c`, with a separately tested streaming parser in `main/ancs_protocol.c`. The firmware compiles with ESP-IDF 5.5.6. On the attached SuperMini, an iPhone bond was restored, authenticated ANCS discovery/subscriptions reached ready, and the status API reported encrypted/bonded connectivity while Wi-Fi was online. Notification delivery, call-action outcomes, and the remaining physical acceptance cases below still need end-to-end verification.

## Pair the iPhone

1. Flash the updated firmware. Bluetooth starts independently of Wi-Fi provisioning.
2. Open **Settings → Bluetooth** on the iPhone and select **Qynetic SmartDesk**.
3. Enter the random six-digit code shown on the OLED. If using a headless device, the authenticated local dashboard also shows the code under **iPhone alerts**. Leading zeros matter.
4. Allow system notification sharing when iOS requests it. Check **Share System Notifications** in the device's Bluetooth details if notifications do not appear. Wording and availability depend on iOS.
5. Trigger a notification from another device while the app is in the background. The OLED should show it; the authenticated dashboard should display the same title and message. Lock-screen preview settings can restrict shared contents.

Pairing uses a display-only I/O capability, bonding, and authenticated encryption. Secure Connections is enabled; the client rejects a connection that does not report authenticated encryption. Bond keys persist in NVS. Firmware does not log notification contents or passkeys. A device with no working OLED needs authenticated dashboard access to see the pairing code.

## What the client does

- Advertises an ANCS service solicitation with a scan-response device name.
- Discovers the iPhone's GATT and ANCS services after encryption.
- Subscribes to Service Changed indications when present, Data Source notifications, then Notification Source notifications. Descriptor discovery stays inside each characteristic's handle range.
- Retries service discovery when ANCS is not yet published; processes Service Changed indications and restarts advertising after disconnection.
- Processes added, modified, and removed notification UIDs. Queues up to eight attribute fetches, with one request in flight; excess work increments the `dropped` counter.
- Requests app identifier, title (63 bytes), message (127 bytes), and positive/negative action labels. Parses responses across arbitrary ATT/mbuf boundaries using tuple lengths, not packet lengths or idle timers. Labels and app identifiers are consumed completely but retained in bounded buffers; invalid text is sanitized and truncated UTF-8 tails are removed.
- Keeps the five latest received active notifications in RAM; removes a notification when iOS removes it and clears session data on disconnection/rediscovery. This is a bounded view, not a complete notification-center mirror.
- Supports source-only basic alerts if the optional Control Point/Data Source characteristics are absent.
- Disconnects on a malformed stream, ambiguous timeout, or failed discovery. Attribute requests time out after ten seconds, discovery after thirty, and initial security after forty-five. Bonded iOS can reconnect when available; the accessory does not scan for the phone.

## Interface and actions

The dashboard includes **iPhone alerts**, pairing guidance/code, Bluetooth status, notification cards, incoming-call category, and action buttons. Refresh runs every five seconds while connected and visible. Actions use the exact labels supplied by iOS, and require confirmation. Nothing is answered or dismissed automatically. A GATT write acknowledgement is reported as accepted; the eventual call/app outcome must be observed on the phone.

The OLED has six rotating pages (clock, reminders, agenda, battery, notification list, companion status), a pairing-code screen, and eighteen-second Bluetooth alert overlays. Pre-existing notifications populate the list without triggering an overlay. Calendar, reminders, and battery still use the existing HTTP/Shortcuts sync; ANCS is not a synchronization API for those datasets.

## Reset a stale pairing

Repeat pairing never silently deletes a saved bond. To reset, send the explicit `UNPAIR` line over the existing provisioning UART. This queues a disconnect and clears stored BLE bonds; it does not erase Wi-Fi credentials or synced lists. Then choose **Forget This Device** on the iPhone and pair again. The command uses native USB Serial/JTAG on the SuperMini USB console build, or UART0 on a UART console build; see `DEPLOY_MACOS.md`.

## Verification required on hardware

- First-time passkey pairing, denied permissions, incorrect code, and code timeout.
- Notification sharing enabled/disabled, locked/unlocked iPhone, hidden previews.
- Bond restoration after reboot, out-of-range reconnection, stale-bond recovery.
- Added/modified/removed alerts, bursts, large/multibyte messages, and calls.
- Confirmed positive/negative actions and expired notifications.
- Service publication/removal and Service Changed indications.
- Simultaneous hotspot Wi-Fi/API activity, heap headroom, task stacks, and OLED timing.

References: [Apple ANCS specification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html), [ESP-IDF NimBLE ANCS example](https://github.com/espressif/esp-idf/tree/v5.5.6/examples/bluetooth/nimble/ble_ancs). The implementation is project-specific; the SDK example was consulted for API usage.

## When alerts do not appear

The dashboard's iPhone alerts view now shows lifetime-since-boot Notification Source event counts and decoded preview counts. The authenticated status API also includes `source_events`, `data_bytes`, `decoded`, and `last_error`. These counters survive Bluetooth session resets but reset on firmware restart; they do not log message contents.

New source events are shown immediately as a basic alert while optional preview text is fetched. New alerts can interrupt a manually selected page; button navigation dismisses an existing alert rather than blocking future arrivals. Notification overlays last eighteen seconds. Pre-existing alerts still populate the list quietly, and iOS removal clears them.

If source events stay zero while Bluetooth is ready, check iPhone Bluetooth device settings → Share System Notifications, allow notifications for the sending app, and test with a new notification while the iPhone is locked. If source events increase but decoded previews do not, inspect `data_bytes` and `last_error`. Use **Test OLED** to verify the display path separately; it does not prove iOS notification delivery.
