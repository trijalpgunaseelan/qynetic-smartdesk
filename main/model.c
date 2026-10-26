#include "smartdesk.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
sd_state sd={.battery=-1};
static bool restoring;
/* Convert a UTC broken-down date without relying on nonportable timegm(). */
static time_t sd_utc_epoch(const struct tm *t) {
    int y=t->tm_year+1900, m=t->tm_mon+1, d=t->tm_mday;
    y -= m <= 2;
    const int era=(y>=0 ? y : y-399)/400;
    const unsigned yoe=(unsigned)(y-era*400);
    const unsigned mp=(unsigned)(m+(m>2 ? -3 : 9));
    const unsigned doy=(153*mp+2)/5+(unsigned)d-1;
    const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    int64_t days=(int64_t)era*146097+(int64_t)doe-719468;
    return (time_t)(days*86400+t->tm_hour*3600+t->tm_min*60+t->tm_sec);
}
static time_t parse_iso(const char *s){int y,m,d,h=0,mi=0,sec=0,zh=0,zm=0;char sign='Z';if(!s||sscanf(s,"%4d-%2d-%2dT%2d:%2d:%2d",&y,&m,&d,&h,&mi,&sec)<5)return (time_t)-1; if(y<2020||y>2100||m<1||m>12||d<1||d>31||h>23||mi>59||sec>60)return (time_t)-1;struct tm t={.tm_year=y-1900,.tm_mon=m-1,.tm_mday=d,.tm_hour=h,.tm_min=mi,.tm_sec=sec};time_t epoch=sd_utc_epoch(&t);if(epoch==(time_t)-1)return epoch;const char *z=strchr(s,'T');if(!z)return (time_t)-1;z+=strlen(z);while(z>s&&z[-1]!='+'&&z[-1]!='-'&&z[-1]!='Z')z--;if(z>s&&(z[-1]=='+'||z[-1]=='-')){sign=z[-1];if(sscanf(z,"%2d:%2d",&zh,&zm)!=2||zh>23||zm>59)return (time_t)-1;epoch+=(sign=='+'?-1:1)*(zh*3600+zm*60);}return epoch;}
static int cmp(const void *a,const void *b){const sd_item*x=a,*y=b;return (x->when>y->when)-(x->when<y->when);}
static void save(const char *key,const char *value){nvs_handle_t h;if(nvs_open("smartdesk",NVS_READWRITE,&h)==ESP_OK){if(nvs_set_str(h,key,value)==ESP_OK)nvs_commit(h);nvs_close(h);}}
void sd_model_init(void){nvs_handle_t h;if(nvs_open("smartdesk",NVS_READONLY,&h)!=ESP_OK)return;restoring=true;const char *keys[]={"reminders","calendar","battery"};for(int i=0;i<3;i++){size_t n=0;if(nvs_get_str(h,keys[i],NULL,&n)==ESP_OK&&n&&n<=SD_BODY_MAX){char *b=malloc(n);if(b&&nvs_get_str(h,keys[i],b,&n)==ESP_OK)sd_update(keys[i],b);free(b);}}nvs_close(h);restoring=false;}
esp_err_t sd_update(const char *kind,const char *json){if(!json||strlen(json)>SD_BODY_MAX)return ESP_ERR_INVALID_SIZE;cJSON *root=cJSON_Parse(json);if(!root)return ESP_ERR_INVALID_ARG;esp_err_t result=ESP_ERR_INVALID_ARG;
if(!strcmp(kind,"battery")){cJSON *v=cJSON_GetObjectItemCaseSensitive(root,"level");if(cJSON_IsNumber(v)&&v->valuedouble>=0&&v->valuedouble<=100&&v->valueint==v->valuedouble){cJSON *charging=cJSON_GetObjectItemCaseSensitive(root,"charging");if(charging&&!cJSON_IsBool(charging)){cJSON_Delete(root);return ESP_ERR_INVALID_ARG;}sd.charging_known=cJSON_IsBool(charging);sd.charging=cJSON_IsTrue(charging);sd.battery=v->valueint;sd.battery_sync=restoring?0:time(NULL);result=ESP_OK;}}
else if(!strcmp(kind,"reminders")||!strcmp(kind,"calendar")){const char *key=!strcmp(kind,"reminders")?"reminders":"events";cJSON *a=cJSON_GetObjectItemCaseSensitive(root,key);if(cJSON_IsArray(a)&&cJSON_GetArraySize(a)<=32){sd_item *tmp=calloc(32,sizeof(*tmp));if(!tmp){cJSON_Delete(root);return ESP_ERR_NO_MEM;}int n=0;result=ESP_OK;cJSON *v=NULL;cJSON_ArrayForEach(v,a){cJSON *title=cJSON_GetObjectItemCaseSensitive(v,"title"),*id=cJSON_GetObjectItemCaseSensitive(v,"id"),*date=cJSON_GetObjectItemCaseSensitive(v,!strcmp(kind,"reminders")?"due":"start"),*done=cJSON_GetObjectItemCaseSensitive(v,"completed"),*allday=cJSON_GetObjectItemCaseSensitive(v,"all_day");if(!cJSON_IsString(title)||strlen(title->valuestring)>=SD_TITLE||!cJSON_IsString(date)||strlen(date->valuestring)>40||(!strcmp(kind,"reminders")&&(!cJSON_IsBool(done)||!cJSON_IsString(id)||strlen(id->valuestring)>=SD_ID))){result=ESP_ERR_INVALID_ARG;break;}time_t when=parse_iso(date->valuestring);if(when==(time_t)-1){result=ESP_ERR_INVALID_ARG;break;}cJSON *end=cJSON_GetObjectItemCaseSensitive(v,"end");if(!strcmp(kind,"calendar")&&end){if(!cJSON_IsString(end)||strlen(end->valuestring)>40){result=ESP_ERR_INVALID_ARG;break;}tmp[n].end=parse_iso(end->valuestring);if(tmp[n].end==(time_t)-1||tmp[n].end<=when){result=ESP_ERR_INVALID_ARG;break;}}snprintf(tmp[n].title,SD_TITLE,"%s",title->valuestring);if(cJSON_IsString(id))snprintf(tmp[n].id,SD_ID,"%s",id->valuestring);tmp[n].when=when;tmp[n].completed=cJSON_IsTrue(done);tmp[n].all_day=cJSON_IsTrue(allday);n++;}if(result==ESP_OK){qsort(tmp,n,sizeof(sd_item),cmp);sd_item *dest=!strcmp(kind,"reminders")?sd.reminders:sd.calendar;int limit=!strcmp(kind,"reminders")?SD_MAX_ITEMS:SD_MAX_EVENTS;memset(dest,0,limit*sizeof(sd_item));memcpy(dest,tmp,(n<limit?n:limit)*sizeof(sd_item));if(!strcmp(kind,"reminders")){sd.nr=n<5?n:5;sd.reminder_sync=restoring?0:time(NULL);}else{sd.nc=n<SD_MAX_EVENTS?n:SD_MAX_EVENTS;sd.calendar_sync=restoring?0:time(NULL);}}free(tmp);}}
else if(!strcmp(kind,"notification")){cJSON *t=cJSON_GetObjectItemCaseSensitive(root,"text");if(cJSON_IsString(t)&&strlen(t->valuestring)<sizeof(sd.notification)){snprintf(sd.notification,sizeof(sd.notification),"%s",t->valuestring);sd.notification_until=time(NULL)+10;result=ESP_OK;}}
if (!restoring && result == ESP_OK && strcmp(kind, "notification") != 0) {
    save(kind, json);
}
cJSON_Delete(root);
return result;
}
