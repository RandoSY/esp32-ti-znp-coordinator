#include "ZnpFrame.h"
#include <cassert>
#include <cstdio>
#include <vector>
struct Sink:znp::FrameSink {
    std::vector<znp::Frame> frames;
    void onFrame(const znp::Frame &f)override{frames.push_back(f);}
};
void feed(znp::FrameDecoder &d,const std::vector<uint8_t> &v,uint32_t ms=1){for(auto b:v)d.input(b,ms);}
int main(){
    // Every possible valid checksum, immediately followed by another frame.
    // The one-byte payload is chosen independently to produce the target XOR.
    for(unsigned fcs=0;fcs<256;++fcs){
        Sink sink;znp::FrameDecoder d(sink);
        const uint8_t payload=uint8_t(fcs^1^0x21^0x99);
        feed(d,{0xFE,1,0x21,0x99,payload,uint8_t(fcs)});
        feed(d,{0xFE,0,0x21,1,0x20},2);
        if(sink.frames.size()!=2){std::fprintf(stderr,"FAIL valid FCS 0x%02X: expected two frames, got %zu\n",fcs,sink.frames.size());return 1;}
        assert(sink.frames[0].data[0]==payload && sink.frames[1].cmd1==1);
        assert(d.badFcs()==0 && d.truncated()==0);
    }
    // Preserve existing recovery: an invalid FCS FE may be the next frame SOF.
    {Sink s;znp::FrameDecoder d(s);feed(d,{0xFE,0,0x21,1,0xFE,0,0x21,1,0x20});
     assert(s.frames.size()==1 && d.badFcs()==1 && d.good()==1);}
    {Sink s;znp::FrameDecoder d(s);feed(d,{0xFE,0,0x21,1,0x21,0xFE,0,0x21,1,0x20});
     assert(s.frames.size()==1 && d.badFcs()==1);}
    {Sink s;znp::FrameDecoder d(s);feed(d,{0xFE,2,0x21},1);d.poll(752);
     feed(d,{0xFE,0,0x21,1,0x20},753);assert(s.frames.size()==1 && d.truncated()==1);}
    // Maximum payload and embedded FE bytes are data, not delimiters.
    {Sink s;znp::FrameDecoder d(s);std::vector<uint8_t> v={0xFE,250,0x21,0x99};uint8_t x=250^0x21^0x99;
     for(unsigned i=0;i<250;++i){v.push_back(0xFE);x^=0xFE;}v.push_back(x);feed(d,v);
     feed(d,{0xFE,0,0x21,1,0x20},2);assert(s.frames.size()==2 && s.frames[0].len==250);}
    std::puts("PASS: 256 valid-FCS boundaries, corrupt-FE/non-FE recovery, timeout recovery and maximum embedded-FE payload");
}
