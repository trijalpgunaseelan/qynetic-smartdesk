#include "smartdesk.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 sd_item items[5]={{.when=100,.end=200},{.when=200,.end=300},{.when=50,.end=400,.all_day=true}};
 assert(sd_agenda_current(items,3,150)==0);
 assert(sd_agenda_current(items,3,200)==1); // end-exclusive, next starts immediately
 assert(sd_agenda_current(items,3,350)==2); // all-day after timed meetings
 assert(sd_agenda_current(items,3,400)==-1);
 assert(sd_agenda_current(items,3,75)==2);
 assert(sd_agenda_current(items,2,75)==0);
 items[3]=(sd_item){.when=125,.end=175};assert(sd_agenda_current(items,4,150)==3); // overlapping latest start
 items[0].end=0;assert(sd_agenda_current(items,1,150)==-1); // never invent an old event's duration
 assert(sd_agenda_current(items,0,150)==-1);
 puts("PASS: current event, end boundary, next event, all-day fallback, overlaps, missing duration and empty agenda");
}
