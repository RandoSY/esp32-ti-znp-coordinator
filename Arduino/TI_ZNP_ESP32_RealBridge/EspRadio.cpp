#include "EspRadio.h"
#include "ZnpFrame.h"
#include "LqiMetadata.h"
#include <esp_mac.h>
#include <esp_system.h>
#include <new>
#include <algorithm>
#include "test/esp_zigbee_test_utils.h"
extern "C" {
#include "zboss_api.h"
}
EspRadio *EspRadio::instance=nullptr;
namespace {
const uint8_t eps[]={1,2,3,4,5,6,8,10,11,110,12,13,47,239,242};
const uint16_t profiles[]={0x0104,0x0101,0x0104,0x0107,0x0108,0x0109,0x0104,0x0104,0x0104,0x0104,0xC05E,0x0104,0x0104,0xC001,0xA1E0};
uint16_t profile(uint8_t ep){for(unsigned i=0;i<sizeof(eps);++i)if(eps[i]==ep)return profiles[i];return 0xFFFF;}
void finish(EspRadio::Context *c,uint8_t code,const uint8_t *p=nullptr,size_t n=0,bool address=true){
    uint8_t out[250]={};size_t offset=0;
    if(address){znp::put16(out,c->dst);offset=2;}
    out[offset++]=code;
    if(n>sizeof(out)-offset)n=sizeof(out)-offset;
    if(n)memcpy(out+offset,p,n);
    c->self->event(uint8_t(c->command|0x80),out,offset+n);delete c;
}
void addressResponse(esp_zb_zdp_status_t status,esp_zb_zdo_nwk_addr_rsp_t *r,void *ctx){
    auto c=static_cast<EspRadio::Context *>(ctx);uint8_t p[247]={};size_t n=12;
    if(status==0 && r){memcpy(p,r->ieee_addr,8);znp::put16(p+8,r->nwk_addr);
        if(r->ext_resp){p[10]=r->ext_resp->start_index;p[11]=r->ext_resp->total;
            unsigned count=std::min<unsigned>(r->ext_resp->count,117);
            for(unsigned i=0;i<count;++i)znp::put16(p+12+2*i,r->ext_resp->nwk_addresses[i]);n+=2*count;}}
    finish(c,uint8_t(status),p,n,false);
}
uint16_t route_dst=0;bool route_busy=false;
void routeResponse(zb_uint8_t b){
    uint8_t p[250]={};znp::put16(p,route_dst);unsigned n=zb_buf_len(b);
    const uint8_t *raw=static_cast<uint8_t *>(zb_buf_begin(b));
    if(n>1){n=std::min<unsigned>(n-1,248);memcpy(p+2,raw+1,n);if(EspRadio::instance)EspRadio::instance->event(0xB2,p,n+2);}
    zb_buf_free(b);route_busy=false;
}
}
bool EspRadio::begin(){
    instance=this;rx_queue_=xQueueCreate(24,sizeof(ApsRx));confirmation_queue_=xQueueCreate(24,sizeof(ApsConfirm));events_=xQueueCreate(24,sizeof(RadioEvent));
    group_done_=xSemaphoreCreateBinary();
    if(!rx_queue_ || !confirmation_queue_ || !events_ || !group_done_){state_=State::Error;return false;}
    uint8_t mac[8];if(esp_read_mac(mac,ESP_MAC_IEEE802154)!=ESP_OK){state_=State::Error;return false;}
    for(unsigned i=0;i<8;++i)cached_.ieee[i]=mac[7-i];
    cached_.short_addr=0xFFFE;cached_.pan_id=0xFFFF;state_=State::Waiting;return true;
}
bool EspRadio::registerEndpoints(){
    auto list=esp_zb_ep_list_create();if(!list){error_code_=0x02000001;return false;}
    auto check=[this](esp_err_t r,uint8_t ep,uint8_t step){if(r!=ESP_OK)error_code_=(uint32_t(step)<<24)|(uint32_t(ep)<<16)|uint16_t(r);return r==ESP_OK;};
    for(unsigned i=0;i<sizeof(eps);++i){
        auto clusters=esp_zb_zcl_cluster_list_create();if(!clusters){error_code_=0x02000002;return false;}
        if(eps[i]==11){
            if(!check(esp_zb_cluster_list_add_ias_ace_cluster(clusters,esp_zb_zcl_attr_list_create(0x0501),ESP_ZB_ZCL_CLUSTER_SERVER_ROLE),eps[i],3))return false;
            if(!check(esp_zb_cluster_list_add_time_cluster(clusters,esp_zb_zcl_attr_list_create(0x000A),ESP_ZB_ZCL_CLUSTER_SERVER_ROLE),eps[i],3))return false;
            if(!check(esp_zb_cluster_list_add_ias_zone_cluster(clusters,esp_zb_zcl_attr_list_create(0x0500),ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE),eps[i],3))return false;
            if(!check(esp_zb_cluster_list_add_ias_wd_cluster(clusters,esp_zb_zcl_attr_list_create(0x0502),ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE),eps[i],3))return false;
        }
        if(eps[i]==13 && !check(esp_zb_cluster_list_add_ota_cluster(clusters,esp_zb_zcl_attr_list_create(0x0019),ESP_ZB_ZCL_CLUSTER_SERVER_ROLE),eps[i],3))return false;
        // Gateway endpoints let host software handle all application clusters.
        esp_zb_endpoint_config_t c={};c.endpoint=eps[i];c.app_profile_id=profiles[i];c.app_device_id=eps[i]==11?0x0400:5;
        if(!check(esp_zb_ep_list_add_ep(list,clusters,c),eps[i],4))return false;
    }
    return check(esp_zb_device_register(list),0,5);
}
uint8_t EspRadio::start(const NetworkConfig &c,bool formation){
    if(task_created_)return cached_.started?0:0x11;
    if(c.pan_id==0xFFFF || !c.primary_channels)return 2;
    config_=c;formation_=formation;state_=State::Commissioning;
    task_created_=true;
    if(xTaskCreate(task,"znp_zigbee",12288,this,5,nullptr)!=pdPASS){task_created_=false;state_=State::Error;return 0x10;}
    return 0;
}
void EspRadio::task(void *arg){
    auto self=static_cast<EspRadio *>(arg);
    esp_zb_platform_config_t platform={};platform.radio_config.radio_mode=ZB_RADIO_MODE_NATIVE;platform.host_config.host_connection_mode=ZB_HOST_CONNECTION_MODE_NONE;
    esp_err_t result=esp_zb_platform_config(&platform);
    if(result!=ESP_OK){self->error_code_=0x01000000|uint16_t(result);self->state_=State::Error;vTaskDelete(nullptr);return;}
    esp_zb_cfg_t cfg={};cfg.esp_zb_role=ESP_ZB_DEVICE_TYPE_COORDINATOR;cfg.install_code_policy=false;cfg.nwk_cfg.zczr_cfg.max_children=32;
    esp_zb_init(&cfg);
    if(!self->registerEndpoints()){self->state_=State::Error;vTaskDelete(nullptr);return;}
    esp_zb_aps_data_indication_handler_register(indication);esp_zb_aps_data_confirm_handler_register(confirmation);
    esp_zb_set_long_address(self->config_.ieee);esp_zb_set_pan_id(self->config_.pan_id);esp_zb_set_extended_pan_id(self->config_.extended_pan);
    esp_zb_secur_network_key_set(self->config_.network_key);esp_zb_secur_link_key_exchange_required_set(false);
    esp_zb_set_primary_network_channel_set(self->config_.primary_channels);esp_zb_set_secondary_network_channel_set(self->config_.secondary_channels);
    esp_zb_nvram_erase_at_start(self->config_.erase);
    result=esp_zb_start(false);
    if(result!=ESP_OK){self->error_code_=0x06000000|uint16_t(result);self->state_=State::Error;vTaskDelete(nullptr);return;}
    self->initialized_=true;esp_zb_stack_main_loop();vTaskDelete(nullptr);
}
void EspRadio::signal(esp_zb_app_signal_t *s){
    const auto sig=esp_zb_app_signal_type_t(*s->p_app_signal);
    const bool ok=s->esp_err_status==ESP_OK;
    if(sig==ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP){esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);return;}
    if(sig==ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START || sig==ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT){
        if(!ok){error_code_=0x07000000|(uint32_t(sig)<<16)|uint16_t(s->esp_err_status);state_=State::Error;return;}
        if(esp_zb_bdb_is_factory_new()){
            // Parameters must be reapplied after SDK NVRAM initialization.
            esp_zb_set_pan_id(config_.pan_id);esp_zb_set_extended_pan_id(config_.extended_pan);esp_zb_secur_network_key_set(config_.network_key);
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);return;
        }
    }else if(sig!=ESP_ZB_BDB_SIGNAL_FORMATION){
        if(sig==ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE){
            auto p=static_cast<esp_zb_zdo_signal_device_annce_params_t *>(esp_zb_app_signal_get_params(s->p_app_signal));uint8_t b[13];
            znp::put16(b,p->device_short_addr);znp::put16(b+2,p->device_short_addr);memcpy(b+4,p->ieee_addr,8);b[12]=p->capability;event(0xC1,b,13);
        }else if(sig==ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE){
            auto p=static_cast<esp_zb_zdo_signal_device_update_params_t *>(esp_zb_app_signal_get_params(s->p_app_signal));uint8_t b[12];
            znp::put16(b,p->short_addr);memcpy(b+2,p->long_addr,8);znp::put16(b+10,p->parent_short);event(0xCA,b,12);
        }else if(sig==ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION){
            auto p=static_cast<esp_zb_zdo_signal_leave_indication_params_t *>(esp_zb_app_signal_get_params(s->p_app_signal));uint8_t b[13]={};
            znp::put16(b,p->short_addr);memcpy(b+2,p->device_addr,8);b[12]=p->rejoin;event(0xC9,b,13);
        }
        return;
    }
    if(!ok){error_code_=0x07000000|(uint32_t(sig)<<16)|uint16_t(s->esp_err_status);state_=State::Error;return;}
    const uint32_t current=esp_zb_nwk_get_frame_counter();if(config_.frame_counter>current)esp_zb_nwk_set_frame_counter(config_.frame_counter);
    portENTER_CRITICAL(&mux_);cached_.started=true;portEXIT_CRITICAL(&mux_);
    state_=State::Online;esp_zb_bdb_close_network();const uint8_t state=9;event(0xC0,&state,1);
}
extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *s){if(EspRadio::instance)EspRadio::instance->signal(s);}
RadioSnapshot EspRadio::snapshot(){
    RadioSnapshot s;portENTER_CRITICAL(&mux_);s=cached_;portEXIT_CRITICAL(&mux_);
    if(initialized_ && s.started && esp_zb_lock_acquire(pdMS_TO_TICKS(1000))){
        esp_zb_get_long_address(s.ieee);s.short_addr=esp_zb_get_short_address();s.pan_id=esp_zb_get_pan_id();s.channel=esp_zb_get_current_channel();
        esp_zb_get_extended_pan_id(s.extended_pan);esp_zb_secur_primary_network_key_get(s.network_key);s.frame_counter=esp_zb_nwk_get_frame_counter();
        esp_zb_lock_release();
    }
    return s;
}
void EspRadio::event(uint8_t cmd,const uint8_t *p,size_t n){
    if(n>250){++dropped_;return;}RadioEvent e={};e.subsystem=5;e.command=cmd;e.len=uint8_t(n);if(n)memcpy(e.data,p,n);
    if(!events_ || xQueueSend(events_,&e,0)!=pdTRUE)++dropped_;
}
uint8_t EspRadio::reset(bool){reboot_at_=millis()+100;return 0;}
void EspRadio::service(){
    if(reboot_at_ && int32_t(millis()-reboot_at_)>=0){Serial.flush();ESP.restart();}
    if(state_==State::Joining && join_until_ && int32_t(millis()-join_until_)>=0){state_=State::Online;join_until_=0;}
}
uint8_t EspRadio::endpoints(uint8_t *p){memcpy(p,eps,sizeof(eps));return sizeof(eps);}
bool EspRadio::descriptor(uint8_t ep,EndpointDescriptor &e){
    if(profile(ep)==0xFFFF)return false;
    e={};e.endpoint=ep;e.profile=profile(ep);e.device=ep==11?0x0400:5;
    if(ep==11){e.in_count=2;e.out_count=2;e.in[0]=0x0501;e.in[1]=0x000A;e.out[0]=0x0500;e.out[1]=0x0502;}
    if(ep==13){e.in_count=1;e.in[0]=0x0019;}
    return true;
}
uint8_t EspRadio::endpoint(const EndpointDescriptor &e){
    if(e.endpoint==0 || e.endpoint>242 || e.in_count>16 || e.out_count>16)return 2;
    // These descriptors are registered in the stack before network formation.
    EndpointDescriptor actual={};
    if(!descriptor(e.endpoint,actual) || actual.profile!=e.profile || actual.device!=e.device || actual.version!=e.version || actual.in_count!=e.in_count || actual.out_count!=e.out_count)return 0x12;
    for(unsigned i=0;i<e.in_count;++i)if(e.in[i]!=actual.in[i])return 0x12;
    for(unsigned i=0;i<e.out_count;++i)if(e.out[i]!=actual.out[i])return 0x12;
    return 0;
}
uint8_t EspRadio::txPower(int8_t power){if(!initialized_)return 0x83;esp_zb_lock_acquire(portMAX_DELAY);esp_zb_set_tx_power(power);esp_zb_lock_release();return 0;}
uint8_t EspRadio::permitJoin(uint8_t sec){
    if(!cached_.started)return 0x83;esp_zb_lock_acquire(portMAX_DELAY);
    esp_err_t r=sec?esp_zb_bdb_open_network(sec):esp_zb_bdb_close_network();esp_zb_lock_release();
    if(r!=ESP_OK)return 1;state_=sec?State::Joining:State::Online;join_until_=(sec && sec!=255)?millis()+1000U*sec:0;return 0;
}
uint8_t EspRadio::group(uint8_t ep,uint16_t id,bool add){
    if(!cached_.started || profile(ep)==0xFFFF)return 0x83;
    esp_zb_lock_acquire(portMAX_DELAY);
    bool exists=zb_aps_is_endpoint_in_group(id,ep);
    esp_err_t result=ESP_OK;
    // The SDK's direct APS table API supports the reserved GP endpoint 242.
    // Pin Arduino-ESP32: this SDK API is in Espressif's test/certification API.
    if(add && !exists){result=esp_zb_aps_group_table_add_group(id,ep);exists=zb_aps_is_endpoint_in_group(id,ep);}
    esp_zb_lock_release();
    return result==ESP_OK && exists?0:1;
}
uint8_t EspRadio::installCode(const uint8_t *ieee,const uint8_t *code,uint8_t n){
    if(!initialized_)return 0x83;
    uint8_t type=n==8?0:n==10?1:n==14?2:n==18?3:255;if(type==255)return 2;
    esp_zb_ieee_addr_t addr;memcpy(addr,ieee,8);uint8_t copy[18];memcpy(copy,code,n);
    esp_zb_lock_acquire(portMAX_DELAY);auto r=esp_zb_secur_ic_add(addr,type,copy);esp_zb_lock_release();return r==ESP_OK?0:1;
}
uint8_t EspRadio::sendAps(uint16_t dst,uint8_t de,uint8_t se,uint16_t cl,uint8_t id,uint8_t opt,uint8_t rad,const uint8_t *p,uint8_t n){
    uint8_t a[8]={};znp::put16(a,dst);return sendApsExt(2,a,de,se,cl,id,opt,rad,p,n);
}
uint8_t EspRadio::sendApsExt(uint8_t mode,const uint8_t *dst,uint8_t de,uint8_t se,uint16_t cl,uint8_t id,uint8_t opt,uint8_t rad,const uint8_t *p,uint8_t n){
    if(!cached_.started)return 0x83;
    if(n>220 || profile(se)==0xFFFF || (mode!=1 && mode!=2 && mode!=3 && mode!=15))return 2;
    if(opt & ~(0x10|0x20|0x40))return 2;
    portENTER_CRITICAL(&mux_);
    auto slot=pending_.reserve(mode,dst,de,se,id,p,n);
    portEXIT_CRITICAL(&mux_);if(!slot)return 0x11;
    esp_zb_apsde_data_req_t r={};r.dst_addr_mode=mode==15?2:mode;if(mode==3)memcpy(r.dst_addr.addr_long,dst,8);else r.dst_addr.addr_short=znp::get16(dst);
    r.dst_endpoint=de;r.src_endpoint=se;r.profile_id=profile(se);r.cluster_id=cl;r.asdu_length=n;r.asdu=slot->data;r.radius=rad;
    r.tx_options=(opt&0x10?ESP_ZB_APSDE_TX_OPT_ACK_TX:0)|(opt&0x40?ESP_ZB_APSDE_TX_OPT_SECURITY_ENABLED:0);
    esp_zb_lock_acquire(portMAX_DELAY);auto result=esp_zb_aps_data_request(&r);esp_zb_lock_release();
    if(result!=ESP_OK){portENTER_CRITICAL(&mux_);slot->busy=false;portEXIT_CRITICAL(&mux_);return 1;}return 0;
}
void EspRadio::confirmation(esp_zb_apsde_data_confirm_t c){
    auto self=instance;if(!self)return;ApsConfirm out={};bool matched=false;
    portENTER_CRITICAL(&self->mux_);
    uint8_t dst[8]={};if(c.dst_addr_mode==3)memcpy(dst,c.dst_addr.addr_long,8);else znp::put16(dst,c.dst_addr.addr_short);
    out.status=c.status;out.endpoint=c.src_endpoint;
    matched=self->pending_.take(c.dst_addr_mode,dst,c.dst_endpoint,c.src_endpoint,c.asdu,c.asdu_length,out.transaction_id);
    portEXIT_CRITICAL(&self->mux_);
    if(matched && xQueueSend(self->confirmation_queue_,&out,0)!=pdTRUE)++self->dropped_;
}
bool EspRadio::indication(esp_zb_apsde_data_ind_t i){
    auto self=instance;if(!self || i.status || !i.asdu || i.asdu_length>220 || !i.dst_endpoint)return false;
    ApsRx r={};r.src_addr=i.src_short_addr;r.cluster_id=i.cluster_id;r.src_ep=i.src_endpoint;r.dst_ep=i.dst_endpoint;r.lqi=znp::lqiToByte(i.lqi);
    r.group=i.dst_addr_mode==1?i.dst_short_addr:0;r.broadcast=i.dst_short_addr>=0xFFF8;r.security=i.security_status!=0;r.timestamp=micros()/32;r.len=uint8_t(i.asdu_length);memcpy(r.data,i.asdu,r.len);
#if ZNP_LQI_DIAGNOSTICS
    znp::lqi_diag::Record record;
    record.rx_ms=millis();record.sdk_lqi_raw=i.lqi;record.source=i.src_short_addr;
    record.cluster=i.cluster_id;record.profile=i.profile_id;record.payload_length=uint16_t(i.asdu_length);
    record.src_ep=i.src_endpoint;record.dst_ep=i.dst_endpoint;record.sdk_status=i.status;record.bridge_lqi=r.lqi;
    record.flags=znp::lqiInByteRange(i.lqi)?znp::lqi_diag::RawInByteRange:0;
    r.lqi_diagnostic_id=znp::lqi_diag::capture(record);
#endif
    const bool queued=xQueueSend(self->rx_queue_,&r,0)==pdTRUE;
    if(!queued)++self->dropped_;
#if ZNP_LQI_DIAGNOSTICS
    znp::lqi_diag::queued(r.lqi_diagnostic_id,queued);
#endif
    return true; // The serial host owns ZCL responses; suppress duplicate SDK processing.
}
