#include "smartdesk.h"
#include "bluetooth.h"
#include "navigation.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void screen(void *arg) {
    sd_buttons_init();
    int64_t next_draw=0;
    while(1) {
        int64_t now=esp_timer_get_time()/1000;
        sd_buttons_poll(now);
        if(now>=next_draw) {sd_draw();next_draw=now+50;}
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void sd_provision_start(void);
void app_main(void){esp_err_t e=nvs_flash_init();if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();}ESP_ERROR_CHECK(e);setenv("TZ","IST-5:30",1);tzset();sd_model_init();e=sd_oled_init();if(e!=ESP_OK)ESP_LOGE("smartdesk","OLED unavailable (%s); continuing headless",esp_err_to_name(e));sd_oled_clear();sd_oled_text(0,12,"QYNETIC SMARTDESK");sd_oled_text(0,28,"BOOTING");sd_oled_flush();sd_provision_start();e=sd_ble_start();if(e!=ESP_OK)ESP_LOGE("smartdesk","Bluetooth unavailable (%s)",esp_err_to_name(e));sd_wifi_start();sd_api_start();xTaskCreate(screen,"screen",4096,NULL,4,NULL);}
