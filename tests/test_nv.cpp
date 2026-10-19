#include "ZnpBridge.h"
#include <cassert>
#include <cstring>
#include <iostream>
struct Radio:RadioBackend {
    RadioSnapshot s={};unsigned resets=0;
    Radio(){for(unsigned i=0;i<8;++i)s.ieee[i]=uint8_t(i+1);}
    RadioSnapshot snapshot()override{return s;}
    uint8_t reset(bool)override{++resets;return 0;}
    uint8_t permitJoin(uint8_t)override{return 0;}
    uint8_t sendAps(uint16_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override{return 0;}
    bool popRx(ApsRx &)override{return false;}bool popConfirm(ApsConfirm &)override{return false;}
};
struct FaultStore:MemoryNvStore {bool fail=false;bool put(NvKey k,const uint8_t *p,size_t n)override{return !fail && MemoryNvStore::put(k,p,n);}};
struct Fixture {
    FaultStore store;Radio radio;std::vector<std::vector<uint8_t>> frames;ZnpBridge bridge;
    static void output(const uint8_t *p,size_t n,void *ctx){static_cast<Fixture *>(ctx)->frames.emplace_back(p,p+n);}
    Fixture():bridge(radio,output,this,&store){assert(bridge.begin());}
    void feed(uint8_t cmd,std::initializer_list<uint8_t> p,uint8_t type=0x21){
        std::vector<uint8_t> b(p);uint8_t sum=uint8_t(b.size())^type^cmd;
        bridge.input(0xFE,1);bridge.input(uint8_t(b.size()),1);bridge.input(type,1);bridge.input(cmd,1);
        for(auto v:b){bridge.input(v,1);sum^=v;}bridge.input(sum,1);
    }
    std::vector<uint8_t> reply(){assert(!frames.empty());const auto &f=frames.back();return {f.begin()+4,f.end()-1};}
    void expect(std::initializer_list<uint8_t> p){assert(reply()==std::vector<uint8_t>(p));}
};
int main(){unsigned checks=0;
    {Fixture f;f.feed(0x13,{0x82,0});f.expect({24,0});++checks;}
    {Fixture f;f.feed(7,{0x34,0x12,8,0,2,0xAA,0xBB});f.expect({9});f.feed(0x1C,{0x34,0x12,0,0});f.expect({0,8,0xAA,0xBB,255,255,255,255,255,255});++checks;}
    {Fixture f;f.feed(7,{0x34,0x12,8,0,0});f.feed(0x1D,{0x34,0x12,3,0,3,0,0xFE,0x11,0x13});f.expect({0});f.feed(0x1C,{0x34,0x12,3,0});f.expect({0,5,0xFE,0x11,0x13,255,255});++checks;}
    {Fixture f;f.feed(7,{0x34,0x12,2,0,2,4,5});f.feed(0x1D,{0x34,0x12,1,0,2,0,7,8});f.expect({2});f.feed(8,{0x34,0x12,0});f.expect({0,2,4,5});++checks;}
    {Fixture f;f.feed(0x1D,{0x82,0,0,0,1,1,3});assert(f.frames.back()[2]==0x60);++checks;}
    {Fixture f;f.feed(0x1C,{0x82,0,24,0});f.expect({0,0});++checks;}
    {Fixture f;f.feed(0x30,{1,7,0,2,0,12,0,0,0});f.expect({0});f.feed(0x34,{1,7,0,2,0,0,0,3,4,5,6});f.expect({0});f.feed(0x33,{1,7,0,2,0,0,0,3});f.expect({0,3,4,5,6});f.feed(0x32,{1,7,0,3,0});f.expect({0});++checks;}
    {Fixture f;f.feed(7,{0x34,0x12,1,0x10,0});f.expect({0x0C});++checks;}
    {Fixture f;f.store.fail=true;f.feed(9,{3,0,0,1,7});f.expect({0x0A});assert(f.bridge.storageFault());f.feed(8,{3,0,0});f.expect({0,1,0});++checks;}
    {Fixture f;f.feed(7,{0x34,0x12,2,0,2,4,5});f.feed(0x12,{0x34,0x12,1,0});f.expect({0x0C});f.feed(0x12,{0x34,0x12,2,0});f.expect({0});f.feed(0x13,{0x34,0x12});f.expect({0,0});++checks;}
    {Fixture f;f.feed(9,{3,0,0,1,2});ZnpBridge other(f.radio,Fixture::output,&f,&f.store);assert(other.begin());std::vector<uint8_t> v;assert(f.store.get({0,3,0},v) && v[0]==2);++checks;}
    {Fixture f;f.feed(9,{3,0,0,1,3});f.feed(0,{1},0x41);assert(f.radio.resets==1);std::vector<uint8_t> v;assert(f.store.get({0xFE,1,0},v) && v[0]==1);assert(f.store.get({0,3,0},v) && v[0]==0);++checks;}
    std::cout<<"PASS: "<<checks<<" NV persistence, wire lengths, bounds, failure and reset checks\n";
}
