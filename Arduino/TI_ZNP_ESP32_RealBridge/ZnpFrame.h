#pragma once
#include <stddef.h>
#include <stdint.h>

// TI MT/UNPI frame: FE | len | cmd0 | cmd1 | payload[len] | XOR FCS.
// No allocation and no Arduino dependency, so this can be fuzz/host tested.
namespace znp {
static const uint8_t kSof = 0xFE;
static const size_t kMaxPayload = 250;
struct Frame {
    uint8_t cmd0, cmd1, len;
    uint8_t data[kMaxPayload];
};
class FrameSink {
public:
    virtual ~FrameSink() {}
    virtual void onFrame(const Frame &f) = 0;
};
class FrameDecoder {
public:
    explicit FrameDecoder(FrameSink &sink);
    void input(uint8_t byte, uint32_t ms);
    void poll(uint32_t ms);
    uint32_t good() const { return good_; }
    uint32_t badFcs() const { return bad_fcs_; }
    uint32_t truncated() const { return truncated_; }
private:
    enum State { WAIT_SOF, READ_LEN, CMD0, CMD1, DATA, FCS };
    FrameSink &sink_;
    Frame current_;
    State state_;
    uint8_t x_, n_;
    uint32_t last_ms_, good_, bad_fcs_, truncated_;
    void clear();
};
typedef void (*Writer)(const uint8_t *bytes, size_t count, void *ctx);
void write(Writer writer, void *ctx, uint8_t cmd0, uint8_t cmd1,
           const uint8_t *data, uint8_t len);
inline uint16_t get16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
inline void put16(uint8_t *p, uint16_t value) { p[0]=value; p[1]=value>>8; }
inline void put32(uint8_t *p, uint32_t value) {
    for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(value>>(i*8));
}
}
