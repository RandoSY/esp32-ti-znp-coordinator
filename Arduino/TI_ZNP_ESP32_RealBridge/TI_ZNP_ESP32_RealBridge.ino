/** CC2652-compatible TI ZNP serial personality, ESP32-C6/H2 native Zigbee.
 * Board: Zigbee ZCZR, ZCZR partition, debug None. Select CDCOnBoot to match
 * the USB connection: enabled for native USB, disabled for USB-UART/UART0.
 */
#include <Arduino.h>
#ifndef ZIGBEE_MODE_ZCZR
#error "Select Zigbee ZCZR (coordinator/router)"
#endif
#include "EspRadio.h"
#include "EspNvStore.h"
#include "ZnpBridge.h"
#include "StatusLed.h"
static EspRadio radio;
static EspNvStore store;
static void serialWriter(const uint8_t *p,size_t n,void *){Serial.write(p,n);}
static ZnpBridge bridge(radio,serialWriter,nullptr,&store);
static StatusLed led;
static bool healthy=false;
static uint32_t old_errors=0,old_fcs=0,old_dropped=0,old_received=0;
void setup(){
    Serial.begin(115200);
    healthy=store.begin() && radio.begin();
    if(healthy)healthy=bridge.begin(true);
}
void loop(){
    const uint32_t now=millis();
    if(healthy){
        unsigned budget=1024;
        while(budget-- && Serial.available()>0){int b=Serial.read();if(b>=0)bridge.input(uint8_t(b),now);}
        bridge.poll(now);
        if(radio.received()!=old_received){led.received(now);old_received=radio.received();}
        if(bridge.protocolErrors()!=old_errors || bridge.checksumErrors()!=old_fcs || radio.dropped()!=old_dropped)led.warning(now);
        old_errors=bridge.protocolErrors();old_fcs=bridge.checksumErrors();old_dropped=radio.dropped();
    }
    led.update(now,radio.state(),!healthy || bridge.storageFault());
    delay(1);
}
