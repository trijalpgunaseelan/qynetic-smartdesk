#include "navigation.h"
#include <string.h>
void sd_nav_init(sd_navigation *n,int64_t now) {memset(n,0,sizeof(*n));n->auto_at=now+SD_PAGE_INTERVAL_MS;}
void sd_nav_poll(sd_navigation *n,int64_t now,bool previous,bool next) {
    bool inputs[2]={previous,next};
    for(int i=0;i<2;i++) {
        if(inputs[i]!=n->raw[i]) {n->raw[i]=inputs[i];n->changed[i]=now;}
        if(now-n->changed[i]>=40&&n->stable[i]!=n->raw[i]) {
            n->stable[i]=n->raw[i];
            if(n->stable[i]) {n->pressed[i]=now;n->repeated[i]=now;n->long_sent[i]=false;}
            else if(!n->chord&&!n->long_sent[i]) {
                n->page=(n->page+(i?1:SD_PAGE_COUNT-1))%SD_PAGE_COUNT;
                n->manual=true;n->suppress_until=now+10000;
            }
        }
    }
    if(n->stable[0]&&n->stable[1]) {
        n->chord=true;
        int64_t last=n->pressed[0]>n->pressed[1]?n->pressed[0]:n->pressed[1];
        if(now-last>=1000) {n->manual=false;n->auto_at=now+SD_PAGE_INTERVAL_MS;n->suppress_until=now+10000;}
    }else if(n->chord) {
        if(!n->stable[0]&&!n->stable[1]) n->chord=false;
    }else for(int i=0;i<2;i++) if(n->stable[i]&&now-n->pressed[i]>=600&&(!n->long_sent[i]||now-n->repeated[i]>=300)) {
        n->selection[n->page]+=i?1:-1;n->manual=true;n->long_sent[i]=true;n->repeated[i]=now;n->suppress_until=now+10000;
    }
    if(!n->manual&&!n->raw[0]&&!n->raw[1]&&!n->stable[0]&&!n->stable[1]&&now>=n->auto_at) {n->page=(n->page+1)%SD_PAGE_COUNT;n->auto_at=now+SD_PAGE_INTERVAL_MS;}
}

void sd_nav_poll_boot(sd_navigation *n,int64_t now,bool pressed) {
    if(pressed!=n->raw[0]){n->raw[0]=pressed;n->changed[0]=now;}
    if(now-n->changed[0]>=40&&n->stable[0]!=n->raw[0]) {
        n->stable[0]=n->raw[0];
        if(pressed)n->pressed[0]=now;
        else {
            int64_t held=now-n->pressed[0];
            if(held>=2000){n->manual=false;n->auto_at=now+SD_PAGE_INTERVAL_MS;}
            else {n->page=(n->page+(held>=600?SD_PAGE_COUNT-1:1))%SD_PAGE_COUNT;n->manual=true;}
            n->suppress_until=now+10000;
        }
    }
    if(!n->manual&&!n->raw[0]&&!n->stable[0]&&now>=n->auto_at){n->page=(n->page+1)%SD_PAGE_COUNT;n->auto_at=now+SD_PAGE_INTERVAL_MS;}
}
