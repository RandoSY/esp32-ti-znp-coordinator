#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Caller holds its task lock. SDK confirms can contain a copied ASDU, so match
// bytes rather than pointer identity; distinct messages may share a destination.
class ApsTxTracker {
public:
    struct Entry {bool busy=false;uint8_t id=0,ep=0,mode=0,de=0,len=0;uint8_t dst[8]={},data[220]={};};
    Entry *reserve(uint8_t mode,const uint8_t *dst,uint8_t de,uint8_t se,uint8_t id,const uint8_t *data,size_t n){
        if(n>220 || (n && !data))return nullptr;
        const uint8_t normalized=mode==15?2:mode;
        for(const auto &t:entries_)if(address(t,normalized,dst,de,se) && t.len==n && (!n || !memcmp(t.data,data,n)))return nullptr;
        for(auto &t:entries_)if(!t.busy){t.busy=true;t.id=id;t.ep=se;t.mode=normalized;t.de=de;t.len=uint8_t(n);memcpy(t.dst,dst,8);if(n)memcpy(t.data,data,n);return &t;}
        return nullptr;
    }
    bool take(uint8_t mode,const uint8_t *dst,uint8_t de,uint8_t se,const uint8_t *data,size_t n,uint8_t &id){
        Entry *match=nullptr;unsigned candidates=0;
        for(auto &t:entries_)if(address(t,mode,dst,de,se)){
            if(data){if(t.len==n && (!n || !memcmp(t.data,data,n))){match=&t;break;}}
            else{match=&t;++candidates;}
        }
        // An error callback without an ASDU is usable only when unambiguous.
        if(!match || (!data && candidates!=1))return false;
        id=match->id;match->busy=false;return true;
    }
private:
    Entry entries_[16];
    static bool address(const Entry &t,uint8_t mode,const uint8_t *dst,uint8_t de,uint8_t se){
        return t.busy && t.mode==mode && t.ep==se && (mode==1 || t.de==de) && !memcmp(t.dst,dst,mode==3?8:2);
    }
};
