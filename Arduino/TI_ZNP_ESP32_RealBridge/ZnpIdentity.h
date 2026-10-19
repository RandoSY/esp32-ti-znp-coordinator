#pragma once
#include <stdint.h>
namespace znp {
// Z-Stack 3.x.0 wire personality (CC2652 family). The implementation is ESP32.
constexpr uint32_t Revision=20240710;
inline void version(uint8_t *p){p[0]=2;p[1]=1;p[2]=2;p[3]=7;p[4]=1;for(unsigned i=0;i<4;++i)p[5+i]=uint8_t(Revision>>(8*i));}
inline void resetInd(uint8_t *p,uint8_t reason=1){p[0]=reason;p[1]=2;p[2]=1;p[3]=2;p[4]=7;p[5]=1;}
}
