#include "ApsTxTracker.h"
#include <assert.h>
#include <stdio.h>
int main(){
    ApsTxTracker tracker;uint8_t dst[8]={0xFD,0xFF},first[]={25,0,2,10,0,0},second[]={25,1,2,10,0,0},id=0;
    assert(tracker.reserve(2,dst,242,242,1,first,sizeof(first)));
    assert(tracker.reserve(2,dst,242,242,2,second,sizeof(second)));
    // A copied ASDU must still select the correct transaction, out of order.
    uint8_t copy[6];memcpy(copy,second,sizeof(copy));
    assert(!tracker.take(2,dst,242,242,nullptr,0,id));
    assert(tracker.take(2,dst,242,242,copy,sizeof(copy),id) && id==2);
    assert(tracker.take(2,dst,242,242,first,sizeof(first),id) && id==1);
    assert(!tracker.take(2,dst,242,242,first,sizeof(first),id));
    assert(tracker.reserve(15,dst,242,242,3,first,sizeof(first)));
    assert(!tracker.reserve(2,dst,242,242,4,first,sizeof(first)));
    assert(tracker.take(2,dst,242,242,nullptr,0,id) && id==3);
    for(unsigned i=0;i<16;++i){first[1]=uint8_t(i);assert(tracker.reserve(2,dst,242,242,uint8_t(i),first,sizeof(first)));}
    first[1]=16;assert(!tracker.reserve(2,dst,242,242,16,first,sizeof(first)));
    puts("PASS: APS burst, copied/out-of-order confirmation, ambiguity and capacity regressions");
}
