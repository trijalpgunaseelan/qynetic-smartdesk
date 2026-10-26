const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const fs = require('fs');
const assert = require('assert');
(async()=>{
const browser=await chromium.launch({headless:true,...(process.env.BROWSER_PATH ? {executablePath:process.env.BROWSER_PATH} : {}),args:['--no-sandbox']});
const page=await browser.newPage({viewport:{width:1440,height:1050}});
const errors=[];page.on('pageerror',e=>errors.push(e.message));
const state={wifi:true,battery:64,battery_sync:Date.now()/1000,reminder_items:[],calendar_items:[{title:'Device integration review',when:Date.now()/1000+3600,all_day:false}]};
state.bluetooth={status:'advertising',connected:false,encrypted:false,bonded:false,ready:false,details:false,pairing:false,session:4,notifications:[],action_status:''};
let notification=null,actions=[],expireAction=false;
await page.route('http://desk.test/**',async route=>{
const req=route.request();const url=new URL(req.url());
if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:fs.readFileSync('main/web/index.html','utf8')});
if(req.headers().authorization!=='Bearer '+'a'.repeat(32))return route.fulfill({status:401,body:'{"error":"unauthorized"}'});
if(url.pathname.endsWith('/status'))return route.fulfill({contentType:'application/json',body:JSON.stringify(state)});
const body=req.postDataJSON();
if(url.pathname.endsWith('/notification-action')){if(expireAction)return route.fulfill({status:409,body:'{}'});actions.push(body);state.bluetooth.action_status='completed';return route.fulfill({status:202,contentType:'application/json',body:'{\"queued\":true}'})}
if(url.pathname.endsWith('/reminders'))state.reminder_items=body.reminders.map(x=>({...x,when:Date.parse(x.due)/1000}));
if(url.pathname.endsWith('/notification'))notification=body.text;
return route.fulfill({contentType:'application/json',body:'{"ok":true}'});
});
await page.goto('http://desk.test/');
await page.waitForTimeout(700);await page.screenshot({path:'docs/screenshots/dashboard-desktop.png',fullPage:true});
assert.equal(await page.locator('#agenda-list .event').count(),3);
await page.locator('#reminder-list .check').first().click();assert.equal(await page.locator('#reminder-list .task.done').count(),2);
await page.locator('#connect').click();await page.locator('#token').fill('x'.repeat(32));await page.locator('#connect-submit').click();await page.locator('#connect-error').filter({hasText:'not accepted'}).waitFor();
await page.locator('#token').fill('a'.repeat(32));await page.locator('#connect-submit').click();await page.locator('#connect-dialog').waitFor({state:'hidden'});
assert.equal(await page.locator('#agenda-list .event').count(),1);
await page.locator('#add-task').click();await page.locator('#task-title').fill('Test reminder <safe>');await page.locator('#task-submit').click();await page.locator('#task-dialog').waitFor({state:'hidden'});assert.equal(state.reminder_items.length,1);
await page.locator('#reminder-list .check').click();await page.waitForFunction(()=>document.querySelector('#reminder-list .check').getAttribute('aria-pressed')==='true');assert.equal(state.reminder_items[0].completed,true);
await page.locator('[data-view="device"]').click();await page.locator('#notification').fill('A calmer day');await page.locator('#send-message').click();await page.waitForFunction(()=>document.querySelector('#toast').textContent.startsWith('Sent.'));assert.equal(notification,'A calmer day');
// Bluetooth UI follows real status payloads; actions require explicit confirmation.
await page.locator('[data-view="notifications"]').click();
assert.equal(await page.locator('#ble-status').textContent(),'Ready to pair');
state.bluetooth={...state.bluetooth,status:'enter_passkey_on_iphone',connected:true,pairing:true,passkey:123};
await page.locator('#sync').click();await page.locator('#pairing-code').filter({hasText:'000123'}).waitFor();
state.bluetooth={...state.bluetooth,status:'waiting_for_ancs',pairing:false,encrypted:true,bonded:true};
await page.locator('#sync').click();await page.locator('#pairing-code').waitFor({state:'hidden'});
state.bluetooth={...state.bluetooth,status:'ready',ready:true,details:true,notifications:[{uid:4294967295,category:1,flags:24,app:'com.apple.mobilephone',title:'Caller <img src=x onerror=alert(1)>',message:'Incoming call',positive:'Answer',negative:'Decline',received_at:Date.now()/1000}]};
await page.locator('#sync').click();await page.locator('.notification-actions button').first().waitFor();
assert.equal(await page.locator('#notification-list img').count(),0);
await page.locator('.notification-actions button').first().click();await page.locator('[data-close="action-dialog"]').click();assert.equal(actions.length,0);
await page.locator('.notification-actions button').first().click();await page.locator('#action-submit').click();await page.locator('#action-dialog').waitFor({state:'hidden'});
assert.deepEqual(actions,[{uid:4294967295,action:0,session:4}]);
await page.locator('#action-result').filter({hasText:'accepted'}).waitFor();
expireAction=true;
await page.locator('.notification-actions button').last().click();await page.locator('#action-submit').click();await page.locator('#action-error').filter({hasText:'expired'}).waitFor();await page.locator('[data-close="action-dialog"]').click();
state.bluetooth.notifications[0].title='Alex Morgan';await page.locator('#sync').click();await page.waitForFunction(()=>document.querySelector('.notification-card h3').textContent==='Alex Morgan');
await page.waitForTimeout(500);await page.locator('#toast').evaluate(el=>el.classList.add('hidden'));
await page.screenshot({path:'docs/screenshots/iphone-notifications.png',fullPage:true});
state.bluetooth={...state.bluetooth,status:'advertising',connected:false,ready:false,details:false,notifications:[]};
await page.locator('#sync').click();await page.waitForFunction(()=>document.querySelector('#notification-count').textContent==='0 alerts');
assert.equal(await page.locator('.notification-actions button').count(),0);
await page.locator('[data-view="settings"]').click();await page.locator('#disconnect').click();assert.equal(await page.locator('#settings-mode').textContent(),'Preview mode');
await page.locator('[data-view="overview"]').click();
await page.locator('#toast').evaluate(el=>el.classList.add('hidden'));
for(const width of [320,390,768,1024,1440]){
 await page.setViewportSize({width,height:1000});
 for(const view of ['overview','agenda','reminders','notifications','device','settings']){
  await page.locator(`[data-view="${view}"]`).click();await page.waitForTimeout(100);
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`Overflow at ${width} in ${view}`);
 }
 await page.locator('[data-view="overview"]').click();
 if(width===390)await page.screenshot({path:'docs/screenshots/dashboard-mobile.png',fullPage:true});
}
assert.deepEqual(errors,[]);console.log('PASS: preview, authentication failures/success, real-mode rendering, add/complete reminder, notification, Bluetooth/pairing states, session-bound confirmed actions, stale actions, cleared alerts, disconnect, all six views at five responsive widths; no browser errors. API responses mocked.');
await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
