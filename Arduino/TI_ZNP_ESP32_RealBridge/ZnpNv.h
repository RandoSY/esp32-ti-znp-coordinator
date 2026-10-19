#pragma once
#include "NvStore.h"
#include "RadioBackend.h"
class ZnpNv {
    NvStore &store_;
    bool seed(uint16_t id,const uint8_t *p,size_t n);
public:
    explicit ZnpNv(NvStore &s):store_(s){}
    bool begin(const RadioSnapshot &s);
    bool read(NvKey k,std::vector<uint8_t> &v){return store_.get(k,v);}
    bool save(NvKey k,const uint8_t *p,size_t n){return store_.put(k,p,n);}
    bool remove(NvKey k){return store_.erase(k);}
    bool clearNetwork();
    bool sync(const RadioSnapshot &s);
    NetworkConfig config();
};
