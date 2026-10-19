#pragma once
#include <stdint.h>

namespace znp {
// Existing saturation policy, isolated for regression testing. This is NOT an
// RSSI conversion or an assertion that the SDK documents negative sentinels.
constexpr bool lqiInByteRange(int raw){return raw>=0 && raw<=255;}
constexpr uint8_t lqiToByte(int raw){return uint8_t(raw<0?0:(raw>255?255:raw));}
}
