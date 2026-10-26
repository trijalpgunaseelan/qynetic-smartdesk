#include "smartdesk.h"
/* Prefer a current timed event, then an active all-day event, then next start.
   The end boundary is exclusive: at 11:00, a 10:00–11:00 event is finished. */
int sd_agenda_current(const sd_item *items,int count,time_t now) {
    int active=-1,all_day=-1,next=-1;
    for(int i=0;i<count;i++) {
        const sd_item *e=&items[i];
        bool running=e->end>e->when&&e->when<=now&&now<e->end;
        if(!e->all_day&&running&&(active<0||e->when>items[active].when))active=i;
        if(e->all_day) {
            if(!e->end){struct tm start,today;localtime_r(&e->when,&start);localtime_r(&now,&today);running=start.tm_year==today.tm_year&&start.tm_yday==today.tm_yday;}
            if(running&&all_day<0)all_day=i;
        }
        if(e->when>now&&(next<0||e->when<items[next].when))next=i;
    }
    return active>=0?active:all_day>=0?all_day:next;
}
