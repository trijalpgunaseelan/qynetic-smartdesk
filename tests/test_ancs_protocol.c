#include "ancs_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t packet[5000];static size_t length;
static void header(uint32_t uid) {length=0;packet[length++]=0;for(int i=0;i<4;i++) packet[length++]=(uint8_t)(uid>>(i*8));}
static void attribute(uint8_t id,const void *bytes,size_t n) {
    packet[length++]=id;packet[length++]=(uint8_t)n;packet[length++]=(uint8_t)(n>>8);
    if(n)memcpy(packet+length,bytes,n);length+=n;
}
static void normal(uint32_t uid) {
    header(uid);attribute(0,"com.apple.MobileSMS",19);attribute(1,"Hello \xe2\x98\x80",9);
    attribute(3,"Notification preview",20);attribute(6,"Open",4);attribute(7,"Dismiss",7);
}
static void check_value(const sd_ancs_parser *p) {
    assert(p->value.uid==0x78563412);assert(!strcmp(p->value.app,"com.apple.MobileSMS"));
    assert(!strcmp(p->value.title,"Hello \xe2\x98\x80"));assert(!strcmp(p->value.message,"Notification preview"));
    assert(!strcmp(p->value.positive,"Open"));assert(!strcmp(p->value.negative,"Dismiss"));
}
int main(void) {
    uint8_t request[14];assert(sd_ancs_request(0x78563412,request)==14);
    const uint8_t expected[]={0,0x12,0x34,0x56,0x78,0,1,63,0,3,127,0,6,7};
    assert(!memcmp(request,expected,sizeof(expected)));
    normal(0x78563412);sd_ancs_parser p;
    /* Every possible two-fragment split, plus single-byte ATT fragments. */
    for(size_t split=0;split<=length;split++) {
        sd_ancs_parser_init(&p,0x78563412);
        int first=sd_ancs_parser_feed(&p,packet,split);assert(first==(split==length?1:0));
        assert(sd_ancs_parser_feed(&p,packet+split,length-split)==1);check_value(&p);
    }
    sd_ancs_parser_init(&p,0x78563412);
    for(size_t i=0;i<length;i++)assert(sd_ancs_parser_feed(&p,packet+i,1)==(i==length-1?1:0));check_value(&p);
    /* Wrong command, UID, tuple order, and bytes after a completed response. */
    sd_ancs_parser_init(&p,123);assert(sd_ancs_parser_feed(&p,packet,length)==-1);
    packet[0]=1;sd_ancs_parser_init(&p,0x78563412);assert(sd_ancs_parser_feed(&p,packet,length)==-1);normal(0x78563412);
    packet[5]=1;sd_ancs_parser_init(&p,0x78563412);assert(sd_ancs_parser_feed(&p,packet,length)==-1);normal(0x78563412);
    sd_ancs_parser_init(&p,0x78563412);assert(sd_ancs_parser_feed(&p,packet,length)==1);assert(sd_ancs_parser_feed(&p,packet,1)==-1);
    /* Empty attributes still count toward completion. */
    header(7);attribute(0,NULL,0);attribute(1,NULL,0);attribute(3,NULL,0);attribute(6,NULL,0);attribute(7,NULL,0);
    sd_ancs_parser_init(&p,7);assert(sd_ancs_parser_feed(&p,packet,length)==1);assert(!p.value.title[0]);
    /* Oversize/malformed lengths are rejected before any buffer writes. */
    header(7);uint8_t bad[]={0,1,4};memcpy(packet+length,bad,3);length+=3;
    sd_ancs_parser_init(&p,7);assert(sd_ancs_parser_feed(&p,packet,length)==-1);
    header(7);attribute(0,NULL,0);uint8_t long_title[]={1,64,0};memcpy(packet+length,long_title,3);length+=3;
    sd_ancs_parser_init(&p,7);assert(sd_ancs_parser_feed(&p,packet,length)==-1);
    /* Consume long labels without truncating the tuple stream. Preserve UTF-8. */
    char long_app[70];memset(long_app,'a',61);memcpy(long_app+61,"\xe2\x98\x80",3);memset(long_app+64,'b',6);
    char label[1024];memset(label,'x',sizeof(label));header(8);attribute(0,long_app,sizeof(long_app));
    const char dirty[]={'A','\0','\n',(char)0xff,'B'};attribute(1,dirty,sizeof(dirty));attribute(3,NULL,0);attribute(6,label,sizeof(label));attribute(7,"Done",4);
    sd_ancs_parser_init(&p,8);
    for(size_t pos=0;pos<length;) {size_t n=13;if(n>length-pos)n=length-pos;int rc=sd_ancs_parser_feed(&p,packet+pos,n);pos+=n;assert(rc==(pos==length?1:0));}
    assert(strlen(p.value.app)==61);assert(!strcmp(p.value.title,"A  ?B"));assert(strlen(p.value.positive)==31);assert(!strcmp(p.value.negative,"Done"));
    /* Deterministic malformed-input stress under ASan/UBSan. */
    uint32_t seed=42;
    for(int trial=0;trial<10000;trial++) {
        sd_ancs_parser_init(&p,(uint32_t)trial);uint8_t random[128];
        for(size_t i=0;i<sizeof(random);i++){seed=seed*1664525+1013904223;random[i]=(uint8_t)(seed>>24);}
        (void)sd_ancs_parser_feed(&p,random,sizeof(random));
    }
    puts("PASS: ANCS encoding, all fragment boundaries, UTF-8, bounds, malformed responses and 10,000 stress inputs");
}
