#pragma once
#include "ZnpFrame.h"
#include "RadioBackend.h"
#include "ZnpNv.h"
class ZnpBridge : private znp::FrameSink {
public:
    ZnpBridge(RadioBackend &,znp::Writer,void *,NvStore *store=nullptr);
    bool begin(bool notifyReset=false);
    void input(uint8_t,uint32_t);
    void poll(uint32_t);
    uint32_t frames()const{return decoder_.good();}
    uint32_t checksumErrors()const{return decoder_.badFcs();}
    uint32_t protocolErrors()const{return errors_;}
    bool storageFault()const{return storage_fault_;}
private:
    RadioBackend &radio_;znp::Writer tx_;void *tx_ctx_;
    znp::FrameDecoder decoder_;
    MemoryNvStore fallback_;ZnpNv nv_;
    uint32_t now_=0,errors_=0;
    uint8_t next_seq_=0;
    bool initialized_=false,storage_fault_=false,resetting_=false;
    void onFrame(const znp::Frame &)override;
    void reply(uint8_t,uint8_t,const uint8_t *,uint8_t);
    void status(uint8_t,uint8_t,uint8_t);
    void rpcError(const znp::Frame &,uint8_t);
    void sys(const znp::Frame &);
    void af(const znp::Frame &);
    void zdo(const znp::Frame &);
    void util(const znp::Frame &);
    void app(const znp::Frame &);
    bool nvCommand(const znp::Frame &);
    void emitIncoming(const ApsRx &);
};
