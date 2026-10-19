// Separate compatibility issue found during LQI diagnostics. Manual reproducer;
// the original decoder fails; the narrow corrected decoder accepts both frames.
#include "ZnpFrame.h"
#include <cstdio>
struct Sink: znp::FrameSink {
    unsigned frames=0;
    void onFrame(const znp::Frame &) override {++frames;}
};
int main(){
    // Read diagnostic records 44 then 45. Each independently valid XOR frame.
    const unsigned char first[]={0xFE,5,0x27,0xF1,1,44,0,0,0,0xFE};
    const unsigned char second[]={0xFE,5,0x27,0xF1,1,45,0,0,0,0xFF};
    Sink sink; znp::FrameDecoder decoder(sink);
    for(auto b:first)decoder.input(b,1);
    for(auto b:second)decoder.input(b,2);
    std::printf("Valid consecutive frames: expected=2 actual=%u; boundary=%s\n",sink.frames,sink.frames==2?"PASS":"FAIL");
    return sink.frames==2?0:1;
}
