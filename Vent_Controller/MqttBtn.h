#ifndef _mqttBtn
#define _mqttBtn

#include "Arduino.h"
#include <GyverButton.h>

class MqttBtn: public GButton
{
  public: 
    MqttBtn(uint8_t pin1): GButton(pin1) {};
    char* mqtt_topic_cmd;
    bool useMQTT = false;
    bool mqttRegistered = false;      // канал зарегисрирован на MQTT
};

#endif
