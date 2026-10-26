#include "smartdesk.h"
#include "bluetooth.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static char token[65];
static esp_err_t reply(httpd_req_t*r,int status,const char *msg){httpd_resp_set_type(r,"application/json");if(status==202)httpd_resp_set_status(r,"202 Accepted");if(status==409)httpd_resp_set_status(r,"409 Conflict");if(status==400)httpd_resp_set_status(r,"400 Bad Request");if(status==401)httpd_resp_set_status(r,"401 Unauthorized");if(status==413)httpd_resp_set_status(r,"413 Payload Too Large");if(status==503)httpd_resp_set_status(r,"503 Service Unavailable");return httpd_resp_sendstr(r,msg);}
static bool authorized(httpd_req_t*r){char h[96]={0};if(httpd_req_get_hdr_value_str(r,"Authorization",h,sizeof(h))!=ESP_OK||strncmp(h,"Bearer ",7)||!token[0])return false;size_t n=strlen(token);if(strlen(h+7)!=n)return false;unsigned diff=0;for(size_t i=0;i<n;i++)diff|=(unsigned char)(h[7+i]^token[i]);return diff==0;}
static esp_err_t post_body(httpd_req_t*r,const char *b){const char *kind=strrchr(r->uri,'/');if(!kind)return reply(r,400,"{\"error\":\"path\"}");kind++;if(!strcmp(kind,"notification-action")) {
    cJSON *root=cJSON_Parse(b);
    if(!root) return reply(r,400,"{\"error\":\"invalid action\"}");
    cJSON *uid=cJSON_GetObjectItemCaseSensitive(root,"uid"),*action=cJSON_GetObjectItemCaseSensitive(root,"action"),*generation=cJSON_GetObjectItemCaseSensitive(root,"session");
    bool valid=cJSON_IsNumber(uid)&&uid->valuedouble>=0&&uid->valuedouble<=UINT32_MAX&&uid->valuedouble==(double)(uint32_t)uid->valuedouble&&
        cJSON_IsNumber(action)&&(action->valuedouble==0||action->valuedouble==1)&&
        cJSON_IsNumber(generation)&&generation->valuedouble>=0&&generation->valuedouble<=UINT32_MAX&&generation->valuedouble==(double)(uint32_t)generation->valuedouble;
    esp_err_t e=valid?sd_ble_action((uint32_t)uid->valuedouble,(uint8_t)action->valueint,(uint32_t)generation->valuedouble):ESP_ERR_INVALID_ARG;
    cJSON_Delete(root);
    return reply(r,e==ESP_OK?202:e==ESP_ERR_INVALID_ARG?400:409,e==ESP_OK?"{\"queued\":true}":"{\"error\":\"action unavailable or notification expired\"}");
}if(!strcmp(kind,"config"))return reply(r,400,"{\"error\":\"configure via serial provisioning\"}");esp_err_t e=sd_update(kind,b);return reply(r,e==ESP_OK?200:400,e==ESP_OK?"{\"ok\":true}":"{\"error\":\"invalid JSON or fields\"}");}
static esp_err_t post(httpd_req_t *r) {
    if(!authorized(r)) return reply(r,401,"{\"error\":\"unauthorized\"}");
    if(r->content_len<1||r->content_len>SD_BODY_MAX) return reply(r,413,"{\"error\":\"payload size\"}");
    char *body=malloc((size_t)r->content_len+1);
    if(!body) return reply(r,503,"{\"error\":\"out of memory\"}");
    int offset=0;
    while(offset<r->content_len) {
        int n=httpd_req_recv(r,body+offset,r->content_len-offset);
        if(n<=0) {free(body);return reply(r,400,"{\"error\":\"receive failed\"}");}
        offset+=n;
    }
    body[offset]=0;esp_err_t result=post_body(r,body);free(body);return result;
}
extern const uint8_t sync_shortcut_start[] asm("_binary_smartdesk_sync_shortcut_start");
extern const uint8_t sync_shortcut_end[] asm("_binary_smartdesk_sync_shortcut_end");
extern const uint8_t battery_shortcut_start[] asm("_binary_smartdesk_battery_shortcut_start");
extern const uint8_t battery_shortcut_end[] asm("_binary_smartdesk_battery_shortcut_end");
static esp_err_t download_shortcut(httpd_req_t *r) {
    bool battery=!strcmp(r->uri,"/smartdesk-battery.shortcut");
    const uint8_t *start=battery?battery_shortcut_start:sync_shortcut_start;
    const uint8_t *end=battery?battery_shortcut_end:sync_shortcut_end;
    httpd_resp_set_type(r,"application/octet-stream");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Disposition",battery?"attachment; filename=\"SmartDesk Battery Sync.shortcut\"":"attachment; filename=\"SmartDesk Auto Sync.shortcut\"");
    return httpd_resp_send(r,(const char *)start,end-start);
}
extern const char iphone_sync_start[] asm("_binary_iphone_sync_js_start");
extern const char iphone_sync_end[] asm("_binary_iphone_sync_js_end");
static esp_err_t iphone_sync(httpd_req_t *r) {
    httpd_resp_set_type(r,"application/javascript; charset=utf-8");
    httpd_resp_set_hdr(r,"Content-Disposition","attachment; filename=SmartDeskSync.js");
    return httpd_resp_send(r,iphone_sync_start,iphone_sync_end-iphone_sync_start-1);
}
/* Embedded dashboard is public; device data still requires the bearer token. */
extern const char dashboard_start[] asm("_binary_index_html_start");
extern const char dashboard_end[] asm("_binary_index_html_end");
static esp_err_t dashboard(httpd_req_t *r) {
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'");
    return httpd_resp_send(r, dashboard_start, dashboard_end-dashboard_start-1);
}
static bool add_items(cJSON *root, const char *key, const sd_item *items, int count) {
    cJSON *array=cJSON_AddArrayToObject(root,key);
    if (!array) return false;
    for (int i=0;i<count;i++) {
        cJSON *item=cJSON_CreateObject();
        if (!item) return false;
        if (!cJSON_AddItemToArray(array,item)) { cJSON_Delete(item); return false; }
        if (!cJSON_AddStringToObject(item,"id",items[i].id) ||
            !cJSON_AddStringToObject(item,"title",items[i].title) ||
            !cJSON_AddNumberToObject(item,"when",(double)items[i].when) ||
            !cJSON_AddNumberToObject(item,"end",(double)items[i].end) ||
            !cJSON_AddBoolToObject(item,"completed",items[i].completed) ||
            !cJSON_AddBoolToObject(item,"all_day",items[i].all_day)) return false;
    }
    return true;
}
static bool add_bluetooth(cJSON *root) {
    /* Heap allocation keeps the HTTP task's stack bounded. */
    sd_ble_state *state=malloc(sizeof(*state));
    if(!state) return false;
    sd_ble_snapshot(state);
    cJSON *ble=cJSON_AddObjectToObject(root,"bluetooth");
    bool ok=ble&&cJSON_AddStringToObject(ble,"status",state->status)&&
        cJSON_AddBoolToObject(ble,"connected",state->connected)&&
        cJSON_AddBoolToObject(ble,"encrypted",state->encrypted)&&
        cJSON_AddBoolToObject(ble,"bonded",state->bonded)&&
        cJSON_AddBoolToObject(ble,"ready",state->ready)&&
        cJSON_AddBoolToObject(ble,"details",state->details)&&
        cJSON_AddBoolToObject(ble,"pairing",state->pairing)&&
        cJSON_AddNumberToObject(ble,"error",state->error)&&
        cJSON_AddNumberToObject(ble,"session",state->session)&&
        cJSON_AddNumberToObject(ble,"dropped",state->dropped)&&
        cJSON_AddNumberToObject(ble,"source_events",state->source_events)&&
        cJSON_AddNumberToObject(ble,"data_bytes",state->data_bytes)&&
        cJSON_AddNumberToObject(ble,"decoded",state->decoded)&&
        cJSON_AddNumberToObject(ble,"last_error",state->last_error)&&
        cJSON_AddStringToObject(ble,"action_status",state->action_status)&&
        cJSON_AddNumberToObject(ble,"action_error",state->action_error);
    if(ok&&state->pairing) ok=cJSON_AddNumberToObject(ble,"passkey",state->passkey)!=NULL;
    cJSON *items=ok?cJSON_AddArrayToObject(ble,"notifications"):NULL;
    ok=items!=NULL;
    for(int i=0;ok&&i<state->count;i++) {
        const sd_ble_notification *n=&state->items[i];
        cJSON *item=cJSON_CreateObject();
        if(!item) {ok=false;break;}
        if(!cJSON_AddItemToArray(items,item)) {cJSON_Delete(item);ok=false;break;}
        ok=cJSON_AddNumberToObject(item,"uid",n->text.uid)&&
            cJSON_AddNumberToObject(item,"category",n->category)&&
            cJSON_AddNumberToObject(item,"flags",n->flags)&&
            cJSON_AddNumberToObject(item,"received_at",(double)n->received_at)&&
            cJSON_AddStringToObject(item,"app",n->text.app)&&
            cJSON_AddStringToObject(item,"title",n->text.title)&&
            cJSON_AddStringToObject(item,"message",n->text.message)&&
            cJSON_AddStringToObject(item,"positive",n->text.positive)&&
            cJSON_AddStringToObject(item,"negative",n->text.negative);
    }
    free(state);return ok;
}
static esp_err_t get(httpd_req_t *r) {
    if (!strcmp(r->uri,"/api/v1/health")) return reply(r,200,"{\"ok\":true}");
    if (!authorized(r)) return reply(r,401,"{\"error\":\"unauthorized\"}");
    cJSON *root=cJSON_CreateObject();
    if (!root) return reply(r,503,"{\"error\":\"out of memory\"}");
    bool ok=cJSON_AddBoolToObject(root,"ok",true) &&
        cJSON_AddBoolToObject(root,"wifi",sd_wifi_up) &&
        cJSON_AddNumberToObject(root,"battery",sd.battery) &&
        cJSON_AddBoolToObject(root,"charging",sd.charging) &&
        cJSON_AddBoolToObject(root,"charging_known",sd.charging_known) &&
        cJSON_AddNumberToObject(root,"battery_sync",(double)sd.battery_sync) &&
        cJSON_AddNumberToObject(root,"calendar_sync",(double)sd.calendar_sync) &&
        cJSON_AddNumberToObject(root,"reminders",sd.nr) &&
        cJSON_AddNumberToObject(root,"calendar",sd.nc) &&
        cJSON_AddNumberToObject(root,"epoch",(double)time(NULL)) &&
        add_items(root,"reminder_items",sd.reminders,sd.nr) &&
        add_items(root,"calendar_items",sd.calendar,sd.nc) &&
        add_bluetooth(root);
    char *body=ok?cJSON_PrintUnformatted(root):NULL;
    cJSON_Delete(root);
    if (!body) return reply(r,503,"{\"error\":\"out of memory\"}");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=reply(r,200,body);
    cJSON_free(body);
    return result;
}
void sd_api_start(void){nvs_handle_t h;if(nvs_open("smartdesk",NVS_READONLY,&h)==ESP_OK){size_t n=sizeof(token);nvs_get_str(h,"token",token,&n);nvs_close(h);}httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.max_uri_handlers=12;c.stack_size=6144;c.recv_wait_timeout=5;c.send_wait_timeout=5;httpd_handle_t server=NULL;if(httpd_start(&server,&c)!=ESP_OK)return;httpd_uri_t page={.uri="/",.method=HTTP_GET,.handler=dashboard};httpd_register_uri_handler(server,&page);httpd_uri_t sync_script={.uri="/iphone_sync.js",.method=HTTP_GET,.handler=iphone_sync};httpd_register_uri_handler(server,&sync_script);const char *shortcut_paths[]={"/smartdesk-sync.shortcut","/smartdesk-battery.shortcut"};for(int i=0;i<2;i++){httpd_uri_t shortcut={.uri=shortcut_paths[i],.method=HTTP_GET,.handler=download_shortcut};httpd_register_uri_handler(server,&shortcut);}const char *paths[]={"/api/v1/reminders","/api/v1/calendar","/api/v1/battery","/api/v1/notification","/api/v1/config","/api/v1/notification-action"};for(int i=0;i<6;i++){httpd_uri_t u={.uri=paths[i],.method=HTTP_POST,.handler=post};httpd_register_uri_handler(server,&u);}const char *reads[]={"/api/v1/health","/api/v1/status"};for(int i=0;i<2;i++){httpd_uri_t u={.uri=reads[i],.method=HTTP_GET,.handler=get};httpd_register_uri_handler(server,&u);}}
