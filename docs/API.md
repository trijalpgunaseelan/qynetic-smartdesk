# Local API

All POST requests use `Content-Type: application/json` and `Authorization: Bearer <token>`. Body maximum 8192 bytes. POST `/api/v1/reminders` with `{"reminders":[{"id":"r1","title":"PCB review","due":"2026-10-26T18:00:00+05:30","completed":false}]}`. POST `/api/v1/calendar` with `{"events":[{"title":"Meeting","start":"2026-10-26T10:30:00+05:30","all_day":false}]}`. POST `/api/v1/battery` with `{"level":85}`. POST `/api/v1/notification` with `{"text":"Test only"}`. GET `/api/v1/health` and `/api/v1/status` return limited JSON status. POST `/api/v1/config` currently returns 400 (not implemented). ISO timestamps must include timezone offsets; parser needs further hardening.


## Dashboard and status data

`GET /` serves the dashboard directly from firmware flash. No token is needed for the page itself; all device data remains authenticated. `GET /api/v1/status` retains its existing count fields and adds `battery_sync` (epoch seconds), `reminder_items`, and `calendar_items`. Each item contains `id`, `title`, `when` (epoch seconds), `completed`, and `all_day`. Status responses use `Cache-Control: no-store`. The UI uses the existing POST endpoints; provisioning stays on serial.


## Bluetooth status and notification actions

Authenticated `GET /api/v1/status` also returns a `bluetooth` object with `status`, `connected`, `encrypted`, `bonded`, `ready`, `details`, `pairing`, `session`, `error`, `dropped`, `action_status`, `action_error`, and `notifications`. During passkey pairing, it additionally returns `passkey` (display as six digits including leading zeros). The notification list contains up to five objects with `uid`, `category`, `flags`, `received_at`, `app`, `title`, `message`, `positive`, and `negative`. Strings have bounded UTF-8 storage. Notification data is memory-only and valid only for the returned session.

POST `/api/v1/notification-action` with `{"uid":123,"action":0,"session":4}`. Action 0 is positive; 1 is negative. Use the iPhone's label and obtain user confirmation. A positive action needs flag 8; a negative action needs flag 16. The notification must still exist in the same session. HTTP 202 means queued, not performed. Poll status for `action_status` (`queued`, `sending`, `completed`, `failed`, `expired`, `not_connected`, or `disconnected`). `completed` means iPhone acknowledged the Control Point write. Invalid input returns 400; unavailable/stale actions return 409. At most one action is queued/in flight, serialized with attribute fetches on the NimBLE host task.

Calendar storage now retains up to 16 sorted events; reminders retain five. The existing POST schemas are unchanged. `calendar_sync` is returned as an epoch timestamp; zero indicates that a cached reading has unknown freshness since boot. `GET /iphone_sync.js` serves the credential-free Scriptable helper.

Battery POST accepts `{"level":73,"charging":true}`. `charging` is optional for older clients; when omitted, charging status is unknown. Status returns `charging` and `charging_known`. Invalid non-Boolean charging values return 400.

Calendar events now accept optional ISO8601 `end`, which must be later than `start`. Status item objects expose epoch `end` (zero for older records without it). Current-event selection uses `start <= now < end`; legacy events without an end are not assumed to be ongoing. All-day legacy events use the local calendar date.
