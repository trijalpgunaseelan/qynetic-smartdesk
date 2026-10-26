#include "navigation.h"
#include <assert.h>
#include <stdio.h>
static void tap(sd_navigation *n,int64_t time,bool next) {
 sd_nav_poll(n,time,!next,next);sd_nav_poll(n,time+50,!next,next);
 sd_nav_poll(n,time+100,false,false);sd_nav_poll(n,time+150,false,false);
}
int main(void) {
 sd_navigation n;sd_nav_init(&n,0);
 sd_nav_poll(&n,2999,false,false);assert(n.page==0);
 sd_nav_poll(&n,3000,false,false);assert(n.page==1&&!n.manual);
 tap(&n,8100,true);assert(n.page==2&&n.manual);
 sd_nav_poll(&n,20000,false,false);assert(n.page==2);
 tap(&n,20100,false);assert(n.page==1);
 sd_nav_poll(&n,21000,false,true);sd_nav_poll(&n,21050,false,true);sd_nav_poll(&n,21650,false,true);
 assert(n.selection[1]==1);sd_nav_poll(&n,21700,false,false);sd_nav_poll(&n,21750,false,false);assert(n.page==1);
 sd_nav_poll(&n,22000,true,true);sd_nav_poll(&n,22050,true,true);sd_nav_poll(&n,23050,true,true);assert(!n.manual);
 sd_nav_poll(&n,23100,false,false);sd_nav_poll(&n,23150,false,false);assert(n.page==1);
 sd_nav_poll(&n,32000,false,false);assert(n.page==2);
 // Contact bounce must not count as a press.
 sd_nav_init(&n,0);sd_nav_poll(&n,10,false,true);sd_nav_poll(&n,20,false,false);sd_nav_poll(&n,80,false,false);assert(n.page==0&&!n.manual);
 tap(&n,100,false);assert(n.page==5);tap(&n,300,true);assert(n.page==0);
 sd_nav_init(&n,0);
 sd_nav_poll_boot(&n,100,true);sd_nav_poll_boot(&n,150,true);sd_nav_poll_boot(&n,250,false);sd_nav_poll_boot(&n,300,false);assert(n.page==1&&n.manual);
 sd_nav_poll_boot(&n,500,true);sd_nav_poll_boot(&n,550,true);sd_nav_poll_boot(&n,1250,false);sd_nav_poll_boot(&n,1300,false);assert(n.page==0&&n.manual);
 sd_nav_poll_boot(&n,2000,true);sd_nav_poll_boot(&n,2050,true);sd_nav_poll_boot(&n,4200,false);sd_nav_poll_boot(&n,4250,false);assert(n.page==0&&!n.manual);
 sd_nav_poll_boot(&n,7250,false);assert(n.page==1);
 puts("PASS: page wrap, debounce, long press, manual pause, two-button resume and automatic timing");
}
