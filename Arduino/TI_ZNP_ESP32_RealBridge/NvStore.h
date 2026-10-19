#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// A record replacement is atomic. Protocol offsets are handled above this layer.
// OSAL records use system 0; NV_EXT records retain their full three-part key.
struct NvKey {
    uint8_t system; uint16_t item, sub;
    bool operator==(const NvKey &b) const {return system==b.system && item==b.item && sub==b.sub;}
};
class NvStore {
public:
    static constexpr size_t MaxRecord=4096;
    virtual ~NvStore() {}
    virtual bool get(NvKey key, std::vector<uint8_t> &value)=0;
    virtual bool put(NvKey key, const uint8_t *value, size_t size)=0;
    virtual bool erase(NvKey key)=0;
};
class MemoryNvStore : public NvStore {
    struct Record { NvKey key; std::vector<uint8_t> value; };
    std::vector<Record> records_;
public:
    bool get(NvKey key, std::vector<uint8_t> &value) override {
        for(const auto &r:records_) if(r.key==key){value=r.value;return true;}
        value.clear();return false;
    }
    bool put(NvKey key,const uint8_t *value,size_t size) override {
        if(!size || size>MaxRecord)return false;
        for(auto &r:records_) if(r.key==key){r.value.assign(value,value+size);return true;}
        if(records_.size()>=256)return false;
        records_.push_back({key,std::vector<uint8_t>(value,value+size)});return true;
    }
    bool erase(NvKey key) override {
        for(auto p=records_.begin();p!=records_.end();++p)if(p->key==key){records_.erase(p);return true;}
        return false;
    }
};
