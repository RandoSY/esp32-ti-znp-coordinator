#pragma once
#include <stdint.h>
#include <stddef.h>
#include "LqiDiagnostics.h"

// Narrow boundary between TI serial compatibility and the actual Zigbee stack.
// Implementations must never invent successful RF transmissions or neighbor tables.
struct RadioSnapshot {
    bool started;
    uint8_t ieee[8];        // TI on-wire IEEE address: little-endian octets
    uint16_t short_addr;    // coordinator = 0x0000
    uint16_t pan_id;
    uint8_t channel;
    uint8_t extended_pan[8];
    uint8_t network_key[16];
    uint32_t frame_counter;
    uint8_t key_sequence, update_id;
};
struct NetworkConfig {
    uint16_t pan_id=0xFFFF;
    uint32_t primary_channels=0x07FFF800, secondary_channels=0;
    uint8_t extended_pan[8]={}, network_key[16]={}, ieee[8]={};
    uint32_t frame_counter=0;
    bool erase=false;
};
struct EndpointDescriptor {
    uint8_t endpoint, version, latency, in_count, out_count;
    uint16_t profile, device;
    uint16_t in[16], out[16];
};
struct RadioEvent { uint8_t subsystem, command, len; uint8_t data[250]; };
struct ApsRx {
#if ZNP_LQI_DIAGNOSTICS
    uint32_t lqi_diagnostic_id;
#endif
    uint16_t src_addr, cluster_id;
    uint16_t group;
    uint8_t broadcast;
    uint8_t src_ep, dst_ep, lqi, security;
    uint32_t timestamp;
    uint8_t len;
    uint8_t data[220];
};
struct ApsConfirm {
    uint8_t status, endpoint, transaction_id;
};
class RadioBackend {
public:
    virtual ~RadioBackend() {}
    virtual RadioSnapshot snapshot() = 0;
    virtual uint8_t start(const NetworkConfig &,bool) {return 0x12;}
    virtual uint8_t reset(bool) {return 0x12;}
    virtual uint8_t endpoint(const EndpointDescriptor &) {return 0x12;}
    virtual bool descriptor(uint8_t,EndpointDescriptor &) {return false;}
    virtual uint8_t endpoints(uint8_t *list) {list[0]=1;return 1;}
    virtual uint8_t zdoRequest(uint8_t,const uint8_t *,uint8_t) {return 0x12;}
    virtual uint8_t group(uint8_t,uint16_t,bool) {return 0x12;}
    virtual uint8_t txPower(int8_t) {return 0x12;}
    virtual uint8_t installCode(const uint8_t *,const uint8_t *,uint8_t) {return 0x12;}
    virtual bool popEvent(RadioEvent &) {return false;}
    virtual void service() {}
    virtual uint32_t diagnostic() {return 0;}
    virtual uint8_t sendApsExt(uint8_t mode,const uint8_t *dst,uint8_t de,uint8_t se,uint16_t cl,
                              uint8_t id,uint8_t opt,uint8_t rad,const uint8_t *p,uint8_t n) {
        if(mode!=2 && mode!=15)return 0x12;
        return sendAps(uint16_t(dst[0])|(uint16_t(dst[1])<<8),de,se,cl,id,opt,rad,p,n);
    }
    // Returned status: 0=accepted, !=0=not accepted; a successful acceptance
    // is not a guarantee of a completed over-the-air transaction.
    virtual uint8_t permitJoin(uint8_t seconds) = 0;
    virtual uint8_t sendAps(uint16_t dst, uint8_t dst_ep, uint8_t src_ep,
                             uint16_t cluster, uint8_t transaction_id,
                             uint8_t opts, uint8_t radius,
                             const uint8_t *payload, uint8_t length) = 0;
    virtual bool popRx(ApsRx &rx) = 0;
    virtual bool popConfirm(ApsConfirm &cf) = 0;
};
