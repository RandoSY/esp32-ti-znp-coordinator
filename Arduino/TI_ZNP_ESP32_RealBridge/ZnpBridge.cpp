#include "ZnpBridge.h"
#include "ZnpIdentity.h"
#include <string.h>
#include <algorithm>
namespace {
constexpr uint8_t SUCCESS=0,FAILURE=1,INVALID_PARAMETER=2,NOT_SUPPORTED=0x12,NOT_ACTIVE=0x83;
uint32_t get32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
ZnpBridge::ZnpBridge(RadioBackend &r,znp::Writer w,void *c,NvStore *s)
 :radio_(r),tx_(w),tx_ctx_(c),decoder_(*this),nv_(s?*s:fallback_){}
bool ZnpBridge::begin(bool notify){
    initialized_=nv_.begin(radio_.snapshot());storage_fault_=!initialized_;
    if(notify){uint8_t p[6];znp::resetInd(p);znp::write(tx_,tx_ctx_,0x41,0x80,p,6);}
    return initialized_;
}
void ZnpBridge::input(uint8_t b,uint32_t ms){now_=ms;if(!resetting_)decoder_.input(b,ms);}
void ZnpBridge::reply(uint8_t s,uint8_t c,const uint8_t *p,uint8_t n){znp::write(tx_,tx_ctx_,uint8_t(0x60|s),c,p,n);}
void ZnpBridge::status(uint8_t s,uint8_t c,uint8_t v){if(v)++errors_;reply(s,c,&v,1);}
void ZnpBridge::rpcError(const znp::Frame &f,uint8_t e){++errors_;const uint8_t p[]={e,f.cmd0,f.cmd1};reply(0,0,p,3);}
void ZnpBridge::onFrame(const znp::Frame &f){
    if(!initialized_ && !begin())return;
    const uint8_t type=f.cmd0&0xE0,s=f.cmd0&0x1F;
    if(type==0x40 && s==1 && f.cmd1==0){
        if(f.len!=1 || f.data[0]>1){rpcError(f,4);return;}
        std::vector<uint8_t> v;const bool clear=nv_.read({0,3,0},v) && (v[0]&3);
        if(clear && !nv_.clearNetwork()){storage_fault_=true;return;}
        if(radio_.reset(clear)==0)resetting_=true;
        return;
    }
    if(type!=0x20)return;
    switch(s){case 1:sys(f);break;case 4:af(f);break;case 5:zdo(f);break;case 7:util(f);break;case 15:app(f);break;default:rpcError(f,1);}
}
bool ZnpBridge::nvCommand(const znp::Frame &f){
    const uint8_t c=f.cmd1;const uint8_t *p=f.data;
    const bool ext=c>=0x30 && c<=0x35;
    const bool legacy=c==7 || c==8 || c==9 || c==0x12 || c==0x13 || c==0x1C || c==0x1D;
    if(!ext && !legacy)return false;
    const unsigned base=ext?5:2;
    if(f.len<base){rpcError(f,4);return true;}
    NvKey k=ext?NvKey{p[0],znp::get16(p+1),znp::get16(p+3)}:NvKey{0,znp::get16(p),0};
    // Reflect live keys, PAN/channel and outgoing counter in host-visible records.
    if((k.system==0 && (k.item==0x21 || k.item==0x3A || k.item==0x3B || k.item==0x82)) || (k.system==1 && k.item==7))
        if(!nv_.sync(radio_.snapshot()))storage_fault_=true;
    std::vector<uint8_t> value;bool exists=nv_.read(k,value);
    if(c==0x13 || c==0x32){
        if(f.len!=base){rpcError(f,4);return true;}
        uint8_t out[2]={};if(exists)znp::put16(out,uint16_t(value.size()));
        reply(1,c,out,c==0x32?1:2);return true;
    }
    if(c==0x12 || c==0x31){
        if(f.len!=base+(ext?0:2)){rpcError(f,4);return true;}
        if(!exists){status(1,c,9);return true;}
        if(!ext && znp::get16(p+2)!=value.size()){status(1,c,0x0C);return true;}
        const bool ok=nv_.remove(k);storage_fault_|=!ok;status(1,c,ok?0:0x0A);return true;
    }
    if(c==7 || c==0x30){
        const unsigned header=ext?9:5;
        if(f.len<header){rpcError(f,4);return true;}
        const uint32_t size=ext?get32(p+5):znp::get16(p+2);
        const unsigned initial=ext?0:p[4];
        if(f.len!=header+initial || initial>size){rpcError(f,4);return true;}
        if(exists){status(1,c,0);return true;}
        if(!size || size>NvStore::MaxRecord || (ext && size>255)){status(1,c,0x0C);return true;}
        value.assign(size,0xFF);if(initial)memcpy(value.data(),p+header,initial);
        const bool ok=nv_.save(k,value.data(),value.size());storage_fault_|=!ok;
        status(1,c,ok?(ext?0:9):0x0A);return true;
    }
    if(c==8 || c==0x1C || c==0x33){
        const unsigned header=ext?8:(c==8?3:4);
        if(f.len!=header){rpcError(f,4);return true;}
        const unsigned offset=ext?znp::get16(p+5):(c==8?p[2]:znp::get16(p+2));
        const unsigned requested=ext?p[7]:248;
        uint8_t out[250]={};
        if(!exists || offset>value.size()){out[0]=!exists?9:2;reply(1,c,out,2);return true;}
        const unsigned n=std::min<unsigned>(248,std::min<unsigned>(requested,unsigned(value.size())-offset));
        out[1]=uint8_t(n);if(n)memcpy(out+2,value.data()+offset,n);reply(1,c,out,uint8_t(n+2));return true;
    }
    const unsigned header=ext?(c==0x35?6:8):(c==9?4:6);
    if(f.len<header){rpcError(f,4);return true;}
    const unsigned offset=ext?(c==0x35?0:znp::get16(p+5)):(c==9?p[2]:znp::get16(p+2));
    const unsigned n=c==0x1D?znp::get16(p+4):p[header-1];
    if(f.len!=header+n){rpcError(f,4);return true;}
    if(!exists || offset>value.size() || n>value.size()-offset){status(1,c,!exists?9:2);return true;}
    if(n)memcpy(value.data()+offset,p+header,n);
    const bool ok=nv_.save(k,value.data(),value.size());storage_fault_|=!ok;status(1,c,ok?0:0x0A);return true;
}
void ZnpBridge::sys(const znp::Frame &f){
    if(nvCommand(f))return;
    if(f.cmd1==1 && !f.len){const uint8_t p[]={0x59,0x80};reply(1,1,p,2);return;}
    if(f.cmd1==2 && !f.len){uint8_t p[9];znp::version(p);reply(1,2,p,9);return;}
    if(f.cmd1==4 && !f.len){std::vector<uint8_t> v;if(nv_.read({0,1,0},v))reply(1,4,v.data(),8);return;}
    if(f.cmd1==5 && f.len==8){const bool ok=nv_.save({0,1,0},f.data,8);storage_fault_|=!ok;status(1,5,ok?0:0x0A);return;}
    if(f.cmd1==0x0F && f.len==2){status(1,f.cmd1,radio_.txPower(int8_t(f.data[1])));return;}
    if(f.cmd1==0x14 && f.len==1){uint8_t v=f.data[0];if(radio_.txPower(int8_t(v)))v=0;reply(1,f.cmd1,&v,1);return;}
    rpcError(f,2);
}
void ZnpBridge::util(const znp::Frame &f){
#if ZNP_LQI_DIAGNOSTICS
    if(f.cmd1==0xF1){uint8_t p[30];const uint8_t n=znp::lqi_diag::read(f.data,f.len,p);reply(7,0xF1,p,n);return;}
#endif
    if(f.cmd1==0xF0 && !f.len){uint8_t p[4];znp::put32(p,radio_.diagnostic());reply(7,0xF0,p,4);return;}
    if(f.cmd1==0 && !f.len){
        auto s=radio_.snapshot();uint8_t p[14]={};std::vector<uint8_t> v;
        if(nv_.read({0,1,0},v))memcpy(p+1,v.data(),8);
        znp::put16(p+9,s.started?s.short_addr:0xFFFE);p[11]=1;p[12]=s.started?9:1;
        reply(7,0,p,14);return;
    }
    // LED command is a real host preference, independent of diagnostic RGB.
    if(f.cmd1==0x0A && f.len==2){status(7,f.cmd1,0);return;}
    rpcError(f,2);
}
void ZnpBridge::app(const znp::Frame &f){
    if(f.cmd1==8 && f.len==5){
        const uint32_t channels=get32(f.data+1);
        if(f.data[0]>1 || (channels&~uint32_t(0x07FFF800)) || (f.data[0] && !channels)){status(15,8,2);return;}
        if(f.data[0]){const bool ok=nv_.save({0,0x84,0},f.data+1,4);storage_fault_|=!ok;status(15,8,ok?0:0x0A);}
        else status(15,8,channels==0?0:NOT_SUPPORTED);
        return;
    }
    if(f.cmd1==5 && f.len==1){
        if(!(f.data[0]&4)){status(15,5,NOT_SUPPORTED);return;}
        status(15,5,radio_.start(nv_.config(),true));return;
    }
    if(f.cmd1==4 && f.len>=10){status(15,4,radio_.installCode(f.data+1,f.data+9,uint8_t(f.len-9)));return;}
    rpcError(f,2);
}
void ZnpBridge::zdo(const znp::Frame &f){
    // Local ZDO queries use the descriptors actually registered with the backend.
    // Do not loop a request through the SDK's on-air transaction machinery.
    if(f.cmd1==4 && f.len==5 && znp::get16(f.data)==0 && znp::get16(f.data+2)==0){
        if(!radio_.snapshot().started){status(5,4,NOT_ACTIVE);return;}
        EndpointDescriptor e={};uint8_t p[80]={};unsigned at=6;
        const bool found=radio_.descriptor(f.data[4],e);
        p[2]=found?0:0x82;
        if(found){p[at++]=e.endpoint;znp::put16(p+at,e.profile);at+=2;znp::put16(p+at,e.device);at+=2;p[at++]=e.version;
            p[at++]=e.in_count;for(unsigned i=0;i<e.in_count;++i){znp::put16(p+at,e.in[i]);at+=2;}
            p[at++]=e.out_count;for(unsigned i=0;i<e.out_count;++i){znp::put16(p+at,e.out[i]);at+=2;}p[5]=uint8_t(at-6);}
        status(5,4,0);znp::write(tx_,tx_ctx_,0x45,0x84,p,uint8_t(at));return;
    }
    if(f.cmd1==0x40 && f.len==2){status(5,0x40,radio_.start(nv_.config(),false));return;}
    if(f.cmd1==0x50 && !f.len){
        auto s=radio_.snapshot();uint8_t p[24]={};znp::put16(p,s.started?s.short_addr:0xFFFE);p[2]=s.started?9:1;
        znp::put16(p+3,s.pan_id);znp::put16(p+5,0);memcpy(p+7,s.extended_pan,8);p[23]=s.channel;reply(5,0x50,p,24);return;
    }
    if(f.cmd1==5 && f.len==4 && znp::get16(f.data)==0 && znp::get16(f.data+2)==0){
        if(!radio_.snapshot().started){status(5,5,NOT_ACTIVE);return;}
        uint8_t p[254]={};p[5]=radio_.endpoints(p+6);status(5,5,0);znp::write(tx_,tx_ctx_,0x45,0x85,p,uint8_t(6+p[5]));return;
    }
    if(f.cmd1==0x36 && f.len==5 && znp::get16(f.data+1)==0){
        if((f.data[0]!=2 && f.data[0]!=15) || f.data[4]>1){status(5,0x36,INVALID_PARAMETER);return;}
        if(!radio_.snapshot().started){status(5,0x36,NOT_ACTIVE);return;}
        const uint8_t code=radio_.permitJoin(f.data[3]);status(5,0x36,code);
        if(!code){const uint8_t p[]={0,0,0};znp::write(tx_,tx_ctx_,0x45,0xB6,p,3);}return;
    }
    if(f.cmd1==0x4A && f.len==3){
        const uint8_t result=radio_.group(f.data[0],znp::get16(f.data+1),false);
        const uint8_t p[]={result,f.data[1],f.data[2],0};reply(5,0x4A,p,4);return;
    }
    if(f.cmd1==0x4B && f.len>=4 && f.len==unsigned(4+f.data[3])){status(5,0x4B,radio_.group(f.data[0],znp::get16(f.data+1),true));return;}
    status(5,f.cmd1,radio_.zdoRequest(f.cmd1,f.data,f.len));
}
void ZnpBridge::af(const znp::Frame &f){
    if(f.cmd1==0){
        if(f.len<9){rpcError(f,4);return;}
        const unsigned split=8+2*f.data[7];
        if(split>=f.len || f.data[7]>16 || f.data[split]>16 || split+1+2*f.data[split]!=f.len){rpcError(f,4);return;}
        EndpointDescriptor e={};e.endpoint=f.data[0];e.profile=znp::get16(f.data+1);e.device=znp::get16(f.data+3);
        e.version=f.data[5];e.latency=f.data[6];e.in_count=f.data[7];e.out_count=f.data[split];
        for(unsigned i=0;i<e.in_count;++i)e.in[i]=znp::get16(f.data+8+2*i);
        for(unsigned i=0;i<e.out_count;++i)e.out[i]=znp::get16(f.data+split+1+2*i);
        status(4,0,radio_.endpoint(e));return;
    }
    if(f.cmd1==1){
        if(f.len<10 || f.len!=unsigned(10+f.data[9])){rpcError(f,4);return;}
        if(!radio_.snapshot().started){status(4,1,NOT_ACTIVE);return;}
        status(4,1,radio_.sendAps(znp::get16(f.data),f.data[2],f.data[3],znp::get16(f.data+4),f.data[6],f.data[7],f.data[8],f.data+10,f.data[9]));return;
    }
    if(f.cmd1==2){
        if(f.len<20 || znp::get16(f.data+18)>220 || f.len!=20+znp::get16(f.data+18)){rpcError(f,4);return;}
        status(4,2,radio_.sendApsExt(f.data[0],f.data+1,f.data[9],f.data[12],znp::get16(f.data+13),f.data[15],f.data[16],f.data[17],f.data+20,uint8_t(znp::get16(f.data+18))));return;
    }
    rpcError(f,2);
}
void ZnpBridge::emitIncoming(const ApsRx &r){
    if(r.len>220)return;
    uint8_t p[237]={};znp::put16(p,r.group);znp::put16(p+2,r.cluster_id);znp::put16(p+4,r.src_addr);
    p[6]=r.src_ep;p[7]=r.dst_ep;p[8]=r.broadcast;p[9]=r.lqi;p[10]=r.security;
    znp::put32(p+11,r.timestamp);p[15]=++next_seq_;p[16]=r.len;memcpy(p+17,r.data,r.len);
#if ZNP_LQI_DIAGNOSTICS
    znp::lqi_diag::encoded(r.lqi_diagnostic_id,p[9],p[15]);
#endif
    znp::write(tx_,tx_ctx_,0x44,0x81,p,uint8_t(17+r.len));
}
void ZnpBridge::poll(uint32_t ms){
    now_=ms;decoder_.poll(ms);radio_.service();
    if(resetting_)return;
    RadioEvent e;unsigned budget=16;
    while(budget-- && radio_.popEvent(e)){
        if(e.subsystem==5 && e.command==0xC0 && e.len==1 && e.data[0]==9)
            if(!nv_.sync(radio_.snapshot()))storage_fault_=true;
        znp::write(tx_,tx_ctx_,uint8_t(0x40|e.subsystem),e.command,e.data,e.len);
    }
    ApsRx rx;budget=16;while(budget-- && radio_.popRx(rx))emitIncoming(rx);
    ApsConfirm c;budget=16;while(budget-- && radio_.popConfirm(c)){
        const uint8_t p[]={c.status,c.endpoint,c.transaction_id};znp::write(tx_,tx_ctx_,0x44,0x80,p,3);
    }
}
