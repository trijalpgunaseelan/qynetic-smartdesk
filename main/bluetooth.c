#include "bluetooth.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "os/os_mbuf.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG="smartdesk_ble";
static const ble_uuid128_t ancs_uuid=BLE_UUID128_INIT(0xd0,0x00,0x2d,0x12,0x1e,0x4b,0x0f,0xa4,0x99,0x4e,0xce,0xb5,0x31,0xf4,0x05,0x79);
static const ble_uuid128_t ns_uuid=BLE_UUID128_INIT(0xbd,0x1d,0xa2,0x99,0xe6,0x25,0x58,0x8c,0xd9,0x42,0x01,0x63,0x0d,0x12,0xbf,0x9f);
static const ble_uuid128_t ds_uuid=BLE_UUID128_INIT(0xfb,0x7b,0x7c,0xce,0x6a,0xb3,0x44,0xbe,0xb5,0x4b,0xd6,0x24,0xe9,0xc6,0xea,0x22);
static const ble_uuid128_t cp_uuid=BLE_UUID128_INIT(0xd9,0xd9,0xaa,0xfd,0xbd,0x9b,0x21,0x98,0xa8,0x49,0xe1,0x45,0xf3,0xd8,0xd1,0x69);
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static sd_ble_state view={.status="starting"};
/* All transport state is confined to the NimBLE host task. */
static uint16_t connection=BLE_HS_CONN_HANDLE_NONE;
static uint8_t own_address;
static bool started,discovering,secured,rediscover,host_synced;
static int64_t security_deadline;
static int64_t discovery_deadline,retry_at;
static uint32_t session;
static struct ble_npl_callout timer;
static struct ble_npl_event command_event;
static struct {bool pending,forget;uint32_t uid,session;uint8_t action;} command;
static bool action_inflight;
static int64_t action_deadline;

typedef struct {uint16_t start,end;} service_range;
typedef struct {uint16_t value,end,cccd;uint8_t properties;} characteristic;
static service_range ancs,gatt;
/* Subscribe to Service Changed, then Data Source, then Notification Source. */
static characteristic changed,data_source,notification_source,control_point;
static characteristic *previous;
static int characteristic_stage,subscription_stage;
static characteristic *subscriptions[3]={&changed,&data_source,&notification_source};
typedef struct {uint32_t uid;uint8_t category,flags;} pending_notification;
static pending_notification queue[8],current;
static int queued;
static bool fetching,write_pending,response_complete,removed;
static int64_t request_deadline;
static sd_ancs_parser parser;

void ble_store_config_init(void);
static int gap_event(struct ble_gap_event *event,void *arg);
static void advertise(void);
static void discover(void);
static void pump(void);
static void subscribe_next(void);
static void discover_characteristics(void);

void sd_ble_snapshot(sd_ble_state *out) {
    portENTER_CRITICAL(&lock);*out=view;portEXIT_CRITICAL(&lock);
}
static void status(const char *text,int error) {
    portENTER_CRITICAL(&lock);
    snprintf(view.status,sizeof(view.status),"%s",text);view.error=error;if(error)view.last_error=error;
    portEXIT_CRITICAL(&lock);
    ESP_LOGI(TAG,"%s (code %d)",text,error);
}
static void clear_notifications(void) {
    queued=0;fetching=false;write_pending=false;response_complete=false;removed=false;
    memset(queue,0,sizeof(queue));memset(&current,0,sizeof(current));memset(&parser,0,sizeof(parser));
    portENTER_CRITICAL(&lock);
    view.count=0;view.overlay_until=0;memset(view.items,0,sizeof(view.items));
    view.ready=false;view.details=false;
    portEXIT_CRITICAL(&lock);
}
static void fail(int error) {
    discovering=false;secured=false;rediscover=false;
    status("connection_error",error);
    portENTER_CRITICAL(&lock);view.ready=false;portEXIT_CRITICAL(&lock);
    if(connection!=BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
}
static bool valid_callback(uint16_t conn) {return conn==connection&&secured;}
static void action_result(const char *text,int error) {
    portENTER_CRITICAL(&lock);
    snprintf(view.action_status,sizeof(view.action_status),"%s",text);view.action_error=error;
    portEXIT_CRITICAL(&lock);
}
static void remove_notification(uint32_t uid) {
    for(int i=0;i<queued;) {
        if(queue[i].uid==uid) {memmove(queue+i,queue+i+1,(size_t)(--queued-i)*sizeof(*queue));}else i++;
    }
    if(fetching&&current.uid==uid) removed=true;
    portENTER_CRITICAL(&lock);
    for(int i=0;i<view.count;i++) if(view.items[i].text.uid==uid) {
        if(i==0) view.overlay_until=0;
        memmove(view.items+i,view.items+i+1,(size_t)(view.count-i-1)*sizeof(*view.items));
        memset(&view.items[--view.count],0,sizeof(*view.items));break;
    }
    portEXIT_CRITICAL(&lock);
}
static void publish(const pending_notification *event,const sd_ancs_attributes *text) {
    int64_t received_at=(int64_t)time(NULL),overlay_until=esp_timer_get_time()+18000000;
    portENTER_CRITICAL(&lock);
    int old=view.count;
    for(int i=0;i<view.count;i++) if(view.items[i].text.uid==event->uid) {old=i;break;}
    if(old==view.count&&view.count<SD_NOTIFICATION_MAX) view.count++;
    if(old>=SD_NOTIFICATION_MAX) old=SD_NOTIFICATION_MAX-1;
    memmove(view.items+1,view.items,(size_t)old*sizeof(*view.items));
    view.items[0]=(sd_ble_notification){.text=*text,.category=event->category,.flags=event->flags,.received_at=received_at};
    /* Existing notifications populate the list without interrupting the screen. */
    view.overlay_until=(event->flags&4)?0:overlay_until;
    portEXIT_CRITICAL(&lock);
}
static void finish_request(void) {
    if(response_complete&&!write_pending) {fetching=false;pump();}
}
static int attributes_written(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    if(!valid_callback(conn)||!fetching) return 0;
    write_pending=false;
    if(error->status) {
        /* ANCS errors produce no Data Source response. A transport timeout is
           ambiguous; disconnect rather than assigning late bytes to a new UID. */
        if(error->status==BLE_HS_ETIMEOUT) {fail(error->status);return 0;}
        portENTER_CRITICAL(&lock);view.error=error->status;view.last_error=error->status;view.dropped++;portEXIT_CRITICAL(&lock);
        fetching=false;pump();
    }else finish_request();
    return 0;
}
static void pump(void) {
    while(!fetching&&!action_inflight&&!discovering&&!rediscover&&secured&&notification_source.cccd&&queued) {
    current=queue[0];memmove(queue,queue+1,(size_t)(--queued)*sizeof(*queue));
    if(!data_source.value||!control_point.value) {
        sd_ancs_attributes text={.uid=current.uid};
        snprintf(text.title,sizeof(text.title),"%s",current.category==1?"Incoming call":"iPhone notification");
        publish(&current,&text);continue;
    }
    uint8_t bytes[14];sd_ancs_request(current.uid,bytes);
    sd_ancs_parser_init(&parser,current.uid);
    fetching=true;write_pending=true;response_complete=false;removed=false;
    request_deadline=esp_timer_get_time()+10000000;
    int rc=ble_gattc_write_flat(connection,control_point.value,bytes,sizeof(bytes),attributes_written,NULL);
    if(rc) {write_pending=false;fetching=false;fail(rc);}
    return;
    }
}
static void notification_event(const uint8_t bytes[8]) {
    portENTER_CRITICAL(&lock);view.source_events++;portEXIT_CRITICAL(&lock);
    uint32_t uid=(uint32_t)bytes[4]|(uint32_t)bytes[5]<<8|(uint32_t)bytes[6]<<16|(uint32_t)bytes[7]<<24;
    if(bytes[0]==2) {remove_notification(uid);return;}
    if(bytes[0]>1) return;
    pending_notification next={uid,bytes[2],bytes[1]};
    /* Show an alert immediately; fetching optional details must not hide it. */
    sd_ancs_attributes basic={.uid=uid};
    snprintf(basic.title,sizeof(basic.title),"%s",next.category==1?"Incoming call":"New iPhone notification");
    snprintf(basic.message,sizeof(basic.message),"Fetching preview...");
    publish(&next,&basic);
    for(int i=0;i<queued;i++) if(queue[i].uid==uid) {queue[i]=next;return;}
    if(queued==(int)(sizeof(queue)/sizeof(*queue))) {
        portENTER_CRITICAL(&lock);view.dropped++;portEXIT_CRITICAL(&lock);return;
    }
    queue[queued++]=next;pump();
}
/* Verify the server's CCCD, rather than treating an acknowledged write as
   proof that iOS enabled the notification stream. */
static int subscription_verified(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    if(!valid_callback(conn)) return 0;
    if(error->status) {fail(error->status);return 0;}
    uint8_t expected=subscription_stage==0?2:1;
    if(!attr||attr->handle!=subscriptions[subscription_stage]->cccd||
       attr->om==NULL||OS_MBUF_PKTLEN(attr->om)!=2) {fail(BLE_HS_EBADDATA);return 0;}
    uint8_t value[2];
    if(os_mbuf_copydata(attr->om,0,2,value)||value[0]!=expected||value[1]!=0) {
        fail(BLE_HS_EAUTHEN);return 0;
    }
    subscription_stage++;subscribe_next();return 0;
}
static int subscription_written(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    if(!valid_callback(conn)) return 0;
    if(error->status) {fail(error->status);return 0;}
    int rc=ble_gattc_read(conn,subscriptions[subscription_stage]->cccd,subscription_verified,NULL);
    if(rc) fail(rc);
    return 0;
}
static int descriptor_found(uint16_t conn,const struct ble_gatt_error *error,uint16_t value,const struct ble_gatt_dsc *dsc,void *arg) {
    if(!valid_callback(conn)) return 0;
    characteristic *chr=subscriptions[subscription_stage];
    if(!error->status) {
        if(ble_uuid_cmp(&dsc->uuid.u,BLE_UUID16_DECLARE(0x2902))==0) chr->cccd=dsc->handle;
    }else if(error->status==BLE_HS_EDONE) {
        if(!chr->cccd) {fail(BLE_HS_ENOENT);return 0;}
        uint8_t enable[2]={subscription_stage==0?2:1,0};
        int rc=ble_gattc_write_flat(conn,chr->cccd,enable,sizeof(enable),subscription_written,NULL);
        if(rc) fail(rc);
    }else fail(error->status);
    return 0;
}
static void subscribe_next(void) {
    while(subscription_stage<3&&!subscriptions[subscription_stage]->value) subscription_stage++;
    if(subscription_stage==3) {
        discovering=false;retry_at=esp_timer_get_time()+10000000;
        bool ready=notification_source.value!=0;
        portENTER_CRITICAL(&lock);
        view.ready=ready;view.details=ready&&data_source.value&&control_point.value;
        portEXIT_CRITICAL(&lock);
        status(ready?"ready":"waiting_for_ancs",0);pump();return;
    }
    characteristic *chr=subscriptions[subscription_stage];
    if(chr->end<=chr->value) {fail(BLE_HS_ENOENT);return;}
    int rc=ble_gattc_disc_all_dscs(connection,chr->value,chr->end,descriptor_found,NULL);
    if(rc) fail(rc);
}
static int characteristic_found(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_chr *chr,void *arg) {
    if(!valid_callback(conn)) return 0;
    if(!error->status) {
        /* A descriptor search must stop before the next characteristic. */
        if(previous) previous->end=chr->def_handle-1;
        characteristic *target=NULL;
        if(ble_uuid_cmp(&chr->uuid.u,&ns_uuid.u)==0) target=&notification_source;
        else if(ble_uuid_cmp(&chr->uuid.u,&ds_uuid.u)==0) target=&data_source;
        else if(ble_uuid_cmp(&chr->uuid.u,&cp_uuid.u)==0) target=&control_point;
        else if(characteristic_stage==0&&ble_uuid_cmp(&chr->uuid.u,BLE_UUID16_DECLARE(0x2a05))==0) target=&changed;
        previous=target;
        if(target) *target=(characteristic){.value=chr->val_handle,.end=characteristic_stage==0?gatt.end:ancs.end,.properties=chr->properties};
    }else if(error->status==BLE_HS_EDONE) {
        characteristic_stage++;discover_characteristics();
    }else fail(error->status);
    return 0;
}
static void discover_characteristics(void) {
    while(characteristic_stage<2) {
        service_range range=characteristic_stage==0?gatt:ancs;
        if(!range.start) {characteristic_stage++;continue;}
        previous=NULL;
        int rc=ble_gattc_disc_all_chrs(connection,range.start,range.end,characteristic_found,NULL);
        if(rc) fail(rc);
        return;
    }
    if((notification_source.value&&!(notification_source.properties&BLE_GATT_CHR_PROP_NOTIFY))||
       (data_source.value&&!(data_source.properties&BLE_GATT_CHR_PROP_NOTIFY))||
       (control_point.value&&!(control_point.properties&BLE_GATT_CHR_PROP_WRITE))||
       (changed.value&&!(changed.properties&BLE_GATT_CHR_PROP_INDICATE))) {fail(BLE_HS_ENOTSUP);return;}
    subscription_stage=0;subscribe_next();
}
static int service_found(uint16_t conn,const struct ble_gatt_error *error,const struct ble_gatt_svc *svc,void *arg) {
    if(!valid_callback(conn)) return 0;
    if(!error->status) {
        if(ble_uuid_cmp(&svc->uuid.u,&ancs_uuid.u)==0) ancs=(service_range){svc->start_handle,svc->end_handle};
        else if(ble_uuid_cmp(&svc->uuid.u,BLE_UUID16_DECLARE(0x1801))==0) gatt=(service_range){svc->start_handle,svc->end_handle};
    }else if(error->status==BLE_HS_EDONE) {characteristic_stage=0;discover_characteristics();}
    else fail(error->status);
    return 0;
}
static void rotate_session(void) {
    /* Randomize across reboots too, so an old browser dialog cannot target a
       reused UID after power loss. Only the host writes this generation. */
    uint32_t next=esp_random();if(next==session) next++;
    portENTER_CRITICAL(&lock);view.session=session=next;portEXIT_CRITICAL(&lock);
}
static void discover(void) {
    if(!secured||discovering||fetching||action_inflight) {rediscover=true;return;}
    rediscover=false;discovering=true;discovery_deadline=esp_timer_get_time()+30000000;
    clear_notifications();
    rotate_session();
    memset(&ancs,0,sizeof(ancs));memset(&gatt,0,sizeof(gatt));
    memset(&notification_source,0,sizeof(notification_source));memset(&data_source,0,sizeof(data_source));
    memset(&control_point,0,sizeof(control_point));memset(&changed,0,sizeof(changed));
    status("discovering",0);
    int rc=ble_gattc_disc_all_svcs(connection,service_found,NULL);if(rc) fail(rc);
}
static void advertise(void) {
    if(!host_synced||connection!=BLE_HS_CONN_HANDLE_NONE||ble_gap_adv_active()) return;
    /* Solicitation advertises that we consume ANCS; it is hosted by iOS. */
    struct ble_hs_adv_fields adv={0},scan={0};
    adv.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    adv.sol_uuids128=&ancs_uuid;adv.sol_num_uuids128=1;
    const char *name="Qynetic SmartDesk";
    scan.name=(const uint8_t *)name;scan.name_len=strlen(name);scan.name_is_complete=1;
    int rc=ble_gap_adv_set_fields(&adv);
    if(!rc) rc=ble_gap_adv_rsp_set_fields(&scan);
    struct ble_gap_adv_params params={.conn_mode=BLE_GAP_CONN_MODE_UND,.disc_mode=BLE_GAP_DISC_MODE_GEN,
        .itvl_min=BLE_GAP_ADV_FAST_INTERVAL2_MIN,.itvl_max=BLE_GAP_ADV_FAST_INTERVAL2_MAX};
    if(!rc) rc=ble_gap_adv_start(own_address,NULL,BLE_HS_FOREVER,&params,gap_event,NULL);
    status(rc?"advertising_error":"advertising",rc);
}
static void reset_session(void) {
    secured=false;discovering=false;rediscover=false;action_inflight=false;
    clear_notifications();
    rotate_session();
    portENTER_CRITICAL(&lock);
    view.connected=false;view.encrypted=false;view.bonded=false;view.pairing=false;view.passkey=0;
    if(command.pending||!strcmp(view.action_status,"sending")) {command.pending=false;snprintf(view.action_status,sizeof(view.action_status),"disconnected");}
    portEXIT_CRITICAL(&lock);
}
static int gap_event(struct ble_gap_event *event,void *arg) {
    switch(event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if(event->connect.status) {advertise();break;}
        reset_session();connection=event->connect.conn_handle;
        portENTER_CRITICAL(&lock);view.connected=true;portEXIT_CRITICAL(&lock);
        security_deadline=esp_timer_get_time()+45000000;
        status("securing",0);
        {int rc=ble_gap_security_initiate(connection);if(rc&&rc!=BLE_HS_EALREADY) fail(rc);}
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        connection=BLE_HS_CONN_HANDLE_NONE;reset_session();advertise();break;
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise();break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        if(event->enc_change.conn_handle!=connection) break;
        struct ble_gap_conn_desc desc;
        int rc=ble_gap_conn_find(connection,&desc);
        if(event->enc_change.status||rc||!desc.sec_state.encrypted||!desc.sec_state.authenticated) {
            fail(event->enc_change.status?event->enc_change.status:BLE_HS_EAUTHEN);break;
        }
        secured=true;
        portENTER_CRITICAL(&lock);
        view.encrypted=true;view.bonded=desc.sec_state.bonded;view.pairing=false;view.passkey=0;
        portEXIT_CRITICAL(&lock);
        discover();break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        if(event->passkey.params.action!=BLE_SM_IOACT_DISP) {fail(BLE_HS_ENOTSUP);break;}
        struct ble_sm_io io={.action=BLE_SM_IOACT_DISP,.passkey=esp_random()%1000000};
        portENTER_CRITICAL(&lock);view.pairing=true;view.passkey=io.passkey;portEXIT_CRITICAL(&lock);
        status("enter_passkey_on_iphone",0);
        int rc=ble_sm_inject_io(connection,&io);if(rc) fail(rc);break;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Never silently replace an existing bond. Owner can use serial UNPAIR. */
        status("bond_reset_required",0);return BLE_GAP_REPEAT_PAIRING_IGNORE;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        if(!secured||event->notify_rx.conn_handle!=connection) break;
        struct os_mbuf *om=event->notify_rx.om;
        if(!om) break;
        uint16_t size=OS_MBUF_PKTLEN(om),handle=event->notify_rx.attr_handle;
        if(handle==changed.value) {rediscover=true;break;}
        if(handle==notification_source.value&&size==8) {
            uint8_t bytes[8];if(!os_mbuf_copydata(om,0,8,bytes)) notification_event(bytes);
        }else if(handle==data_source.value&&fetching) {
            portENTER_CRITICAL(&lock);view.data_bytes+=size;portEXIT_CRITICAL(&lock);
            uint8_t bytes[64];int result=0;
            for(unsigned offset=0;offset<size;offset+=sizeof(bytes)) {
                unsigned n=size-offset;if(n>sizeof(bytes)) n=sizeof(bytes);
                if(os_mbuf_copydata(om,offset,n,bytes)) {fail(BLE_HS_EBADDATA);break;}
                result=sd_ancs_parser_feed(&parser,bytes,n);
                if(result<0) {fail(BLE_HS_EBADDATA);break;}
            }
            if(result==1&&secured) {
                portENTER_CRITICAL(&lock);view.decoded++;portEXIT_CRITICAL(&lock);
                response_complete=true;if(!removed) publish(&current,&parser.value);finish_request();
            }
        }
        break;
    }
    default: break;
    }
    return 0;
}
static int action_written(uint16_t conn,const struct ble_gatt_error *error,struct ble_gatt_attr *attr,void *arg) {
    if(conn!=connection||!action_inflight) return 0;
    action_inflight=false;action_result(error->status?"failed":"completed",error->status);
    if(error->status==BLE_HS_ETIMEOUT) fail(error->status);else pump();
    return 0;
}
static void run_command(struct ble_npl_event *event) {
    uint32_t uid,generation;uint8_t action;bool pending,forget;
    portENTER_CRITICAL(&lock);
    pending=command.pending;forget=command.forget;command.forget=false;
    uid=command.uid;action=command.action;generation=command.session;
    portEXIT_CRITICAL(&lock);
    if(forget) {
        if(connection!=BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
        reset_session();int rc=ble_store_clear();status(rc?"bond_reset_failed":"bonds_cleared",rc);return;
    }
    if(!pending) return;
    if(generation!=session||!secured||discovering||rediscover||!control_point.value) {
        action_result("not_connected",BLE_HS_ENOTCONN);
    }else if(fetching||action_inflight) return; /* timer reschedules after fetch */
    else {
        bool exists=false;
        portENTER_CRITICAL(&lock);
        for(int i=0;i<view.count;i++) if(view.items[i].text.uid==uid&&(view.items[i].flags&(action?16:8))) exists=true;
        portEXIT_CRITICAL(&lock);
        if(!exists) action_result("expired",BLE_HS_ENOENT);
        else {
            uint8_t bytes[6]={2,(uint8_t)uid,(uint8_t)(uid>>8),(uint8_t)(uid>>16),(uint8_t)(uid>>24),action};
            action_inflight=true;action_deadline=esp_timer_get_time()+10000000;action_result("sending",0);
            int rc=ble_gattc_write_flat(connection,control_point.value,bytes,sizeof(bytes),action_written,NULL);
            if(rc) {action_inflight=false;action_result("failed",rc);}
        }
    }
    portENTER_CRITICAL(&lock);command.pending=false;portEXIT_CRITICAL(&lock);
}
esp_err_t sd_ble_action(uint32_t uid,uint8_t action,uint32_t generation) {
    if(!started||action>1) return ESP_ERR_INVALID_ARG;
    esp_err_t result=ESP_ERR_NOT_FOUND;
    portENTER_CRITICAL(&lock);
    if(!view.ready||!view.details||command.pending||!strcmp(view.action_status,"sending")||generation!=view.session) result=ESP_ERR_INVALID_STATE;
    else for(int i=0;i<view.count;i++) if(view.items[i].text.uid==uid&&(view.items[i].flags&(action?16:8))) {
        command.uid=uid;command.action=action;command.session=generation;command.pending=true;
        snprintf(view.action_status,sizeof(view.action_status),"queued");view.action_error=0;result=ESP_OK;break;
    }
    portEXIT_CRITICAL(&lock);
    if(result==ESP_OK) ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&command_event);
    return result;
}
esp_err_t sd_ble_forget_bonds(void) {
    if(!started) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&lock);command.forget=true;portEXIT_CRITICAL(&lock);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&command_event);return ESP_OK;
}
static void timer_tick(struct ble_npl_event *event) {
    int64_t now=esp_timer_get_time();
    if(connection!=BLE_HS_CONN_HANDLE_NONE&&!secured&&now>security_deadline) fail(BLE_HS_ETIMEOUT);
    if((fetching&&now>request_deadline)||(discovering&&now>discovery_deadline)||(action_inflight&&now>action_deadline)) fail(BLE_HS_ETIMEOUT);
    if(secured&&!discovering&&!fetching&&!action_inflight) {
        if(rediscover||(!notification_source.value&&now>retry_at)) discover();
        else {run_command(NULL);pump();}
    }
    if(connection==BLE_HS_CONN_HANDLE_NONE) advertise();
    ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(1000));
}
static void on_reset(int reason) {host_synced=false;connection=BLE_HS_CONN_HANDLE_NONE;reset_session();status("host_reset",reason);}
static void on_sync(void) {
    int rc=ble_hs_util_ensure_addr(0);
    if(!rc) rc=ble_hs_id_infer_auto(0,&own_address);
    if(rc) {status("address_error",rc);return;}
    host_synced=true;advertise();ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(1000));
}
static void host_task(void *arg) {nimble_port_run();nimble_port_freertos_deinit();}
esp_err_t sd_ble_start(void) {
    esp_err_t result=nimble_port_init();if(result!=ESP_OK) {status("init_error",result);return result;}
    ble_hs_cfg.reset_cb=on_reset;ble_hs_cfg.sync_cb=on_sync;ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_ONLY;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("Qynetic SmartDesk");ble_store_config_init();
    ble_npl_event_init(&command_event,run_command,NULL);
    ble_npl_callout_init(&timer,nimble_port_get_dflt_eventq(),timer_tick,NULL);
    started=true;nimble_port_freertos_init(host_task);return ESP_OK;
}
