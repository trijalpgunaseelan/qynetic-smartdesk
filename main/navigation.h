#pragma once
#include <stdbool.h>
#include <stdint.h>
#define SD_PAGE_COUNT 6
#define SD_PAGE_INTERVAL_MS 3000
typedef struct {
    int page, selection[SD_PAGE_COUNT];
    bool manual, raw[2], stable[2], long_sent[2], chord;
    int64_t changed[2], pressed[2], repeated[2], auto_at, suppress_until;
} sd_navigation;
void sd_nav_init(sd_navigation *n,int64_t now);
void sd_nav_poll(sd_navigation *n,int64_t now,bool previous,bool next);
void sd_nav_poll_boot(sd_navigation *n,int64_t now,bool pressed);
void sd_buttons_init(void);
void sd_buttons_poll(int64_t now);
const sd_navigation *sd_navigation_state(void);
