#include "ZnpFrame.h"
#include <string.h>
namespace znp {
FrameDecoder::FrameDecoder(FrameSink &sink) : sink_(sink), state_(WAIT_SOF),
    x_(0), n_(0), last_ms_(0), good_(0), bad_fcs_(0), truncated_(0) {
    memset(&current_, 0, sizeof(current_));
}
void FrameDecoder::clear() { state_=WAIT_SOF; x_=n_=0; }
void FrameDecoder::poll(uint32_t ms) {
    // Prevent an incomplete packet from blocking the next packet indefinitely.
    if (state_!=WAIT_SOF && (uint32_t)(ms-last_ms_)>750) {
        ++truncated_; clear();
    }
}
void FrameDecoder::input(uint8_t b, uint32_t ms) {
    poll(ms);
    last_ms_=ms;
    switch(state_) {
    case WAIT_SOF: if(b==kSof) state_=READ_LEN; break;
    case READ_LEN:
        if(b>kMaxPayload){++truncated_;clear();break;}
        current_.len=b; x_=b; state_=CMD0; break;
    case CMD0: current_.cmd0=b; x_^=b; state_=CMD1; break;
    case CMD1: current_.cmd1=b; x_^=b; n_=0;
        state_=current_.len ? DATA : FCS; break;
    case DATA: current_.data[n_++]=b; x_^=b;
        if(n_==current_.len) state_=FCS;
        break;
    case FCS: {
        const bool valid=b==x_;
        if(valid) { ++good_; sink_.onFrame(current_); }
        else { ++bad_fcs_; }
        clear();
        // If a corrupt FCS was actually a fresh SOF, resynchronize immediately.
        if(!valid && b==kSof)state_=READ_LEN;
        break;
    }
    }
}
void write(Writer writer, void *ctx, uint8_t cmd0, uint8_t cmd1,
           const uint8_t *data, uint8_t len) {
    if(!writer || len>kMaxPayload || (len && !data)) return;
    uint8_t buf[kMaxPayload+5];
    buf[0]=kSof;buf[1]=len;buf[2]=cmd0;buf[3]=cmd1;
    uint8_t fcs=len^cmd0^cmd1;
    for(unsigned i=0;i<len;++i){buf[4+i]=data[i];fcs^=data[i];}
    buf[4+len]=fcs;
    writer(buf, size_t(len)+5, ctx);
}
}
