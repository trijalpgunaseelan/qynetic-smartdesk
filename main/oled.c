#include "smartdesk.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
static i2c_master_bus_handle_t bus;static i2c_master_dev_handle_t dev;static uint8_t fb[1024];static bool ready;
static esp_err_t cmd(uint8_t c){uint8_t b[]={0,c};return i2c_master_transmit(dev,b,2,100);}
esp_err_t sd_oled_init(void){i2c_master_bus_config_t cfg={.clk_source=I2C_CLK_SRC_DEFAULT,.i2c_port=I2C_NUM_0,.sda_io_num=GPIO_NUM_6,.scl_io_num=GPIO_NUM_7,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};esp_err_t e=i2c_new_master_bus(&cfg,&bus);if(e!=ESP_OK)return e;i2c_device_config_t dc={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=0x3c,.scl_speed_hz=400000};e=i2c_master_bus_add_device(bus,&dc,&dev);if(e!=ESP_OK)return e;e=i2c_master_probe(bus,0x3c,100);if(e!=ESP_OK){ESP_LOGE("oled","OLED missing at 0x3C: check power, GPIO6/7, controller");return e;}uint8_t seq[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0,0x40,0x8D,0x14,0x20,0x02,0xA1,0xC8,0xDA,0x12,0x81,0x7F,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};for(size_t i=0;i<sizeof(seq);i++)if((e=cmd(seq[i]))!=ESP_OK)return e;ready=true;return ESP_OK;}
void sd_oled_clear(void){memset(fb,0,sizeof(fb));}
/* Compact deterministic 3x5 glyphs; unsupported characters rendered as '?' */
static const char *glyph(char c){switch(c){case '0':return "111101101101111";case '1':return "010110010010111";case '2':return "111001111100111";case '3':return "111001111001111";case '4':return "101101111001001";case '5':return "111100111001111";case '6':return "111100111101111";case '7':return "111001001001001";case '8':return "111101111101111";case '9':return "111101111001111";case 'A':return "010101111101101";case 'B':return "110101110101110";case 'C':return "011100100100011";case 'D':return "110101101101110";case 'E':return "111100110100111";case 'F':return "111100110100100";case 'G':return "011100101101011";case 'H':return "101101111101101";case 'I':return "111010010010111";case 'J':return "001001001101010";case 'K':return "101101110101101";case 'L':return "100100100100111";case 'M':return "101111111101101";case 'N':return "101111111111101";case 'O':return "111101101101111";case 'P':return "110101110100100";case 'Q':return "111101101111011";case 'R':return "110101110101101";case 'S':return "011100010001110";case 'T':return "111010010010010";case 'U':return "101101101101111";case 'V':return "101101101101010";case 'W':return "101101111111101";case 'X':return "101101010101101";case 'Y':return "101101010010010";case 'Z':return "111001010100111";case ':':return "000010000010000";case '.':return "000000000000010";case '-':return "000000111000000";case '/':return "001001010100100";case '%':return "101001010100101";case '!':return "010010010000010";case ' ':return "000000000000000";default:return "111001010000010";}}
static void pixel(int x,int y) {
    if(x>=0&&x<128&&y>=0&&y<64) fb[y/8*128+x]|=1<<(y%8);
}
void sd_oled_rect(int x,int y,int width,int height,bool fill) {
    for(int row=0;row<height;row++) for(int col=0;col<width;col++)
        if(fill||row==0||row==height-1||col==0||col==width-1) pixel(x+col,y+row);
}
void sd_oled_text_scaled(int x,int y,const char *s,int scale) {
    if(!s||scale<1||scale>5) return;
    while(*s&&x+3*scale<=128) {
        unsigned char c=(unsigned char)*s++;
        if(c>=128) { while(((unsigned char)*s&0xc0)==0x80) s++; c='?'; }
        if(c>='a'&&c<='z') c-=32;
        const char *g=glyph((char)c);
        for(int row=0;row<5;row++) for(int col=0;col<3;col++)
            if(g[row*3+col]=='1') sd_oled_rect(x+col*scale,y+row*scale,scale,scale,true);
        x+=4*scale;
    }
}
/* Five-by-seven body type: 21 readable characters across the panel. */
typedef struct {char c;uint8_t rows[7];} glyph7;
static const glyph7 body_font[]={
 {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}},{'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}},{'N',{17,25,25,21,19,19,17}},
 {'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
 {' ',{0,0,0,0,0,0,0}},{'.',{0,0,0,0,0,12,12}},
 {':',{0,12,12,0,12,12,0}},{'-',{0,0,0,31,0,0,0}},
 {'/',{1,2,2,4,8,8,16}},{'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}},{'%',{17,2,4,4,8,16,17}},
 {'+',{0,4,4,31,4,4,0}},{'(',{2,4,8,8,8,4,2}},
 {')',{8,4,2,2,2,4,8}},{'\'',{4,4,0,0,0,0,0}},
 {',',{0,0,0,0,4,4,8}},{'&',{12,18,20,8,21,18,13}}
};
void sd_oled_text(int x,int y,const char *s) {
    if(!s) return;
    while(*s&&x+5<=128) {
        unsigned char c=(unsigned char)*s++;
        if(c>=128){while(((unsigned char)*s&0xc0)==0x80)s++;c='?';}
        if(c>='a'&&c<='z')c-=32;
        const uint8_t *rows=NULL;
        for(size_t i=0;i<sizeof(body_font)/sizeof(*body_font);i++) if(body_font[i].c==c){rows=body_font[i].rows;break;}
        if(!rows)rows=body_font[42].rows;
        for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(rows[row]&(1<<(4-col)))pixel(x+col,y+row);
        x+=6;
    }
}
static uint8_t displayed[1024];
void sd_oled_present(int scene) {
    static int previous=-1;
    static int64_t began;
    static uint8_t source[1024],target[1024];
    int64_t now=esp_timer_get_time();
    if(previous!=scene){memcpy(source,displayed,sizeof(source));began=now;previous=scene;}
    float t=(float)(now-began)/120000.0f;
    if(t<1.0f){
        memcpy(target,fb,sizeof(target));
        int offset=(int)(128*(1-(1-t)*(1-t)*(1-t)));
        for(int y=13;y<54;y++)for(int x=0;x<128;x++){
            int sx=x+offset;bool on;
            if(sx<128)on=(source[y/8*128+sx]>>(y%8))&1;
            else {sx-=128;on=(target[y/8*128+sx]>>(y%8))&1;}
            uint8_t mask=1<<(y%8);uint8_t *value=&fb[y/8*128+x];
            if(on)*value|=mask;else *value&=(uint8_t)~mask;
        }
    }
    sd_oled_flush();memcpy(displayed,fb,sizeof(displayed));
}
void sd_oled_flush(void){if(!ready)return;static uint8_t old[1024];static bool valid;if(valid&&!memcmp(old,fb,sizeof(fb)))return;for(int page=0;page<8;page++){if(valid&&!memcmp(old+page*128,fb+page*128,128))continue;cmd(0xB0|page);cmd(0);cmd(0x10);uint8_t data[129]={0x40};memcpy(data+1,fb+page*128,128);if(i2c_master_transmit(dev,data,sizeof(data),100)!=ESP_OK){ESP_LOGW("oled","I2C write failed");return;}}memcpy(old,fb,sizeof(fb));valid=true;}
