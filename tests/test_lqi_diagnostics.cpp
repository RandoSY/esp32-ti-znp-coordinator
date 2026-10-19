#include "LqiDiagnostics.h"
#include "LqiMetadata.h"
#include "ZnpBridge.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace znp::lqi_diag;
static uint32_t u32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
struct Radio:RadioBackend {
    ApsRx rx={};bool ready=false;
    RadioSnapshot snapshot()override{return {};}
    uint8_t permitJoin(uint8_t)override{return 0x12;}
    uint8_t sendAps(uint16_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override{return 0x12;}
    bool popRx(ApsRx &out)override{if(!ready)return false;out=rx;ready=false;return true;}
    bool popConfirm(ApsConfirm &)override{return false;}
};
static void writer(const uint8_t *p,size_t n,void *ctx){static_cast<std::vector<std::vector<uint8_t>> *>(ctx)->emplace_back(p,p+n);}
int main(){
    Ring local;Record record;Record got;
    assert(!local.get(0,got));local.encoded(0,0,0);local.queued(0,false);assert(local.queueDrops()==0);
    for(unsigned i=0;i<Capacity+3;++i){record.sdk_lqi_raw=int(i);const auto id=local.capture(record);local.queued(id,i!=0);}
    assert(local.count()==Capacity && local.overwritten()==3 && local.queueDrops()==1);
    assert(!local.get(1,got) && local.get(4,got) && got.sdk_lqi_raw==3);
    local.encoded(4,30,99);assert(local.get(4,got) && got.znp_lqi==30 && got.znp_sequence==99 && got.flags==(Queued|Encoded));
    local.encoded(1,173,1);assert(local.get(65,got) && !(got.flags&Encoded));
    record={};record.sdk_lqi_raw=-1;record.source=0xBEEF;record.cluster=0x0500;record.profile=0x0104;record.rx_ms=1234;record.bridge_lqi=znp::lqiToByte(-1);
    const auto id=capture(record);queued(id,true);
    Radio radio;radio.rx.lqi_diagnostic_id=id;radio.rx.lqi=0;radio.ready=true;
    std::vector<std::vector<uint8_t>> frames;ZnpBridge bridge(radio,writer,&frames);assert(bridge.begin(false));bridge.poll(10);
    assert(frames.size()==1 && frames[0][13]==0);
    uint8_t response[30],request[5]={1};znp::put32(request+1,id);
    assert(read(request,5,response)==30 && response[0]==0 && response[1]==Schema);
    assert(u32(response+2)==id && u32(response+6)==1234 && u32(response+10)==0xFFFFFFFFU);
    assert(znp::get16(response+14)==0xBEEF && znp::get16(response+16)==0x0500 && znp::get16(response+18)==0x0104);
    assert(response[25]==0 && response[26]==0 && response[27]==1 && response[28]==(Queued|Encoded));
    assert(read(nullptr,0,response)==1 && response[0]==2);
    request[0]=1;znp::put32(request+1,0);assert(read(request,5,response)==1 && response[0]==9);
    request[0]=0;assert(read(request,1,response)==16 && response[2]==Capacity && u32(response+4)==id);
    // The vendor diagnostic request is read-only and is emitted through normal
    // binary framing; it must not increment ordinary protocol-error counters.
    frames.clear();uint8_t req[]={0xFE,1,0x27,0xF1,0,0xD7};for(auto b:req)bridge.input(b,11);
    assert(frames.size()==1 && frames[0][2]==0x67 && frames[0][3]==0xF1 && frames[0][4]==0 && bridge.protocolErrors()==0);
    puts("PASS: diagnostic ring bounds/overwrite, signed raw preservation, queue/encoder correlation and read-only binary retrieval");
}
