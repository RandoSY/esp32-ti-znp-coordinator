// Native g++ test suite: REAL Zigbee hardware is NOT involved.
// Tests the same ZNP wire parser and command router used on ESP32.
#include "ZnpBridge.h"
#include <assert.h>
#include <iostream>
#include <vector>
#include <cstring>
#include "ZnpIdentity.h"
struct FakeRadio:RadioBackend{
    RadioSnapshot state={};
    uint8_t last_permit=0xFF;
    unsigned sends=0;
    ApsRx pending_rx={}; bool rx_ready=false;
    ApsConfirm confirm={}; bool confirm_ready=false;
    FakeRadio(){
        state.started=true;state.short_addr=0;state.pan_id=0xABCD;state.channel=15;
        for(unsigned i=0;i<8;++i)state.ieee[i]=i+1;
    }
    RadioSnapshot snapshot() override {return state;}
    uint8_t endpoint(const EndpointDescriptor &e) override {return e.endpoint==1 && e.profile==0x0104?0:0x12;}
    uint8_t permitJoin(uint8_t sec) override {last_permit=sec;return 0;}
    uint8_t sendAps(uint16_t dst,uint8_t de,uint8_t se,uint16_t cl,
       uint8_t id,uint8_t opts,uint8_t radius,const uint8_t* payload,uint8_t len) override {
        assert(dst==0x1234 && de==1 && se==1 && cl==0x0500);
        assert(id==0x8A && len==3 && payload[0]==0x19 && radius==30 && opts==0);
        ++sends;return 0;
    }
    bool popRx(ApsRx &v)override {if(!rx_ready)return false;v=pending_rx;rx_ready=false;return true;}
    bool popConfirm(ApsConfirm &v)override {if(!confirm_ready)return false;v=confirm;confirm_ready=false;return true;}
};
struct Fixture{
    FakeRadio radio;
    std::vector<std::vector<uint8_t>> frames;
    ZnpBridge bridge;
    static void writer(const uint8_t* p,size_t n,void *ctx) {
        ((Fixture*)ctx)->frames.emplace_back(p,p+n);
    }
    Fixture():bridge(radio,&writer,this){}
    void feed(uint8_t cmd0,uint8_t cmd1,const std::vector<uint8_t>& data={},bool corrupt=false){
        std::vector<uint8_t> b; uint8_t fcs=data.size()^cmd0^cmd1;
        b.push_back(0xFE);b.push_back(uint8_t(data.size()));b.push_back(cmd0);b.push_back(cmd1);
        for(auto v:data){b.push_back(v);fcs^=v;}
        b.push_back(corrupt?uint8_t(fcs^1):fcs);
        for(uint8_t v:b)bridge.input(v,1000);
    }
    void expect(unsigned index, uint8_t cmd0, uint8_t cmd1, std::vector<uint8_t> payload){
        assert(index<frames.size());
        auto& f=frames[index];assert(f.size()==payload.size()+5);
        assert(f[0]==0xFE && f[1]==payload.size() && f[2]==cmd0 && f[3]==cmd1);
        uint8_t x=f[1]^f[2]^f[3];
        for(unsigned i=0;i<payload.size();++i){
            if(f[4+i]!=payload[i])std::cerr<<"Mismatch cmd="<<unsigned(cmd0)<<":"<<unsigned(cmd1)<<" offset="<<i<<" actual="<<unsigned(f[4+i])<<" expected="<<unsigned(payload[i])<<"\n";
            assert(f[4+i]==payload[i]);x^=f[4+i];}
        assert(f.back()==x);
    }
};
int main(){
    unsigned tests=0;
    {Fixture f;f.feed(0x21,1);f.expect(0,0x61,1,{0x59,0x80});++tests;}
    {Fixture f;uint8_t v[9];znp::version(v);f.feed(0x21,2);f.expect(0,0x61,2,std::vector<uint8_t>(v,v+9));++tests;}
    {Fixture f;f.feed(0x21,4);f.expect(0,0x61,4,{1,2,3,4,5,6,7,8});++tests;}
    {Fixture f;f.feed(0x27,0);f.expect(0,0x67,0,{0,1,2,3,4,5,6,7,8,0,0,1,9,0});++tests;}
    {Fixture f;f.feed(0x25,0x36,{2,0,0,60,1});f.expect(0,0x65,0x36,{0});assert(f.radio.last_permit==60);++tests;}
    {Fixture f;f.feed(0x25,0x36,{2,0,0,0,1});assert(f.radio.last_permit==0);++tests;}
    {Fixture f;f.feed(0x25,0x36,{0,0,0,60,1});f.expect(0,0x65,0x36,{2});++tests;}
    {Fixture f;f.feed(0x25,0x05,{0,0,0,0});f.expect(0,0x65,5,{0});f.expect(1,0x45,0x85,{0,0,0,0,0,1,1});++tests;}
    {Fixture f;f.feed(0x25,0x31,{0,0,0});f.expect(0,0x65,0x31,{0x12});++tests;}
    {Fixture f;f.feed(0x25,0x32,{0,0,0});f.expect(0,0x65,0x32,{0x12});++tests;}
    {Fixture f;f.feed(0x24,0x00,{1,4,1,0x50,0,0,0,0,0});f.expect(0,0x64,0,{0});++tests;}
    {Fixture f;f.feed(0x24,0x00,{2,4,1,0x50,0,0,0,0,0});f.expect(0,0x64,0,{0x12});++tests;}
    {Fixture f;f.feed(0x24,0x01,{0x34,0x12,1,1,0,5,0x8A,0,30,3,0x19,0,1});
        f.expect(0,0x64,1,{0});assert(f.radio.sends==1);++tests;}
    {Fixture f;f.feed(0x24,0x01,{0x34,0x12,1,1,0,5,0x8A,0,30,4,0x19,0,1});
        f.expect(0,0x60,0,{4,0x24,1});assert(f.radio.sends==0);++tests;}
    {Fixture f;f.feed(0x21,1,{},true);assert(f.frames.empty());assert(f.bridge.checksumErrors()==1);++tests;}
    {Fixture f;f.bridge.input(0xFE,0);f.bridge.poll(900);f.feed(0x21,1);
        f.expect(0,0x61,1,{0x59,0x80});++tests;}
    {Fixture f;f.feed(0x21,0x99);f.expect(0,0x60,0,{2,0x21,0x99});++tests;}
    {Fixture f;f.radio.pending_rx.src_addr=0x1234;f.radio.pending_rx.cluster_id=0x0500;
        f.radio.pending_rx.src_ep=1;f.radio.pending_rx.dst_ep=1;f.radio.pending_rx.lqi=180;
        f.radio.pending_rx.len=3;f.radio.pending_rx.data[0]=0x19;
        f.radio.rx_ready=true;f.bridge.poll(1234);
        auto &r=f.frames[0];assert(r[2]==0x44 && r[3]==0x81 && r[1]==20);
        assert(r[4+2]==0 && r[4+3]==5 && r[4+4]==0x34 && r[4+5]==0x12);++tests;}
    {Fixture f;f.radio.confirm={0,1,0x8A};f.radio.confirm_ready=true;
        f.bridge.poll(100);f.expect(0,0x44,0x80,{0,1,0x8A});++tests;}
    {Fixture f;f.radio.state.started=false;f.feed(0x25,0x36,{2,0,0,30,1});
        f.expect(0,0x65,0x36,{0x83});++tests;}
    std::cout<<"PASS: "<<tests<<" ZNP frame / dispatch checks (native mock radio)\n";
}
