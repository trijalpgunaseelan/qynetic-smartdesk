#include "smartdesk.h"
#include "bluetooth.h"
#include "navigation.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 5x7 body type, dedicated 3x5 scaled numerals, and 240 ms page slides. */
static void ascii_text(const char *text,char *out,size_t size) {
    size_t n=0;
    while(*text&&n+1<size){unsigned char c=(unsigned char)*text++;
        if(c>=128){while(((unsigned char)*text&0xc0)==0x80)text++;c='?';}
        out[n++]=(c<32)?' ':(char)c;
    }out[n]=0;
}
static void heading(const char *text) {
    char label[19];snprintf(label,sizeof(label),"%s",text);sd_oled_text(0,1,label);
    sd_oled_rect(0,11,128,1,true);sd_oled_rect(113,2,11,6,false);sd_oled_rect(125,3,2,4,true);
    if(sd.battery>=0)sd_oled_rect(114,3,sd.battery*9/100,4,true);
}
static void marquee(int y,const char *text,unsigned elapsed) {
    char normalized[160],line[22];ascii_text(text,normalized,sizeof(normalized));
    size_t length=strlen(normalized),offset=0;
    if(length>21&&elapsed>1200){unsigned step=(elapsed-1200)/220,range=(unsigned)length-21;unsigned cycle=range+12;
        offset=step%cycle;if(offset>range)offset=range;}
    snprintf(line,sizeof(line),"%s",normalized+offset);sd_oled_text(0,y,line);
}
static void paragraph(int y,const char *text,int lines,unsigned elapsed) {
    char normalized[160],rows[12][22]={{0}};ascii_text(text,normalized,sizeof(normalized));
    const char *p=normalized;int count=0;
    while(*p&&count<12){size_t n=strlen(p);if(n>21){n=21;for(size_t k=21;k>9;k--)if(p[k]==' '){n=k;break;}}
        memcpy(rows[count++],p,n);p+=n;while(*p==' ')p++;}
    unsigned start=count>lines?(elapsed/2800)%(unsigned)(count-lines+1):0;
    for(int i=0;i<lines&&i+(int)start<count;i++)sd_oled_text(0,y+i*9,rows[start+i]);
}
static int selected(const sd_navigation *n,int page,int count,unsigned tick) {
    if(!count)return 0;
    return n->manual?(n->selection[page]%count+count)%count:(int)((tick/4)%(unsigned)count);
}
static const char *app_name(const sd_ble_notification *n) {
    if(n->category==1)return "INCOMING CALL";
    if(strstr(n->text.app,"MobileSMS"))return "MESSAGES";
    if(strstr(n->text.app,"whatsapp"))return "WHATSAPP";
    if(strstr(n->text.app,"mobilemail"))return "MAIL";
    return "IPHONE ALERT";
}
void sd_draw(void) {
    int64_t ms=esp_timer_get_time()/1000;
    unsigned tick=(unsigned)(ms/1000);
    const sd_navigation *n=sd_navigation_state();
    static sd_ble_state ble;sd_ble_snapshot(&ble);
    int page=n->page,scene=page;
    time_t now=time(NULL);struct tm t;localtime_r(&now,&t);char text[64];
    int agenda=sd_agenda_current(sd.calendar,sd.nc,now);
    if(n->manual&&sd.nc&&n->selection[2])agenda=((agenda<0?0:agenda)+n->selection[2]%sd.nc+sd.nc)%sd.nc;
    bool overlay=ms>=n->suppress_until;
    /* Navigation dismisses an existing alert, not alerts that arrive later. */
    bool new_ble_alert=ble.overlay_until/1000>n->suppress_until+8000;
    static time_t previous_message_until;static int64_t message_arrived;
    if(sd.notification_until!=previous_message_until){previous_message_until=sd.notification_until;message_arrived=ms;}
    bool show_message=overlay||message_arrived>n->suppress_until-10000;
    const sd_ble_notification *alert=NULL;
    if((overlay||new_ble_alert)&&ble.count&&ble.overlay_until>ms*1000)alert=&ble.items[0];
    if(ble.pairing)scene=10000001;
    else if(alert)scene=2000000+(int)(alert->text.uid%1000000);
    else if(show_message&&sd.notification_until>now)scene=10000002;
    else if(page==1||page==2||page==4){int count=page==1?sd.nr:page==2?sd.nc:ble.count;scene+=(page==2?(agenda<0?0:agenda):selected(n,page,count,tick))*16;}
    static int last_scene=-1;static int64_t entered;
    if(scene!=last_scene){last_scene=scene;entered=ms;}
    unsigned elapsed=(unsigned)(ms-entered);
    sd_oled_clear();
    if(ble.pairing){
        heading("PAIR IPHONE");sd_oled_text(0,16,"ENTER CODE ON PHONE");
        snprintf(text,sizeof(text),"%06lu",(unsigned long)ble.passkey);sd_oled_text_scaled(8,28,text,3);
        sd_oled_text(0,47,"QYNETIC SMARTDESK");
    }else if(alert){
        heading(app_name(alert));marquee(16,alert->text.title[0]?alert->text.title:"NEW NOTIFICATION",elapsed);
        paragraph(29,alert->text.message[0]?alert->text.message:"OPEN ON YOUR IPHONE",3,elapsed);
    }else if(scene==10000002){heading("DESK MESSAGE");paragraph(16,sd.notification,4,elapsed);
    }else switch(page){
    case 0:
        heading("QYNETIC / NOW");
        if(now<1700000000){sd_oled_text_scaled(3,20,"HELLO",3);sd_oled_text(0,44,"SET TIME: USB / WIFI");}
        else{strftime(text,sizeof(text),"%H:%M",&t);sd_oled_text_scaled(5,18,text,4);strftime(text,sizeof(text),"%a %d %b",&t);sd_oled_text(0,44,text);}
        break;
    case 1:{
        heading("REMINDERS");
        if(!sd.nr){sd_oled_text_scaled(2,20,"ALL CLEAR",2);sd_oled_text(0,43,"A LITTLE OPEN SPACE.");break;}
        int index=selected(n,1,sd.nr,tick);const sd_item *item=&sd.reminders[index];
        snprintf(text,sizeof(text),"%s  %d/%d",item->completed?"DONE":"TO DO",index+1,sd.nr);sd_oled_text(0,16,text);
        paragraph(28,item->title,2,elapsed);struct tm due;localtime_r(&item->when,&due);strftime(text,sizeof(text),"%d %b %H:%M",&due);sd_oled_text(0,46,text);break;
    }
    case 2:{
        heading(agenda>=0?(sd.calendar[agenda].when<=now?"HAPPENING NOW":"UP NEXT"):"TODAY / AGENDA");
        if(agenda<0){sd_oled_text_scaled(2,20,sd.nc?"DAY COMPLETE":"OPEN SPACE",2);sd_oled_text(0,43,sd.nc?"NO UPCOMING EVENTS":"RUN IPHONE SYNC");break;}
        int index=agenda;const sd_item *item=&sd.calendar[index];struct tm e;localtime_r(&item->when,&e);
        if(item->all_day)snprintf(text,sizeof(text),"ALL DAY");else {
            char start[8],end[8];strftime(start,sizeof(start),"%H:%M",&e);
            if(item->end>item->when){struct tm finish;localtime_r(&item->end,&finish);strftime(end,sizeof(end),"%H:%M",&finish);snprintf(text,sizeof(text),"%s-%s",start,end);}else snprintf(text,sizeof(text),"%s",start);
        }
        sd_oled_text_scaled(0,16,text,2);snprintf(text,sizeof(text),"%d/%d",index+1,sd.nc);sd_oled_text(94,17,text);
        paragraph(31,item->title,2,elapsed);break;
    }
    case 3:
        heading("IPHONE BATTERY");
        if(sd.battery<0){sd_oled_text_scaled(2,20,"NO SYNC",2);sd_oled_text(0,42,"RUN SCRIPTABLE SYNC");}
        else{snprintf(text,sizeof(text),"%d%%",sd.battery);sd_oled_text_scaled(5,17,text,4);
            sd_oled_rect(0,42,126,5,false);unsigned phase=elapsed<550?elapsed:550;sd_oled_rect(1,43,sd.battery*124*(int)phase/55000,3,true);
            if(sd.charging_known&&sd.charging&&sd.battery<100){int x=1+sd.battery*124/100;int available=125-x;int pulse=available?(int)(ms/70)%(available+1):0;sd_oled_rect(x,43,pulse,3,true);}
            sd_oled_text(0,48,!sd.battery_sync||now-sd.battery_sync>3600?"CACHED READING":sd.charging_known?(sd.charging?"CHARGING":"ON BATTERY"):"LAST IPHONE REPORT");}
        break;
    case 4:
        heading("NOTIFICATIONS");
        if(ble.count){int index=selected(n,4,ble.count,tick);const sd_ble_notification *item=&ble.items[index];marquee(16,item->text.title[0]?item->text.title:app_name(item),elapsed);paragraph(28,item->text.message[0]?item->text.message:"PREVIEW NOT SHARED",2,elapsed);snprintf(text,sizeof(text),"%d/%d",index+1,ble.count);sd_oled_text(100,47,text);}
        else{sd_oled_text_scaled(2,20,"ALL QUIET",2);sd_oled_text(0,42,ble.ready?"CHECK PHONE SHARING":"CONNECT BLUETOOTH");}
        break;
    default:
        heading("YOUR COMPANION");sd_oled_text_scaled(0,17,sd_wifi_up?"ONLINE":"OFFLINE",2);
        sd_oled_text(0,34,ble.ready?"IPHONE CONNECTED":"PAIR IPHONE VIA BLE");sd_oled_text(0,45,"HOLD BOTH: AUTO");break;
    }
    sd_oled_rect(0,55,128,1,true);sd_oled_text(0,57,n->manual?"MANUAL":sd_wifi_up?"ONLINE":"OFFLINE");
    for(int i=0;i<6;i++)sd_oled_rect(80+i*8,59,4,4,i==page);
    sd_oled_present(scene);
}
