// SmartDesk Sync — run on your iPhone in Scriptable.
// First run: enter the device URL and access token; allow Calendar and Local Network access.
// Credentials stay in the iPhone's Keychain. No account or cloud service is used.
const settingsKey = 'qynetic.smartdesk.connection.v1';
const mode = String(args.shortcutParameter || 'both').toLowerCase();
const automationRun = !!args.shortcutParameter && mode !== 'setup';
let settings;
if (Keychain.contains(settingsKey)) {
  try { settings = JSON.parse(Keychain.get(settingsKey)); } catch (_) {}
}
if (!settings && automationRun) {
  throw new Error('Open SmartDesk Sync once in Scriptable to save the desk address and token before enabling automations.');
}
if (!settings || mode === 'setup') {
  const prompt = new Alert();
  prompt.title = 'Connect SmartDesk';
  prompt.message = 'Enter your desk address and device access token. Keep your iPhone hotspot enabled.';
  prompt.addTextField('Device URL', settings ? settings.url : 'http://172.20.10.2');
  prompt.addSecureTextField('Device access token', settings ? settings.token : '');
  prompt.addAction('Save and sync'); prompt.addCancelAction('Cancel');
  if (await prompt.presentAlert() < 0) { Script.complete(); return; }
  const url = prompt.textFieldValue(0).trim().replace(/\/+$/, '');
  const token = prompt.textFieldValue(1).trim();
  if (!/^http:\/\/(?:\d{1,3}\.){3}\d{1,3}(?::\d+)?$/.test(url) || token.length < 32 || token.length > 64) {
    throw new Error('Enter a local IPv4 device URL and a 32–64 character access token.');
  }
  settings = { url, token }; Keychain.set(settingsKey, JSON.stringify(settings));
}
async function post(path, body) {
  const request = new Request(settings.url + '/api/v1/' + path);
  request.method = 'POST'; request.timeoutInterval = 12;
  request.headers = { 'Content-Type': 'application/json', Authorization: 'Bearer ' + settings.token };
  request.body = JSON.stringify(body);
  const result = await request.loadJSON();
  const code = request.response ? request.response.statusCode : 0;
  if (code !== 200 || !result.ok) throw new Error('SmartDesk ' + path + ' sync failed (' + code + '). Check the address, token, and hotspot.');
}
function clippedTitle(value) {
  let result = '';
  for (const character of String(value || 'Untitled event')) {
    const candidate = result + character;
    if (encodeURIComponent(candidate).replace(/%[0-9A-F]{2}|./gi, 'x').length > 63) break;
    result = candidate;
  }
  return result;
}
const messages = [], errors = [];
// Sync independently: denied Calendar access must not prevent battery updates.
if (mode !== 'calendar') {
  try {
    const reading = Device.batteryLevel();
    if (reading < 0 || reading > 1) throw new Error('iPhone battery reading is unavailable.');
    const level = Math.round(reading * 100);
    const charging = Device.isCharging() || Device.isFullyCharged();
    await post('battery', { level, charging }); messages.push('Battery: ' + level + '% · ' + (charging ? 'Charging / plugged in' : 'On battery'));
  } catch (error) { errors.push(String(error.message || error)); }
}
if (mode !== 'battery') {
  try {
    const today = await CalendarEvent.today([]);
    const ordered = today.sort((a, b) => a.startDate - b.startDate);
    const events = ordered.slice(0, 16).map(event => ({
      title: clippedTitle(event.title), start: event.startDate.toISOString(), end: event.endDate.toISOString(), all_day: !!event.isAllDay
    }));
    // [] clears yesterday's agenda on a day without events.
    await post('calendar', { events });
    messages.push('Calendar: ' + events.length + ' events today' + (ordered.length > 16 ? ' (first 16 of ' + ordered.length + ')' : ''));
  } catch (error) { errors.push('Calendar: ' + String(error.message || error)); }
}
const summary = messages.concat(errors).join('\n');
if (config.runsInApp && !automationRun) {
  const result = new Alert(); result.title = errors.length ? 'Sync needs attention' : 'Your desk is in sync';
  result.message = summary; result.addAction('Done'); await result.presentAlert();
}
Script.setShortcutOutput(summary);
if (errors.length) throw new Error(summary);
Script.complete();
