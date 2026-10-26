# Implementation and acceptance gaps — 2026-10-26

This source tree is a **development prototype**, not a fully functional product. The updated dashboard firmware builds with ESP-IDF 5.5.6 on macOS. Physical ESP32-C3/iPhone behavior remains unverified.

## Remaining limitations

- **Apple ANCS:** implemented with persistent authenticated pairing, discovery/subscriptions, fragmented parsing, actions, and interface integration. Bond restoration and encrypted ANCS readiness have been observed on the attached board. Notification delivery, call-action outcomes, and broader reconnection acceptance checks remain unverified. See `BLUETOOTH.md`.
- **Display:** scaled 3x5 glyphs, six rotating layouts, battery bars, pairing codes, and Bluetooth notifications are implemented. SH1106 support, scroll effects, rotation, and power management are missing.
- **Provisioning:** native USB Serial/JTAG and UART0 console paths are supported. Native USB saving has been verified on the attached SuperMini; encrypted wireless provisioning remains unimplemented.
- **Persistence:** cached sync times are not independently stored. Cached readings now use timestamp zero after reboot so they are shown as having unknown freshness until the iPhone syncs again.
- **Security:** HTTP bearer token over plaintext, unencrypted NVS, unsecured provisioning UART; not suitable for hostile networks.
- **Data:** ISO8601 parser and timezone edge cases need automated verification; JSON size and title limits constrain Shortcuts payloads.
- **Time:** SNTP and timezone behavior require target validation. No RTC backup after power loss.
- **Wi-Fi:** iPhone Personal Hotspot connection, dashboard reachability, and authenticated API status verified on the attached SuperMini after a transmit-power adjustment; broader range/loss/reconnection testing remains pending.
- **Testing:** firmware build and simulated browser interaction checks passed; ANCS parser sanitizer checks pass; BLE hardware tests and OLED hardware tests remain unperformed.

## Acceptance checklist

Physical acceptance is partial: USB flashing/provisioning, Wi-Fi/IP, dashboard/API reachability, and bonded/encrypted ANCS readiness have been observed. Visible OLED output, notification contents, call-action outcomes, and broader reliability remain unverified. See `TESTING.md`.
