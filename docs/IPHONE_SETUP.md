# Ready-to-add SmartDesk shortcuts

Open these links in Safari on the iPhone while the desk is on your hotspot:

- [SmartDesk Auto Sync](http://172.20.10.2/smartdesk-sync.shortcut): battery, charging, and today's Calendar.
- [SmartDesk Battery Sync](http://172.20.10.2/smartdesk-battery.shortcut): faster battery/charging updates.

Open the downloaded file with Shortcuts and tap **Add Shortcut**. Both contain their sync code, use the device settings already saved by Scriptable, and run without routine popups. Scriptable must remain installed. Run once to grant any requested permissions.

In personal automations, add **Run Shortcut** and choose one of these. Use Battery Sync for Charger/Battery Level triggers; use Auto Sync for Calendar-close/morning triggers. Select Run Immediately where available. Importing a shortcut does not create automation triggers; those must be configured once on the phone. See [shortcut source and rebuild instructions](../shortcuts/README.md).

# iPhone battery and Calendar sync with Scriptable

Bluetooth ANCS carries notifications, not battery percentage or your Calendar database. **SmartDesk Sync** reads these on your iPhone and sends them to your ESP32 over its local Wi-Fi API. Calendar events are read only; the script does not change your calendars.

## First run

1. Install **Scriptable** on the iPhone from the App Store.
2. Keep the Personal Hotspot enabled with **Allow Others to Join** and **Maximize Compatibility**.
3. Open `http://172.20.10.2/iphone_sync.js` in Safari to download **SmartDeskSync.js**. The dashboard also has a download link under **Settings → iPhone battery & Calendar**. The source is [main/web/iphone_sync.js](../main/web/iphone_sync.js).
4. In Scriptable, create a script named **SmartDesk Sync**, paste the downloaded file's contents, and tap Run. If importing through Files/Share is available, import it instead.
5. Enter `http://172.20.10.2` and your existing SmartDesk access token in the first-run dialog. The script stores these in the iPhone Keychain; they are not embedded in the downloaded file.
6. Allow **Calendar** access to the calendars you want to sync, and **Local Network** access if iOS asks. Limited calendar permissions limit what can be synced.
7. The result should show your battery percentage and the count of today's events. Check the OLED battery/agenda pages and dashboard.

Each run sends the actual battery percentage and the first **16 events from today**, sorted by start time, including all-day events. Long titles are shortened to the OLED's UTF-8 byte limit. A day with no events sends an empty list to clear yesterday's agenda. Calendar permission errors do not prevent the battery request from running. Failed network requests are reported instead of showing a false success.

The ESP32 retains the last values between runs. These are snapshots; running Scriptable once does not create a continuous background connection. After a reboot, cached values are marked as having unknown freshness until another sync.

## Automatic runs using native Shortcuts

In Shortcuts on the iPhone, create a shortcut with Scriptable's **Run Script** action and select **SmartDesk Sync**. Run it once interactively to grant permissions. Then add personal automations that run that shortcut: a morning time, **Charger connected**, and selected **Battery Level** thresholds. Choose **Run Immediately** where your iOS version offers it. You can also add a Calendar app open/close automation to refresh after editing your schedule. iOS controls which automations may run unattended; verify on the phone.

The Scriptable shortcut parameter may be `battery` (battery only), `calendar` (calendar only), `both` (default), or `setup` (show the address/token dialog again). The hotspot must be active whenever a sync runs. Use `setup` if DHCP changes the ESP32 address or you rotate its token.

References: [Scriptable battery API](https://docs.scriptable.app/device/#batterylevel), [CalendarEvent API](https://docs.scriptable.app/calendarevent/#today), [Apple personal automations](https://support.apple.com/guide/shortcuts/intro-to-personal-automation-apd690170742/ios).

## Optional reminder sync

Reminders remain supported through the existing authenticated `/api/v1/reminders` endpoint (up to five stored reminders). The supplied Scriptable script only syncs battery and calendar, as requested.

## Charging updates

Replace your existing Scriptable script with the latest `/iphone_sync.js` download, then run it once. Existing Keychain settings are retained. Battery requests now include a `charging` Boolean using Scriptable's charging/full-battery APIs; the OLED and dashboard show the last reported charging state and the OLED animates its charging bar.

For automatic updates on charging changes:

1. In iPhone **Shortcuts → Automation → + → Charger**, select **Is Connected** and **Is Disconnected** (use two automations if your iOS version presents them separately).
2. Select **Run Immediately** where available.
3. Add **Wait** for two seconds to let the battery state settle, then add Scriptable's **Run Script**, choose **SmartDesk Sync**, and set its input/parameter to `battery`. Run it once manually to approve required permissions.
4. Add **Battery Level** automations at the percentages you want (for example 20, 40, 60, 80, and 100) to run the same battery sync as the percentage changes. Enable rising/falling thresholds as appropriate.

A Charger automation updates at plug/unplug, not every percentage point. Battery Level automations provide additional updates at the thresholds you configure. Scriptable does not remain running indefinitely in the iOS background. The hotspot and local network access must be available each time the automation runs. These automations must be configured on the iPhone; flashing the ESP32 cannot create them.

## One-time setup for a hands-off Scriptable workflow

Create these personal automations on the iPhone, each using **Run Immediately** where available. Use Scriptable's **Run Script** action with **SmartDesk Sync**, and pass the stated text parameter. Explicit parameters make successful runs silent, even when Scriptable is open.

| Trigger | Script parameter | Purpose |
| --- | --- | --- |
| Charger connected and disconnected | `battery` | Refresh charging state at plug/unplug; add a two-second Wait before the script |
| Battery Level equals chosen thresholds, e.g. 10%, 20%, …, 100% | `battery` | Refresh percentage without manually running the script |
| Calendar app is closed | `both` | Refresh the agenda after editing it |
| Time of Day, e.g. 7:00 AM daily | `both` | Load the new day's agenda |

Charger and Battery Level conditions may require separate automations depending on the interface. One battery threshold is one trigger, not an every-percent subscription. Keep the hotspot and Bluetooth enabled. Bluetooth notifications work independently; Scriptable battery/calendar sync uses the hotspot's Wi-Fi API.

These triggers must be created on the iPhone; the Mac/ESP32 cannot enable personal automations remotely. Once set up, the configured events run the script without routine manual taps. This is event-driven syncing, not an always-running iOS background process. The existing script setup must be completed once before an automation can run. Background runs never open the setup or success dialogs; a real sync failure is still reported.

## Showing the event happening now

The latest full-sync shortcut includes each event's start **and end** time. Re-add the updated **SmartDesk Auto Sync** shortcut (replace the old shortcut with the same name) and run it once, or update your Scriptable copy. The battery-only shortcut does not need to be replaced for this feature.

The agenda automatically prefers a currently active timed event; for overlaps, it uses the most recently started event. Otherwise it shows an active all-day event, then the earliest upcoming event. End times are exclusive, so the next event can take over exactly when the previous one ends. Expired entries are not displayed as current. Older synced events without end times must be resynced; the device does not invent their durations.
