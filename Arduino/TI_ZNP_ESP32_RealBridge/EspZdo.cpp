#include "EspRadio.h"
#include "ZnpFrame.h"
#include <algorithm>
#include <new>
extern "C" {
#include "zboss_api.h"
}
namespace {
using C=EspRadio::Context;
void done(C *c,uint8_t status,const uint8_t *p=nullptr,size_t n=0,bool source=true){
    uint8_t out[250]={};unsigned at=source?2:0;if(source)znp::put16(out,c->dst);out[at++]=status;
    if(n>sizeof(out)-at){status=0x89;out[at-1]=status;n=0;}
    if(n)memcpy(out+at,p,n);c->self->event(uint8_t(c->command|0x80),out,at+n);delete c;
}
void addrCb(esp_zb_zdp_status_t code,esp_zb_zdo_nwk_addr_rsp_t *r,void *ctx){
    auto c=static_cast<C *>(ctx);uint8_t p[247]={};unsigned n=12;
    if(!code && r){memcpy(p,r->ieee_addr,8);znp::put16(p+8,r->nwk_addr);
        if(r->ext_resp){p[10]=r->ext_resp->start_index;p[11]=r->ext_resp->total;unsigned count=std::min<unsigned>(r->ext_resp->count,117);
            for(unsigned i=0;i<count;++i)znp::put16(p+12+2*i,r->ext_resp->nwk_addresses[i]);n+=2*count;}}
    done(c,uint8_t(code),p,n,false);
}
uint16_t routeDestination;bool routePending=false;
void routeCb(zb_uint8_t b){
    uint8_t p[250]={};znp::put16(p,routeDestination);unsigned n=zb_buf_len(b);const auto raw=static_cast<uint8_t *>(zb_buf_begin(b));
    if(n>1){n=std::min<unsigned>(n-1,248);memcpy(p+2,raw+1,n);if(EspRadio::instance)EspRadio::instance->event(0xB2,p,n+2);}
    zb_buf_free(b);routePending=false;
}
}
uint8_t EspRadio::zdoRequest(uint8_t cmd,const uint8_t *p,uint8_t n){
    if(!cached_.started)return 0x83;
    bool valid=false;
    switch(cmd){
    case 0:valid=n==10;break;case 1:valid=n==4;break;
    case 2:case 3:case 5:valid=n==4;break;
    case 4:valid=n==5;break;
    case 0x21:case 0x22:valid=n==23;break;
    case 0x31:case 0x32:case 0x33:valid=n==3;break;
    case 0x34:valid=n==11;break;case 0x36:valid=n==5 && (p[0]==2 || p[0]==15);break;
    case 0x37:valid=n==11;break;
    default:return 0x12;
    }
    if(!valid)return 2;
    if(cmd==0x36 && znp::get16(p+1)>=0xFFF8){const uint8_t r=permitJoin(p[3]);if(r)return r;}
    C *c=new(std::nothrow) C{this,znp::get16(p),cmd};if(!c)return 0x10;
    esp_zb_lock_acquire(portMAX_DELAY);
    switch(cmd){
    case 0:{esp_zb_zdo_nwk_addr_req_param_t r={};r.dst_nwk_addr=0xFFFD;memcpy(r.ieee_addr_of_interest,p,8);r.request_type=p[8];r.start_index=p[9];esp_zb_zdo_nwk_addr_req(&r,addrCb,c);break;}
    case 1:{esp_zb_zdo_ieee_addr_req_param_t r={};r.dst_nwk_addr=znp::get16(p);r.addr_of_interest=r.dst_nwk_addr;r.request_type=p[2];r.start_index=p[3];esp_zb_zdo_ieee_addr_req(&r,addrCb,c);break;}
    case 2:{esp_zb_zdo_node_desc_req_param_t r={};r.dst_nwk_addr=znp::get16(p);
        esp_zb_zdo_node_desc_req(&r,[](esp_zb_zdp_status_t status,uint16_t addr,esp_zb_af_node_desc_t *desc,void *ctx){
            auto c=static_cast<C *>(ctx);uint8_t b[15]={};znp::put16(b,addr);if(!status && desc)memcpy(b+2,desc,13);done(c,uint8_t(status),b,status?2:15);
        },c);break;}
    case 3:{esp_zb_zdo_power_desc_req_param_t r={};r.dst_nwk_addr=znp::get16(p);
        esp_zb_zdo_power_desc_req(&r,[](esp_zb_zdo_power_desc_rsp_t *r,void *ctx){auto c=static_cast<C *>(ctx);uint8_t b[4]={};znp::put16(b,r->nwk_addr_of_interest);memcpy(b+2,&r->desc,2);done(c,r->status,b,4);},c);break;}
    case 4:{esp_zb_zdo_simple_desc_req_param_t r={};r.addr_of_interest=znp::get16(p+2);r.endpoint=p[4];
        esp_zb_zdo_simple_desc_req(&r,[](esp_zb_zdp_status_t status,esp_zb_af_simple_desc_1_1_t *d,void *ctx){
            auto c=static_cast<C *>(ctx);uint8_t b[247]={};znp::put16(b,c->dst);unsigned at=3;
            if(!status && d){unsigned count=d->app_input_cluster_count+d->app_output_cluster_count;if(count>115){done(c,0x89);return;}
                b[2]=uint8_t(8+2*count);b[at++]=d->endpoint;znp::put16(b+at,d->app_profile_id);at+=2;znp::put16(b+at,d->app_device_id);at+=2;b[at++]=d->app_device_version;
                b[at++]=d->app_input_cluster_count;for(unsigned i=0;i<d->app_input_cluster_count;++i){znp::put16(b+at,d->app_cluster_list[i]);at+=2;}
                b[at++]=d->app_output_cluster_count;for(unsigned i=d->app_input_cluster_count;i<count;++i){znp::put16(b+at,d->app_cluster_list[i]);at+=2;}}
            done(c,uint8_t(status),b,at);
        },c);break;}
    case 5:{esp_zb_zdo_active_ep_req_param_t r={};r.addr_of_interest=znp::get16(p+2);
        esp_zb_zdo_active_ep_req(&r,[](esp_zb_zdp_status_t status,uint8_t count,uint8_t *eps,void *ctx){auto c=static_cast<C *>(ctx);uint8_t b[247]={};znp::put16(b,c->dst);b[2]=status?0:count;
            if(count>244){done(c,0x89);return;}if(!status && count)memcpy(b+3,eps,count);done(c,uint8_t(status),b,status?3:3+count);},c);break;}
    case 0x21:case 0x22:{esp_zb_zdo_bind_req_param_t r={};r.req_dst_addr=znp::get16(p);memcpy(r.src_address,p+2,8);r.src_endp=p[10];r.cluster_id=znp::get16(p+11);r.dst_addr_mode=p[13];
        if(r.dst_addr_mode==3)memcpy(r.dst_address_u.addr_long,p+14,8);else r.dst_address_u.addr_short=znp::get16(p+14);r.dst_endp=p[22];
        auto cb=[](esp_zb_zdp_status_t code,void *ctx){done(static_cast<C *>(ctx),uint8_t(code));};
        if(cmd==0x21)esp_zb_zdo_device_bind_req(&r,cb,c);else esp_zb_zdo_device_unbind_req(&r,cb,c);break;}
    case 0x31:{esp_zb_zdo_mgmt_lqi_req_param_t r={};r.dst_addr=znp::get16(p);r.start_index=p[2];
        esp_zb_zdo_mgmt_lqi_req(&r,[](const esp_zb_zdo_mgmt_lqi_rsp_t *r,void *ctx){auto c=static_cast<C *>(ctx);uint8_t b[247]={r->neighbor_table_entries,r->start_index,r->neighbor_table_list_count};unsigned at=3;
            if(r->neighbor_table_list_count>11){done(c,0x89);return;}
            for(unsigned i=0;i<r->neighbor_table_list_count;++i){const auto &v=r->neighbor_table_list[i];memcpy(b+at,v.extended_pan_id,8);at+=8;memcpy(b+at,v.extended_addr,8);at+=8;
                znp::put16(b+at,v.network_addr);at+=2;b[at++]=v.device_type|(v.rx_when_idle<<2)|(v.relationship<<4);b[at++]=v.permit_join;b[at++]=v.depth;b[at++]=v.lqi;}
            done(c,r->status,b,at);},c);break;}
    case 0x32:{
        if(routePending){esp_zb_lock_release();delete c;return 0x11;}
        zb_bufid_t b=zb_buf_get_out();if(!b){esp_zb_lock_release();delete c;return 0x10;}
        auto r=ZB_BUF_GET_PARAM(b,zb_zdo_mgmt_rtg_param_t);r->dst_addr=c->dst;r->start_index=p[2];routeDestination=c->dst;routePending=true;
        const auto seq=zb_zdo_mgmt_rtg_req(b,routeCb);delete c;if(seq==0xFF){routePending=false;zb_buf_free(b);esp_zb_lock_release();return 0x11;}break;}
    case 0x33:{esp_zb_zdo_mgmt_bind_param_t r={};r.dst_addr=c->dst;r.start_index=p[2];
        esp_zb_zdo_binding_table_req(&r,[](const esp_zb_zdo_binding_table_info_t *r,void *ctx){auto c=static_cast<C *>(ctx);uint8_t b[247]={r->total,r->index,0};unsigned at=3;
            for(auto rec=r->record;rec;rec=rec->next){unsigned size=rec->dst_addr_mode==3?21:14;if(at+size>sizeof(b)){done(c,0x89);return;}
                memcpy(b+at,rec->src_address,8);at+=8;b[at++]=rec->src_endp;znp::put16(b+at,rec->cluster_id);at+=2;b[at++]=rec->dst_addr_mode;
                if(rec->dst_addr_mode==3){memcpy(b+at,rec->dst_address.addr_long,8);at+=8;b[at++]=rec->dst_endp;}else{znp::put16(b+at,rec->dst_address.addr_short);at+=2;}++b[2];}
            done(c,r->status,b,at);},c);break;}
    case 0x34:{esp_zb_zdo_mgmt_leave_req_param_t r={};r.dst_nwk_addr=c->dst;memcpy(r.device_address,p+2,8);r.remove_children=(p[10]&0x40)!=0;r.rejoin=(p[10]&0x80)!=0;
        esp_zb_zdo_device_leave_req(&r,[](esp_zb_zdp_status_t status,void *ctx){done(static_cast<C *>(ctx),uint8_t(status));},c);break;}
    case 0x36:{esp_zb_zdo_permit_joining_req_param_t r={};r.dst_nwk_addr=znp::get16(p+1);r.permit_duration=p[3];r.tc_significance=p[4];c->dst=r.dst_nwk_addr;
        esp_zb_zdo_permit_joining_req(&r,[](esp_zb_zdp_status_t status,void *ctx){done(static_cast<C *>(ctx),uint8_t(status));},c);break;}
    case 0x37:{esp_zb_zdo_mgmt_nwk_update_req_param_t r={};r.dst_addr=c->dst;r.scan_channels=uint32_t(p[3])|(uint32_t(p[4])<<8)|(uint32_t(p[5])<<16)|(uint32_t(p[6])<<24);r.scan_duration=p[7];r.scan_count=p[8];r.nwk_manager_addr=znp::get16(p+9);c->command=0x38;
        esp_zb_zdo_mgmt_nwk_update_req(&r,[](const esp_zb_zdo_mgmt_update_notify_t *r,void *ctx){auto c=static_cast<C *>(ctx);uint8_t b[36]={};znp::put32(b,r->scanned_channels);znp::put16(b+4,r->total_transmission);znp::put16(b+6,r->transmission_failures);b[8]=r->scanned_channels_list_count;memcpy(b+9,r->energy_values,std::min<unsigned>(27,b[8]));done(c,r->status,b,9+std::min<unsigned>(27,b[8]));},c);break;}
    }
    esp_zb_lock_release();return 0;
}
