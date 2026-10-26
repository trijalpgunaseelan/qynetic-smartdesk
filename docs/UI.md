# SmartDesk interface

The dashboard is a single self-contained page embedded in the firmware, with no external assets or build dependencies. Preview it by opening `main/web/index.html`. For live data, open `http://<device-ip>/` and click **Connect device** with the provisioned token.

- Warm ivory surfaces, sage accents, serif clock and headings, quiet line icons.
- Responsive sidebar that becomes a mobile navigation strip.
- Clock, agenda, reminders, device details, battery with freshness indication, and iPhone alerts.
- Authenticated reminder creation/completion and ten-second OLED notifications.
- Preview content is explicitly labeled; the token is kept only in memory.
- OLED: large clock, outlined headers, page dots, battery bars, truncated titles, wrapped messages, pairing codes, and Bluetooth notification overlays.

## Verification

`idf.py build` passes with ESP-IDF 5.5.6. Hardware behavior is not verified.

The Playwright check in `tests/dashboard.cjs` uses simulated API responses and checks preview rendering, rejected and accepted tokens, connected data, reminder creation and completion, message sending, disconnect, Bluetooth pairing states, notification rendering, action confirmation/cancellation, expired actions, cleared alerts, and horizontal overflow across all six views at 320, 390, 768, 1024, and 1440 pixels. It also checks for JavaScript runtime errors. This does not replace on-device API testing.

To rerun using an available Playwright installation:

```sh
PLAYWRIGHT_MODULE=/absolute/path/to/playwright BROWSER_PATH=/path/to/chromium node tests/dashboard.cjs
```

Both variables are optional if Playwright and its default browser are installed. Run from the project root. Screenshots are regenerated in `docs/screenshots`.

[Desktop preview](screenshots/dashboard-desktop.png) · [Mobile preview](screenshots/dashboard-mobile.png) · [iPhone notifications](screenshots/iphone-notifications.png)

Reminder edits replace the device's stored list through the existing API. An external Shortcuts sync can replace those edits. The browser clock follows the browser timezone; the OLED currently uses the firmware's IST timezone. The dashboard starts in preview mode on every reload and needs a token to connect again.

Bluetooth setup and remaining physical acceptance checks are described in [BLUETOOTH.md](BLUETOOTH.md). The dashboard refreshes every five seconds when connected and visible.

## Readability and motion update

OLED body text now uses a 5×7 bitmap font (21 characters across), with dedicated large numerals for time/battery. Page changes slide over 120 ms, content refreshes at up to 20 Hz, long notification titles scroll, longer message bodies page through wrapped lines, and the battery bar fills on entry. OLED I2C updates send each changed page in one transaction. The panel remains 128×64 monochrome; non-ASCII glyphs outside the supplied font appear as question marks.

The dashboard uses larger body/event/notification type, stronger muted-text contrast, view/dialog transitions, button feedback, and a subtle status pulse. Browser motion respects `prefers-reduced-motion`. **iPhone alerts → Test OLED** sends a local display test independent of iOS notification sharing.
