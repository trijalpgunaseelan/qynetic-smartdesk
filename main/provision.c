#include "nvs.h"
#include "bluetooth.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
esp_err_t sd_wifi_scan(void);
static char ssid[33],pass[65],token[65];
static bool ready_ssid,ready_pass,ready_token;
static void provision(void *unused){
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
 if(!usb_serial_jtag_is_driver_installed()) {
  usb_serial_jtag_driver_config_t config=USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  config.rx_buffer_size=1024;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
 }
#else
 ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,1024,0,0,NULL,0));
#endif
 char line[130];int n=0;bool overflow=false;
 ESP_LOGW("provision","USB serial provisioning active. Never share serial logs containing credentials.");
 for(;;){uint8_t c;int r;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED
 r=usb_serial_jtag_read_bytes(&c,1,pdMS_TO_TICKS(250));
#else
 r=uart_read_bytes(UART_NUM_0,&c,1,pdMS_TO_TICKS(250));
#endif
 if(r<=0)continue;
 if(c=='\r')continue;
 if(c!='\n'){if(n<(int)sizeof(line)-1&&!overflow)line[n++]=(char)c;else overflow=true;continue;}
 if(overflow){memset(line,0,sizeof(line));n=0;overflow=false;ESP_LOGW("provision","Command too long");continue;}
 line[n]=0;n=0;
 if(!strncmp(line,"SSID ",5)&&strlen(line+5)<=32&&line[5]){strcpy(ssid,line+5);ready_ssid=true;}
 else if(!strncmp(line,"PASS ",5)&&strlen(line+5)>=8&&strlen(line+5)<=63){strcpy(pass,line+5);ready_pass=true;}
 else if(!strncmp(line,"TOKEN ",6)&&strlen(line+6)>=32&&strlen(line+6)<=64){strcpy(token,line+6);ready_token=true;}
 else if(!strcmp(line,"SAVE")&&ready_ssid&&ready_pass&&ready_token){nvs_handle_t h;if(nvs_open("smartdesk",NVS_READWRITE,&h)==ESP_OK){esp_err_t e=nvs_set_str(h,"ssid",ssid);if(e==ESP_OK)e=nvs_set_str(h,"password",pass);if(e==ESP_OK)e=nvs_set_str(h,"token",token);if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);ESP_LOGI("provision","Provisioning %s",e==ESP_OK?"saved; reboot":"failed");}memset(pass,0,sizeof(pass));memset(token,0,sizeof(token));ready_pass=ready_token=false;}
 else if(!strcmp(line,"SCAN")) ESP_LOGI("provision","Wi-Fi scan %s",esp_err_to_name(sd_wifi_scan()));
 else if(!strcmp(line,"REBOOT")){ESP_LOGI("provision","Rebooting");vTaskDelay(pdMS_TO_TICKS(200));esp_restart();}
 else if(!strcmp(line,"UNPAIR")) ESP_LOGI("provision","Bond reset %s",sd_ble_forget_bonds()==ESP_OK?"queued":"unavailable");
 else ESP_LOGW("provision","Invalid provisioning command");
 memset(line,0,sizeof(line));
 }
}
void sd_provision_start(void){xTaskCreate(provision,"provision",3072,NULL,3,NULL);}
