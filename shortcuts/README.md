# Importable SmartDesk shortcuts

If iOS rejects a downloaded file, use the [no-import setup](../docs/IPHONE_SETUP.md#if-iphone-rejects-the-shortcut-file). The unsigned source files are build inputs, not installable shortcuts.

Signed artifacts are embedded in the ESP32 firmware and available in iPhone Safari:

- `http://<desk-ip>/smartdesk-sync.shortcut`: **SmartDesk Auto Sync** — battery, charging, and today's first 16 Calendar events.
- `http://<desk-ip>/smartdesk-battery.shortcut`: **SmartDesk Battery Sync** — battery/charging only, faster for Charger and Battery Level automations.

Open the downloaded file with Shortcuts and choose **Add Shortcut**. Scriptable must be installed and its one-time SmartDesk connection setup completed. The shortcuts contain their own copy of the sync script, reuse the existing Scriptable Keychain configuration, and do not require a particular script name or manually supplied parameter. No Wi-Fi password or device token is embedded in either shortcut.

Each shortcut waits two seconds for charging state to settle, then runs Scriptable's inline-script action with its UI hidden. Successful automated runs do not show popups. Actual permission/network failures are reported. Battery updates run independently of Calendar access failures.

For automatic operation, attach **Run Shortcut → SmartDesk Auto Sync** to Calendar-close and morning automations. Attach **Run Shortcut → SmartDesk Battery Sync** to Charger connected/disconnected and chosen Battery Level thresholds. Select Run Immediately where offered. Personal automation triggers must be created on the iPhone; importing a shortcut cannot enable them remotely. Bluetooth notifications are independent of these shortcuts.

## Rebuild

On a Mac with Apple's Shortcuts CLI:

```sh
python3 tools/build_shortcuts.py --sign
```

The generator reads the project's Scriptable source, forces the correct silent mode for each shortcut, emits unsigned binary plists for inspection, and signs the importable files with `shortcuts sign --mode anyone`. The script uses the published `dk.simonbs.Scriptable.RunScriptInlineIntent` action schema. Signing was verified on this Mac; execution of the imported action and iOS permission prompts must be checked on the iPhone.
