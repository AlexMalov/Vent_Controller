#ifndef _mqttRelay
#define _mqttRelay

#include "Arduino.h"

class MqttRelay
{
  private:
    uint8_t _isOn;            // состояние диммер - влючен или выключен
  public: 
    MqttRelay(uint8_t pin1, uint8_t idx);
    char* mqtt_topic_state;
    bool useMQTT = false;
    bool mqttRegistered = false;      // канал зарегисрирован на MQTT
    uint8_t index;           // номер кнопки
    uint8_t pin;              // pin Кнопки
    void setOn();
    void setOff();
    void toggle();
    void loadEEPROM();
    bool getOnOff();
};

#endif
