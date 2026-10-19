#include "ZnpNv.h"
#include "ZnpFrame.h"
#include <string.h>
namespace {
NvKey osal(uint16_t id){return {0,id,0};}
struct NibWriter {
    std::vector<uint8_t> b;
    void u8(uint8_t v){b.push_back(v);}
    void align(){if(b.size()&1)b.push_back(0);}
    void u16(uint16_t v){align();u8(uint8_t(v));u8(uint8_t(v>>8));}
    void u32(uint32_t v){align();for(unsigned i=0;i<4;++i)u8(uint8_t(v>>(8*i)));}
    void raw(const uint8_t *p,size_t n){b.insert(b.end(),p,p+n);}
};
}
bool ZnpNv::seed(uint16_t id,const uint8_t *p,size_t n){
    std::vector<uint8_t> v;return store_.get(osal(id),v) || store_.put(osal(id),p,n);
}
bool ZnpNv::begin(const RadioSnapshot &s){
    uint8_t zero[24]={}, ff[8];memset(ff,0xFF,sizeof(ff));
    const uint8_t channel[]={0,0xF8,0xFF,7},pan[]={0xFF,0xFF};
    // NWKKEY's 24-byte layout identifies a word-aligned TI ARM personality.
    bool ok=seed(0x82,zero,24) && seed(1,s.ieee,8) && seed(3,zero,1);
    ok=ok && seed(0x84,channel,4) && seed(0x83,pan,2) && seed(0x2D,ff,8);
    ok=ok && seed(0x47,ff,8) && seed(0x87,zero,1) && seed(0x8F,zero,1);
    ok=ok && seed(0x62,zero,16) && seed(0x63,zero,1);
    ok=ok && seed(0x3A,zero,18) && seed(0x3B,zero,18);
    return ok;
}
bool ZnpNv::clearNetwork(){
    // Durable erase intent survives the MCU reset until the SDK consumes it.
    const uint8_t yes=1,zero=0;
    if(!store_.put({0xFE,1,0},&yes,1))return false;
    std::vector<uint8_t> v;
    for(uint16_t id:{uint16_t(0x21),uint16_t(0x60)})
        if(store_.get(osal(id),v) && !store_.erase(osal(id)))return false;
    return store_.put(osal(3),&zero,1);
}
NetworkConfig ZnpNv::config(){
    NetworkConfig c;std::vector<uint8_t> v;
    if(store_.get(osal(1),v) && v.size()==8)memcpy(c.ieee,v.data(),8);
    if(store_.get(osal(0x83),v) && v.size()==2)c.pan_id=znp::get16(v.data());
    if(store_.get(osal(0x84),v) && v.size()==4)c.primary_channels=uint32_t(v[0])|(uint32_t(v[1])<<8)|(uint32_t(v[2])<<16)|(uint32_t(v[3])<<24);
    if(store_.get(osal(0x2D),v) && v.size()==8)memcpy(c.extended_pan,v.data(),8);
    if(store_.get(osal(0x62),v) && v.size()==16)memcpy(c.network_key,v.data(),16);
    // Persisted security material from this personality, including restored counters.
    if(store_.get({1,7,0},v) && v.size()>=12)c.frame_counter=uint32_t(v[0])|(uint32_t(v[1])<<8)|(uint32_t(v[2])<<16)|(uint32_t(v[3])<<24);
    c.erase=store_.get({0xFE,1,0},v) && v[0]!=0;
    return c;
}
bool ZnpNv::sync(const RadioSnapshot &s){
    if(!s.started)return true;
    NibWriter w;
    // TI NIB, 16-bit aligned, in the exact field order consumed by herdsman.
    const uint8_t first[]={0,3,2,32,15,32,0,9,0,2,0,1,5,1,0x8F};w.raw(first,sizeof(first));
    w.u16(500);w.u8(2);w.u8(13);w.u8(30);w.u16(s.short_addr);w.u8(s.channel);
    w.u16(0);w.raw(s.ieee,8);w.u16(s.pan_id);w.u8(9);
    w.u32(uint32_t(1)<<s.channel);w.u8(15);w.u8(15);w.u8(4);w.u8(0);
    w.u32(0);w.u32(0);w.u8(0);w.raw(s.extended_pan,8);w.u8(1);
    for(unsigned i=0;i<2;++i){w.u8(s.key_sequence);w.raw(s.network_key,16);}
    for(uint8_t v:{0,0,15,3,0,1,0,30,1})w.u8(v);
    w.u16(0);w.u16(0);w.u8(s.update_id);w.align();
    auto putChanged=[this](NvKey k,const uint8_t *p,size_t n){
        std::vector<uint8_t> old;
        if(store_.get(k,old) && old.size()==n && !memcmp(old.data(),p,n))return true;
        return store_.put(k,p,n);
    };
    uint8_t key[18]={};key[0]=s.key_sequence;memcpy(key+1,s.network_key,16);
    uint8_t sec[12];znp::put32(sec,s.frame_counter);memcpy(sec+4,s.extended_pan,8);
    uint8_t nk[24]={};memcpy(nk,s.network_key,16);znp::put32(nk+16,s.frame_counter);nk[20]=s.key_sequence;
    bool ok=putChanged(osal(0x21),w.b.data(),w.b.size()) && putChanged(osal(0x3A),key,18)
        && putChanged(osal(0x3B),key,18) && putChanged(osal(0x82),nk,24) && putChanged({1,7,0},sec,12);
    std::vector<uint8_t> intent;
    if(ok && store_.get({0xFE,1,0},intent))ok=store_.erase({0xFE,1,0});
    return ok;
}
