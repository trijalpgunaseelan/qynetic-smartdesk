#include "ancs_protocol.h"
#include <string.h>

static const uint8_t ids[]={0,1,3,6,7};
static char *destination(sd_ancs_parser *p, size_t *capacity) {
    switch(p->id) {
    case 0: *capacity=sizeof(p->value.app); return p->value.app;
    case 1: *capacity=sizeof(p->value.title); return p->value.title;
    case 3: *capacity=sizeof(p->value.message); return p->value.message;
    case 6: *capacity=sizeof(p->value.positive); return p->value.positive;
    default:*capacity=sizeof(p->value.negative); return p->value.negative;
    }
}
/* Keep valid UTF-8, replacing controls and malformed bytes, trimming split tails. */
static void clean_text(char *text, size_t cap) {
    size_t i=0,n=strlen(text);
    while(i<n) {
        uint8_t c=(uint8_t)text[i];
        if(c<0x80) { if(c<32||c==127) text[i]=' '; i++; continue; }
        size_t width=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
        if(width&&i+width>n&&n==cap-1) {text[i]=0;break;}
        bool valid=width&&i+width<=n;
        for(size_t j=1;valid&&j<width;j++) valid=((uint8_t)text[i+j]&0xc0)==0x80;
        if(valid&&width>=3) {
            uint8_t second=(uint8_t)text[i+1];
            if((c==0xe0&&second<0xa0)||(c==0xed&&second>=0xa0)||
               (c==0xf0&&second<0x90)||(c==0xf4&&second>=0x90)) valid=false;
        }
        if(valid) i+=width; else text[i++]='?';
    }
}
void sd_ancs_parser_init(sd_ancs_parser *p,uint32_t uid) {
    memset(p,0,sizeof(*p));p->expected_uid=uid;
}
size_t sd_ancs_request(uint32_t uid,uint8_t out[14]) {
    const uint8_t tail[]={0,1,63,0,3,127,0,6,7};
    out[0]=0;for(int i=0;i<4;i++) out[i+1]=(uint8_t)(uid>>(i*8));
    memcpy(out+5,tail,sizeof(tail));return 14;
}
static void next_attribute(sd_ancs_parser *p) {
    size_t cap;char *text=destination(p,&cap);clean_text(text,cap);
    p->offset=0;p->length=0;p->phase=2;
    if(++p->attribute_index==sizeof(ids)) p->complete=true;
}
int sd_ancs_parser_feed(sd_ancs_parser *p,const uint8_t *data,size_t size) {
    if(p->failed||(!data&&size)) return -1;
    for(size_t i=0;i<size;i++) {
        uint8_t b=data[i];
        if(p->complete) goto invalid;
        switch(p->phase) {
        case 0: if(b!=0) goto invalid; p->phase=1; break;
        case 1:
            p->value.uid|=(uint32_t)b<<(8*p->header_bytes++);
            if(p->header_bytes==4) { if(p->value.uid!=p->expected_uid) goto invalid; p->phase=2; }
            break;
        case 2: if(b!=ids[p->attribute_index]) goto invalid; p->id=b;p->phase=3;break;
        case 3: p->length=b;p->phase=4;break;
        case 4:
            p->length|=(uint16_t)b<<8;
            if(p->length>1024||(p->id==1&&p->length>63)||(p->id==3&&p->length>127)) goto invalid;
            if(p->length) p->phase=5; else next_attribute(p);
            break;
        case 5: {
            size_t cap;char *text=destination(p,&cap);
            if(p->offset<cap-1) text[p->offset]=b?(char)b:' ';
            if(++p->offset==p->length) next_attribute(p);
            break;
        }
        default: goto invalid;
        }
    }
    return p->complete?1:0;
invalid:
    p->failed=true;return -1;
}
