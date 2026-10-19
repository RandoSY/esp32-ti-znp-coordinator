#pragma once
#include "NvStore.h"
#include <nvs.h>
#include <stdio.h>
class EspNvStore : public NvStore {
    nvs_handle_t handle_=0;
    bool ready_=false;
    static void name(NvKey k,char (&out)[16]) {snprintf(out,sizeof(out),"n%02x%04x%04x",k.system,k.item,k.sub);}
public:
    bool begin(){ready_=nvs_open("znp",NVS_READWRITE,&handle_)==ESP_OK;return ready_;}
    bool get(NvKey key,std::vector<uint8_t> &value) override {
        value.clear();if(!ready_)return false;
        char n[16];name(key,n);size_t size=0;
        if(nvs_get_blob(handle_,n,nullptr,&size)!=ESP_OK || !size || size>MaxRecord)return false;
        value.resize(size);return nvs_get_blob(handle_,n,value.data(),&size)==ESP_OK;
    }
    bool put(NvKey key,const uint8_t *value,size_t size) override {
        if(!ready_ || !size || size>MaxRecord)return false;
        char n[16];name(key,n);
        return nvs_set_blob(handle_,n,value,size)==ESP_OK && nvs_commit(handle_)==ESP_OK;
    }
    bool erase(NvKey key) override {
        if(!ready_)return false;
        char n[16];name(key,n);
        return nvs_erase_key(handle_,n)==ESP_OK && nvs_commit(handle_)==ESP_OK;
    }
};
