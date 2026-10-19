#include "LqiDiagnostics.h"
#if ZNP_LQI_DIAGNOSTICS
#include "ZnpFrame.h"
#if defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#else
#include <mutex>
#endif
namespace znp { namespace lqi_diag {
namespace {
Ring ring;
#if defined(ARDUINO)
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard {Guard(){portENTER_CRITICAL(&mutex);}~Guard(){portEXIT_CRITICAL(&mutex);}};
#else
std::mutex mutex;
using Guard=std::lock_guard<std::mutex>;
#endif
}
uint32_t capture(const Record &r){
#if defined(ARDUINO)
    Guard guard;
#else
    Guard guard(mutex);
#endif
    return ring.capture(r);
}
void queued(uint32_t id,bool ok){
#if defined(ARDUINO)
    Guard guard;
#else
    Guard guard(mutex);
#endif
    ring.queued(id,ok);
}
void encoded(uint32_t id,uint8_t value,uint8_t sequence){
#if defined(ARDUINO)
    Guard guard;
#else
    Guard guard(mutex);
#endif
    ring.encoded(id,value,sequence);
}
uint8_t read(const uint8_t *p,uint8_t n,uint8_t *out){
#if defined(ARDUINO)
    Guard guard;
#else
    Guard guard(mutex);
#endif
    out[0]=2;
    if(n==1 && p[0]==0){
        out[0]=0;out[1]=Schema;out[2]=Capacity;out[3]=ring.count();
        put32(out+4,ring.lastId());put32(out+8,ring.overwritten());put32(out+12,ring.queueDrops());return 16;
    }
    if(n!=5 || p[0]!=1)return 1;
    const uint32_t id=uint32_t(p[1])|(uint32_t(p[2])<<8)|(uint32_t(p[3])<<16)|(uint32_t(p[4])<<24);
    Record r;if(!ring.get(id,r)){out[0]=9;return 1;}
    out[0]=0;out[1]=Schema;put32(out+2,r.id);put32(out+6,r.rx_ms);put32(out+10,uint32_t(r.sdk_lqi_raw));
    put16(out+14,r.source);put16(out+16,r.cluster);put16(out+18,r.profile);put16(out+20,r.payload_length);
    out[22]=r.src_ep;out[23]=r.dst_ep;out[24]=r.sdk_status;out[25]=r.bridge_lqi;
    out[26]=r.znp_lqi;out[27]=r.znp_sequence;out[28]=r.flags;out[29]=0;return 30;
}
}}
#endif
