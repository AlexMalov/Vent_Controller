#ifndef fan2x
#define fan2x
#define EEPROMdataVer 23

#include "Arduino.h"
#include "dimmerM.h"

#define acFreq 50                      // частота питающей сети в Герцах
#define zeroPeriod 1000/(2*acFreq)     // период перехода через ноль (для 50 Гц - 10 мс)

class Fan2x: public DimmerM
{
  private:
    uint8_t _dimmerState;        // состояние диммера для корректного выкючения вентилятора
    uint8_t _fanPower;           // текущая мощность вентилятора от 0 до 100%
  public:
    Fan2x(uint8_t dimmerPin, uint8_t relayPin, uint8_t idx);
    //char* mqtt_topic_state;
    //char* mqtt_topic_bri_state;
    //bool useMQTT = false;
    //bool mqttRegistered = false;      // канал зарегисрирован на MQTT
    //uint8_t index;           // номер диммера
    //uint8_t pin;              // pin Диммера
    uint8_t relayPin;           // pin реле второй скорости
    //uint8_t triacTime;        // мощность переведенная во время закрытия семистора
    void setOn();
    void setOff();
    //void toggle();
    void setPower(uint8_t power = 211);      // нужно вызывать периодически
    //void incPower();
    //void setRampTime(uint16_t rampTime); // ramtTime в секундах
    //void zeroCross();
    void loadEEPROM();
    void saveEEPROM();
    //uint8_t getRampTime();
    uint8_t getPower();
    //bool getOnOff();
};


#endif
