#pragma once
#include <stdint.h>
#include <stddef.h>
#ifndef ZNP_LQI_DIAGNOSTICS
#define ZNP_LQI_DIAGNOSTICS 0
#endif
#if ZNP_LQI_DIAGNOSTICS
namespace znp { namespace lqi_diag {
constexpr uint8_t Schema=1,Capacity=64,RecordBytes=28;
enum Flags:uint8_t {RawInByteRange=1,Queued=2,Encoded=4};
struct Record {
    uint32_t id=0,rx_ms=0;
    int32_t sdk_lqi_raw=0;
    uint16_t source=0,cluster=0,profile=0,payload_length=0;
    uint8_t src_ep=0,dst_ep=0,sdk_status=0,bridge_lqi=0;
    uint8_t znp_lqi=0,znp_sequence=0,flags=0,reserved=0;
};
static_assert(sizeof(Record)==RecordBytes,"Unexpected diagnostic record padding");
// Fixed memory; no payloads, keys, persistent writes, timers or logging.
// Ring operations require the caller's lock. Global wrappers below provide it.
class Ring {
    Record records_[Capacity]={};
    uint32_t last_id_=0,overwritten_=0,queue_drops_=0;
    uint8_t count_=0;
public:
    uint32_t capture(Record record){
        ++last_id_;if(!last_id_)++last_id_;
        record.id=last_id_;
        if(count_<Capacity)++count_;else ++overwritten_;
        records_[(last_id_-1)%Capacity]=record;return last_id_;
    }
    void queued(uint32_t id,bool ok){
        if(!id)return;
        if(!ok)++queue_drops_;
        auto &r=records_[(id-1)%Capacity];if(r.id==id && ok)r.flags|=Queued;
    }
    void encoded(uint32_t id,uint8_t lqi,uint8_t sequence){
        if(!id)return;
        auto &r=records_[(id-1)%Capacity];if(r.id!=id)return;
        r.znp_lqi=lqi;r.znp_sequence=sequence;r.flags|=Encoded;
    }
    bool get(uint32_t id,Record &out)const{
        if(!id)return false;
        const auto &r=records_[(id-1)%Capacity];if(r.id!=id)return false;out=r;return true;
    }
    uint32_t lastId()const{return last_id_;}
    uint32_t overwritten()const{return overwritten_;}
    uint32_t queueDrops()const{return queue_drops_;}
    uint8_t count()const{return count_;}
};
uint32_t capture(const Record &);
void queued(uint32_t,bool);
void encoded(uint32_t,uint8_t,uint8_t);
// Read-only UTIL F1: request 00 -> summary; request 01 + uint32 id -> record.
// Status 0=success, 2=bad request, 9=record no longer available. OFF: no command.
uint8_t read(const uint8_t *,uint8_t,uint8_t *);
}}
#endif
