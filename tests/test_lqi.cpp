// Actual production AF encoder with a simulated radio; no serial hardware.
#include "ZnpBridge.h"
#include "LqiMetadata.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <vector>

struct Radio:RadioBackend {
    ApsRx rx={};bool ready=false;
    RadioSnapshot snapshot()override{return {};}
    uint8_t permitJoin(uint8_t)override{return 0x12;}
    uint8_t sendAps(uint16_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override{return 0x12;}
    bool popRx(ApsRx &out)override{if(!ready)return false;out=rx;ready=false;return true;}
    bool popConfirm(ApsConfirm &)override{return false;}
};
struct Fixture {
    Radio radio;std::vector<std::vector<uint8_t>> frames;ZnpBridge bridge;
    static void output(const uint8_t *p,size_t n,void *ctx){static_cast<Fixture *>(ctx)->frames.emplace_back(p,p+n);}
    Fixture():bridge(radio,output,this){assert(bridge.begin(false));}
};
int main(int argc,char **argv){
    FILE *out=argc>1?fopen(argv[1],"w"):nullptr;if(argc>1)assert(out);
    const int samples[]={0,1,15,30,142,173,254,255,-1,-128,-129,256,32767,INT_MIN,INT_MAX};
    unsigned cases=0;
    for(int raw:samples){
        const unsigned expected=raw<0?0U:(raw>255?255U:unsigned(raw));
        assert(znp::lqiToByte(raw)==expected);
        assert(znp::lqiInByteRange(raw)==(raw>=0 && raw<=255));
        for(unsigned length:{0U,3U,220U}){
            Fixture f;auto &r=f.radio.rx;
            r.group=0x1234;r.cluster_id=0x0500;r.src_addr=0xBEEF;r.src_ep=3;r.dst_ep=11;
            r.broadcast=1;r.lqi=znp::lqiToByte(raw);r.security=1;r.timestamp=0x78563412;r.len=uint8_t(length);
            for(unsigned i=0;i<length;++i)r.data[i]=uint8_t(i*37U+9U);
            f.radio.ready=true;f.bridge.poll(123);
            assert(f.frames.size()==1);const auto &b=f.frames[0];
            assert(b.size()==22+length && b[0]==0xFE && b[1]==17+length && b[2]==0x44 && b[3]==0x81);
            // TI layout: 2+2+2+1+1+1 bytes precede LinkQuality. Values chosen
            // to detect shifts, endian mistakes, and accidental field overlap.
            const uint8_t prefix[]={0x34,0x12,0x00,0x05,0xEF,0xBE,3,11,1};
            for(unsigned i=0;i<sizeof(prefix);++i)assert(b[4+i]==prefix[i]);
            assert(b[13]==expected && b[14]==1);
            const uint8_t tail[]={0x12,0x34,0x56,0x78,1};
            for(unsigned i=0;i<sizeof(tail);++i)assert(b[15+i]==tail[i]);
            assert(b[20]==length);
            for(unsigned i=0;i<length;++i)assert(b[21+i]==uint8_t(i*37U+9U));
            unsigned checksum=0;for(size_t i=1;i<b.size();++i)checksum^=b[i];assert(checksum==0);
            if(out){fprintf(out,"{\"raw\":%d,\"expected\":%u,\"length\":%u,\"hex\":\"",raw,expected,length);for(auto v:b)fprintf(out,"%02x",unsigned(v));fprintf(out,"\"}\n");}
            ++cases;
        }
    }
    // Invalid receive length must not overread the fixed payload buffer.
    {Fixture f;f.radio.rx.len=221;f.radio.ready=true;f.bridge.poll(0);assert(f.frames.empty());}
    // Queued metadata/payload must remain independent of the next packet.
    {Fixture f;f.radio.rx.lqi=173;f.radio.ready=true;f.bridge.poll(0);f.radio.rx.lqi=0;f.radio.ready=true;f.bridge.poll(1);assert(f.frames.size()==2 && f.frames[0][13]==173 && f.frames[1][13]==0 && f.frames[0][19]==1 && f.frames[1][19]==2);}
#if !ZNP_LQI_DIAGNOSTICS
    {Fixture f;uint8_t query[]={0xFE,1,0x27,0xF1,0,0xD7};for(auto b:query)f.bridge.input(b,0);assert(f.frames.size()==1 && f.frames[0][2]==0x60 && f.frames[0][3]==0 && f.frames[0][4]==2);}
#endif
    if(out)fclose(out);
    printf("PASS: %u LQI boundary/length cases, adjacent fields, FCS, payload preservation and sequence isolation\n",cases);
}
