#include "smartdesk.h"
#include "bluetooth.h"
#include "navigation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
sd_state sd={.battery=-1};volatile bool sd_wifi_up=true;
static sd_navigation navigation;static sd_ble_state ble;static int scene;static int64_t milliseconds=100000;
int64_t esp_timer_get_time(void){return milliseconds*1000;}
const sd_navigation *sd_navigation_state(void){return &navigation;}
void sd_ble_snapshot(sd_ble_state *out){*out=ble;}
void sd_oled_clear(void){}
void sd_oled_present(int value){scene=value;}
void sd_oled_rect(int x,int y,int w,int h,bool fill){assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=128&&y+h<=64);}
void sd_oled_text(int x,int y,const char *text){assert(x>=0&&y>=0&&x+(int)strlen(text)*6-1<=128&&y+7<=64);}
void sd_oled_text_scaled(int x,int y,const char *text,int scale){assert(x>=0&&y>=0&&x+(int)strlen(text)*4*scale-scale<=128&&y+5*scale<=64);}
int main(void){
 for(int page=0;page<6;page++){navigation.page=page;sd_draw();}
 sd.nr=5;sd.nc=16;sd.battery=100;ble.count=5;
 for(int i=0;i<16;i++)snprintf(sd.calendar[i].title,SD_TITLE,"Calendar event number %d with a longer title",i);
 for(int i=0;i<5;i++){snprintf(sd.reminders[i].title,SD_TITLE,"Reminder %d",i);snprintf(ble.items[i].text.title,64,"Notification %d",i);}
 for(int page=0;page<6;page++){navigation.page=page;sd_draw();}
 navigation.manual=true;navigation.suppress_until=milliseconds+10000;
 ble.items[0].text.uid=123;ble.overlay_until=(milliseconds+18001)*1000;sd_draw();assert(scene==2000123);
 // An alert that predates the last button press remains dismissed.
 ble.overlay_until=(milliseconds+17000)*1000;sd_draw();assert(scene<2000000);
 // New HTTP test messages must also override a paused/manual page.
 sd.notification_until=time(NULL)+10;strcpy(sd.notification,"A display test");milliseconds++;sd_draw();assert(scene==10000002);
 ble.pairing=true;sd_draw();assert(scene==10000001);
 puts("PASS: OLED text/geometry bounds, all pages, new alert visibility after navigation, dismissal and pairing priority");
}
