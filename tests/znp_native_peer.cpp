// Protocol integration peer. ONLY the backend is simulated; production ZNP code
// and disk persistence are exercised by unmodified zigbee-herdsman over TCP.
#include "ZnpBridge.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <deque>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
class DiskStore:public NvStore {
    std::filesystem::path dir_;
    std::filesystem::path path(NvKey k){char b[32];snprintf(b,sizeof(b),"%02x_%04x_%04x.bin",k.system,k.item,k.sub);return dir_/b;}
public:
    explicit DiskStore(const char *p):dir_(p){std::filesystem::create_directories(dir_);}
    bool get(NvKey k,std::vector<uint8_t> &v)override{
        std::ifstream f(path(k),std::ios::binary);v.assign(std::istreambuf_iterator<char>(f),{});return bool(f) && !v.empty();
    }
    bool put(NvKey k,const uint8_t *p,size_t n)override{
        if(!n || n>MaxRecord)return false;
        std::ofstream f(path(k),std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char *>(p),n);f.close();return !f.fail();
    }
    bool erase(NvKey k)override{return std::filesystem::remove(path(k));}
};
struct Peer:RadioBackend {
    RadioSnapshot s={};std::deque<RadioEvent> events;bool reboot=false;
    Peer(){for(unsigned i=0;i<8;++i)s.ieee[i]=uint8_t(i+1);s.pan_id=0xFFFF;s.short_addr=0xFFFE;}
    RadioSnapshot snapshot()override{return s;}
    uint8_t start(const NetworkConfig &c,bool)override{
        s.started=true;s.pan_id=c.pan_id;s.short_addr=0;memcpy(s.extended_pan,c.extended_pan,8);memcpy(s.network_key,c.network_key,16);memcpy(s.ieee,c.ieee,8);
        for(uint8_t ch=11;ch<=26;++ch)if(c.primary_channels&(1UL<<ch)){s.channel=ch;break;}
        s.frame_counter=c.frame_counter;RadioEvent e={};e.subsystem=5;e.command=0xC0;e.len=1;e.data[0]=9;events.push_back(e);return 0;
    }
    uint8_t reset(bool)override{reboot=true;s.started=false;return 0;}
    uint8_t endpoint(const EndpointDescriptor &)override{return 0;}
    uint8_t endpoints(uint8_t *p)override{const uint8_t list[]={1,2,3,4,5,6,8,10,11,110,12,13,47,239,242};memcpy(p,list,sizeof(list));return sizeof(list);}
    uint8_t group(uint8_t,uint16_t,bool add)override{return add?0:1;}
    uint8_t permitJoin(uint8_t)override{return 0;}
    uint8_t txPower(int8_t)override{return 0;}
    uint8_t sendAps(uint16_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override{return 0x12;}
    bool popRx(ApsRx &)override{return false;}
    bool popConfirm(ApsConfirm &)override{return false;}
    bool popEvent(RadioEvent &e)override{if(events.empty())return false;e=events.front();events.pop_front();return true;}
};
static void output(const uint8_t *p,size_t n,void *){fwrite(p,1,n,stdout);fflush(stdout);}
int main(int argc,char **argv){
    if(argc!=2)return 2;
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    DiskStore store(argv[1]);Peer radio;
    auto bridge=std::make_unique<ZnpBridge>(radio,output,nullptr,&store);if(!bridge->begin())return 3;
    uint32_t now=0;int b;
    while((b=getchar())!=EOF){bridge->input(uint8_t(b),++now);bridge->poll(now);
        if(radio.reboot){radio.reboot=false;bridge=std::make_unique<ZnpBridge>(radio,output,nullptr,&store);if(!bridge->begin(true))return 3;}}
    return 0;
}
