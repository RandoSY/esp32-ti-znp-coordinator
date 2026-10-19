#pragma once
#include <Arduino.h>
// WeAct ESP32-C6-MINI schematic: WS2812 DIN on IO8.
#ifndef ZNP_RGB_PIN
#if CONFIG_IDF_TARGET_ESP32C6
#define ZNP_RGB_PIN 8
#else
#define ZNP_RGB_PIN -1
#endif
#endif
class StatusLed {
    uint32_t warning_until_=0,activity_until_=0,last_=0;
    uint8_t last_r_=255,last_g_=255,last_b_=255;
public:
    void warning(uint32_t now){warning_until_=now+1200;}
    void received(uint32_t now){activity_until_=now+120;}
    void update(uint32_t now,EspRadio::State state,bool storageError){
        if(ZNP_RGB_PIN<0)return;
        uint8_t r=0,g=0,b=0;
        if(storageError || state==EspRadio::State::Error){if((now/250)%2==0)r=24;}
        else if(warning_until_ && int32_t(warning_until_-now)>0){r=24;g=10;}
        else if(state==EspRadio::State::Commissioning){if((now/400)%2==0){r=14;b=20;}}
        else if((state==EspRadio::State::Online || state==EspRadio::State::Joining) && activity_until_ && int32_t(activity_until_-now)>0){r=20;g=20;b=20;}
        else if(state==EspRadio::State::Joining){g=16;b=16;}
        else if(state==EspRadio::State::Online){g=10;}
        else {b=((now/1000)%2)?4:16;}
        if(now-last_<25 || (r==last_r_ && g==last_g_ && b==last_b_))return;
        rgbLedWrite(ZNP_RGB_PIN,r,g,b);last_=now;last_r_=r;last_g_=g;last_b_=b;
    }
};
