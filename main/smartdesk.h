#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"
#define SD_MAX_ITEMS 5
#define SD_MAX_EVENTS 16
#define SD_TITLE 64
#define SD_ID 48
#define SD_BODY_MAX 8192
typedef struct {char id[SD_ID];char title[SD_TITLE];time_t when,end;bool completed;bool all_day;} sd_item;
typedef struct {sd_item reminders[SD_MAX_ITEMS];int nr;sd_item calendar[SD_MAX_EVENTS];int nc;int battery;bool charging,charging_known;time_t battery_sync,reminder_sync,calendar_sync;char notification[128];time_t notification_until;} sd_state;
extern sd_state sd;
void sd_model_init(void);
esp_err_t sd_update(const char *kind,const char *json);
void sd_draw(void);
int sd_agenda_current(const sd_item *items,int count,time_t now);
esp_err_t sd_oled_init(void);
void sd_oled_clear(void);
void sd_oled_text(int x,int y,const char *text);
void sd_oled_flush(void);
void sd_oled_present(int scene);
void sd_oled_text_scaled(int x,int y,const char *text,int scale);
void sd_oled_rect(int x,int y,int width,int height,bool fill);
void sd_wifi_start(void);
void sd_api_start(void);
extern volatile bool sd_wifi_up;
