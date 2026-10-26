const fs=require('fs'),assert=require('assert');
const run=new (Object.getPrototypeOf(async function(){}).constructor)('args','Keychain','Alert','Request','Device','CalendarEvent','config','Script',fs.readFileSync('main/web/iphone_sync.js','utf8'));
async function check(mode,events,denied=false,charging=false){const sent=[];let summary='';
 class Request{constructor(url){this.url=url;this.response={statusCode:200}}async loadJSON(){sent.push({url:this.url,body:JSON.parse(this.body)});return{ok:true}}}
 const keychain={contains:()=>true,get:()=>JSON.stringify({url:'http://172.20.10.2',token:'x'.repeat(48)})};
 const error=await run({shortcutParameter:mode},keychain,class{},Request,{batteryLevel:()=>.73,isCharging:()=>charging,isFullyCharged:()=>false},{today:async()=>{if(denied)throw Error('Calendar permission denied');return events}},{runsInApp:true},{setShortcutOutput:x=>summary=x,complete:()=>{}}).then(()=>null,e=>e);
 return{sent,summary,error};}
(async()=>{
 const date=new Date('2026-10-26T09:00:00Z');const event={title:'Team review',startDate:date,endDate:new Date(date.getTime()+3600000),isAllDay:false};
 let r=await check('both',[event]);assert.equal(r.error,null);assert.deepEqual(r.sent[0].body,{level:73,charging:false});assert.equal(r.sent[1].body.events[0].start,date.toISOString());assert.equal(r.sent[1].body.events[0].end,new Date(date.getTime()+3600000).toISOString());
 r=await check('both',[],true);assert(r.error);assert.equal(r.sent.length,1);assert.equal(r.sent[0].body.level,73);
 r=await check('calendar',[]);assert.deepEqual(r.sent[0].body,{events:[]});
 r=await check('both',Array.from({length:20},(_,i)=>({...event,title:'☀'.repeat(40),isAllDay:i===0})));assert.equal(r.sent[1].body.events.length,16);assert.equal(r.sent[1].body.events[0].all_day,true);assert(Buffer.byteLength(r.sent[1].body.events[0].title)<=63);
 r=await check('battery',[event]);assert.equal(r.sent.length,1);assert(r.sent[0].url.endsWith('/battery'));
 r=await check('battery',[],false,true);assert.equal(r.sent[0].body.charging,true);
 console.log('PASS: battery, daily calendar, all-day events, 16-event cap, UTF-8 titles, empty-day clearing and independent permission failures and silent automation runs');
})().catch(e=>{console.error(e);process.exit(1)});
