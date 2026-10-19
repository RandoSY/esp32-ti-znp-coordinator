#pragma once
#include <Arduino.h>
#include "RadioBackend.h"
#include "ApsTxTracker.h"
#include "esp_zigbee_core.h"
#include "aps/esp_zigbee_aps.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
class EspRadio : public RadioBackend {
public:
    enum class State:uint8_t {Boot,Waiting,Commissioning,Online,Joining,Error};
    bool begin();
    RadioSnapshot snapshot()override;
    uint8_t start(const NetworkConfig &,bool)override;
    uint8_t reset(bool)override;
    uint8_t endpoint(const EndpointDescriptor &)override;
    bool descriptor(uint8_t,EndpointDescriptor &)override;
    uint8_t endpoints(uint8_t *)override;
    uint8_t group(uint8_t,uint16_t,bool)override;
    uint8_t txPower(int8_t)override;
    uint8_t installCode(const uint8_t *,const uint8_t *,uint8_t)override;
    uint8_t permitJoin(uint8_t)override;
    uint8_t zdoRequest(uint8_t,const uint8_t *,uint8_t)override;
    uint8_t sendAps(uint16_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override;
    uint8_t sendApsExt(uint8_t,const uint8_t *,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t,const uint8_t *,uint8_t)override;
    bool popRx(ApsRx &r)override{
        if(!rx_queue_ || xQueueReceive(rx_queue_,&r,0)!=pdTRUE)return false;
        ++received_;return true;
    }
    bool popConfirm(ApsConfirm &c)override{return confirmation_queue_ && xQueueReceive(confirmation_queue_,&c,0)==pdTRUE;}
    bool popEvent(RadioEvent &e)override{return events_ && xQueueReceive(events_,&e,0)==pdTRUE;}
    void service()override;
    State state()const{return state_;}
    uint32_t dropped()const{return dropped_;}
    uint32_t received()const{return received_;}
    uint32_t diagnostic()override{return error_code_;}
    void signal(esp_zb_app_signal_t *);
    struct Context {EspRadio *self;uint16_t dst;uint8_t command;};
    void event(uint8_t command,const uint8_t *,size_t);
    static EspRadio *instance;
private:
    QueueHandle_t rx_queue_=nullptr,confirmation_queue_=nullptr,events_=nullptr;
    SemaphoreHandle_t group_done_=nullptr;
    volatile State state_=State::Boot;
    volatile uint32_t dropped_=0;
    // Read and advanced only by the Arduino loop as it drains received data.
    uint32_t received_=0;
    uint32_t reboot_at_=0,join_until_=0;
    bool task_created_=false,initialized_=false,formation_=false;
    volatile uint32_t error_code_=0;
    NetworkConfig config_;
    RadioSnapshot cached_={};
    portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;
    ApsTxTracker pending_;
    static void task(void *);
    static bool indication(esp_zb_apsde_data_ind_t);
    static void confirmation(esp_zb_apsde_data_confirm_t);
    bool registerEndpoints();
};
