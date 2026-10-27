#include "smartdesk.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
volatile bool sd_wifi_up=false;
volatile bool sd_wifi_scan_active=false;static int attempts;static esp_timer_handle_t retry;
/* Retry indefinitely, including when esp_wifi_connect itself fails. */
static void schedule_retry(void) {
    if(sd_wifi_scan_active) return;
    unsigned seconds=1u<<(attempts<3?attempts++:3);
    esp_timer_stop(retry);
    esp_timer_start_once(retry,(uint64_t)seconds*1000000);
}
static void connect_cb(void *arg) {
    if(sd_wifi_scan_active||sd_wifi_up) return;
    esp_err_t err=esp_wifi_connect();
    if(err!=ESP_OK) {
        ESP_LOGW("wifi","Connect could not start (%s); retrying",esp_err_to_name(err));
        schedule_retry();
    }
}
static void handler(void *a,esp_event_base_t base,int32_t id,void *data) {
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED) {
        sd_wifi_up=false;
        const wifi_event_sta_disconnected_t *event=data;
        ESP_LOGW("wifi","Connection lost/failed (reason %u); retrying automatically",event->reason);
        schedule_retry();
    }else if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP) {
        sd_wifi_up=true;attempts=0;esp_timer_stop(retry);
        /* System time keeps running if Wi-Fi drops. Reconnection must not
           reconfigure an already-running SNTP client. */
        if(!esp_sntp_enabled()) {
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0,"pool.ntp.org");
            esp_sntp_init();
        }
        ESP_LOGI("wifi","Connected; clock continues independently of Wi-Fi");
    }
}
void sd_wifi_start(void){esp_netif_init();esp_event_loop_create_default();esp_netif_create_default_wifi_sta();wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&c));esp_timer_create_args_t args={.callback=connect_cb,.name="wifi_retry"};ESP_ERROR_CHECK(esp_timer_create(&args,&retry));esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,handler,NULL);esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,handler,NULL);wifi_config_t config={0};nvs_handle_t n;if(nvs_open("smartdesk",NVS_READONLY,&n)==ESP_OK){size_t z=sizeof(config.sta.ssid);nvs_get_str(n,"ssid",(char*)config.sta.ssid,&z);z=sizeof(config.sta.password);nvs_get_str(n,"password",(char*)config.sta.password,&z);nvs_close(n);}config.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK;ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&config));ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(CONFIG_SMARTDESK_WIFI_TX_POWER));ESP_LOGI("wifi","Transmit power cap set to %d quarter-dBm units",CONFIG_SMARTDESK_WIFI_TX_POWER);if(config.sta.ssid[0])connect_cb(NULL);else ESP_LOGW("wifi","No Wi-Fi configured; use serial provisioning script");}

/* Explicit serial diagnostic; pause retries while the station scans. */
esp_err_t sd_wifi_scan(void) {
    extern volatile bool sd_wifi_scan_active;
    if(sd_wifi_scan_active) return ESP_ERR_INVALID_STATE;
    sd_wifi_scan_active=true;
    esp_timer_stop(retry);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(150));
    wifi_scan_config_t config={.show_hidden=true};
    esp_err_t result=esp_wifi_scan_start(&config,true);
    if(result==ESP_OK) {
        uint16_t count=16;
        wifi_ap_record_t *records=calloc(count,sizeof(*records));
        if(records) {
            result=esp_wifi_scan_get_ap_records(&count,records);
            if(result==ESP_OK) {
                ESP_LOGI("wifi","Scan found %u networks (maximum 16 reported)",count);
                for(unsigned i=0;i<count;i++) ESP_LOGI("wifi","Scan SSID: %.*s | channel %u | signal %d dBm",32,(char *)records[i].ssid,records[i].primary,records[i].rssi);
            }
            free(records);
        }else {esp_wifi_clear_ap_list();result=ESP_ERR_NO_MEM;}
    }
    sd_wifi_scan_active=false;
    connect_cb(NULL);
    return result;
}
