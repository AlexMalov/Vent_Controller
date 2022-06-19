#ifndef _triacPLC
  #define _triacPLC

//#define DEBUGPLC
// Значения по умолчанию
#define def_staticIP 192, 168, 0, 211                   // Утановить IP адресс для этой Arduino (должен быть уникальным в вашей сети)
#define def_staticDNS 192, 168, 0, 1
#define def_staticGW 192, 168, 0, 1
#define def_staticMASK 255, 255, 255, 0
#define def_macAddr { 0x90, 0xB2, 0xFA, 0x0D, 0x4E, 0x59 }  // Установить MAC адресс для этой Arduino (должен быть уникальным в вашей сети)
#define def_useDHCP false

#define def_mqtt_user "mqtt"                            // ИмяКлиента, Логин и Пароль для подключения к MQTT брокеру
#define def_mqtt_pass "mqtt"
#define def_mqtt_clientID "plcVent"
#define def_HomeAssistantDiscovery "homeassistant"
#define def_HABirthTopic "homeassistant/status"
#define def_mqtt_brokerIP 192,168,0,124   // IP адресс MQTT брокера по умочанию

#include "Arduino.h"
#include "dimmerM.h"
#include "MqttBtn.h"
#include "MqttRelay.h"
#include "Fan2x.h"

#include <Ethernet.h>
#include <PubSubClient.h>
#include <microDS18B20.h>

#define zeroFreq100 2*acFreq*100         // частота прерывания для таймера (для 50 Hz - 10 000 Hz)
#define zeroDetectorPin PA0             // пин детектора нуля
#define channelAmount 2                 // количество диммеров
#define btnsAmount 2                    // количество входов PLC
#define fansAmount 2                    // количество вентиляторов
#define relaysAmount 9                  // количество реле
#define DS_SENS_AMOUNT 5                // кол-во датчиков температуры
#define DS_rd_PIN PB13 // пин чтения для термометров
#define DS_wrt_PIN PB12 // пин записи для термометров

// Уникальные адреса датчиков - считать можно в примере address_read

struct inet_cfg_t {
  bool use_dhcp;
  uint8_t dhcp_refresh_minutes;
  uint8_t mac[6];
  uint8_t ip[4];
  uint8_t gw[4];
  uint8_t mask[4];
  uint8_t dns[4];
  uint16_t webSrvPort;
};

struct mqtt_cfg_t {
  bool useMQTT;
  char User[21];
  char Pass[9];
  char ClientID[21];
  char HADiscover[14];    // "HomeAssistant"
  uint8_t SrvIP[4];        
  uint16_t SrvPort;       //1883
  char HABirthTopic[21];  // "homeassistant/status"
  bool isHAonline = false;       // curent HA online status 
};

class triacPLC
{
  private:
    DimmerM _dimmers[channelAmount] = {DimmerM(PB11, 1), DimmerM(PB10, 2)};
    Fan2x _fans[fansAmount] = {Fan2x(PB1, PA7, 1), Fan2x(PB0, PA6, 2)};
    MqttBtn _btns[btnsAmount] = {MqttBtn(PB14), MqttBtn(PB15)};  // кнопки входов PLC
    MqttRelay _relays[relaysAmount] = {MqttRelay(PA5, 1), MqttRelay(PA4, 2), MqttRelay(PA3, 3), MqttRelay(PA2, 4), MqttRelay(PA1, 5), MqttRelay(PC15, 6), MqttRelay(PC14, 7), MqttRelay(PC13, 8), MqttRelay(PB9, 9)};  // relay выходов PLC
    volatile uint8_t _counter;                                                     // счётчик цикла диммирования
    volatile uint8_t _zeroCrossCntr = 10; 
    volatile uint32_t _zeroCrossTime;    
    bool _doEthernet();
    bool _doMqtt();
    bool _mqttReconnect();
    bool _haDiscover();                                 // регистрация канала за каналом
    void _doButtonsDimmers();
    void _doDSsensors();
  public:
    triacPLC();
    inet_cfg_t inet_cfg;
    mqtt_cfg_t mqtt_cfg;
    volatile int16_t ACfreq = 0;
    uint8_t DSaddrs[DS_SENS_AMOUNT][8]/* = {
      {0x28, 0xFF, 0x78, 0x5B, 0x50, 0x17, 0x4, 0xCF},      // перед калорифером - улица
      {0x28, 0xFF, 0x99, 0x80, 0x50, 0x17, 0x4, 0x4D},      // после калорифера
      {0x28, 0xFF, 0x53, 0xE5, 0x50, 0x17, 0x4, 0xC3},      // после рекуператора приток
      {0x28, 0xFF, 0x42, 0x5A, 0x51, 0x17, 0x4, 0xD2},      // после рекуператора сброс на улицу
      {0x28, 0x71, 0xCF, 0x48, 0xF6, 0xD9, 0x3C, 0xAB},      // подача гликоля в калорифер
    }*/;
    MicroDS18B20<DS_rd_PIN, DS_wrt_PIN, nullptr, DS_SENS_AMOUNT> _DSsensors;          // Создаем термометр с адресацией
    bool flipDisplay = false;                                                 // переворачивать дисплей
    bool needDelayedSaveRom = false;                                          // нужно отложенное сохранение eeprom
    void rtDisplay();                                                         // переворачивает дисплей
    bool begin(MQTT_CALLBACK_SIGNATURE = NULL);                               //инициализация дисплея и сетевой карты
    void mqttCallback(char* topic, byte* payload, uint16_t length);
    bool processEvents();
    void setDefInetCfg();
    void setDefMqttCfg();
    void display();
    void showFreq();
    void zeroCrossISR();
    void timerISR();
    void allOff();
    void setOn(uint8_t channel);
    void setOff(uint8_t channel);
    void toggle(uint8_t channel);
    void setPower(uint8_t channel, uint8_t power);
    void incPower(uint8_t channel);
    void setRampTime(uint8_t channel, uint16_t rampTime); // ramtTime в секундах
    void setFanOn(uint8_t channel);
    void setFanOff(uint8_t channel);
    void toggleFan(uint8_t channel);
    void setRelayOn(uint8_t channel);
    void setRelayOff(uint8_t channel);
    void toggleRelay(uint8_t channel);
    void setFanPower(uint8_t channel, uint8_t power);
    void incFanPower(uint8_t channel);
    void setFanRampTime(uint8_t channel, uint16_t rampTime); // ramtTime в секундах
    void loadEEPROM();
    void saveEEPROM();
    uint8_t getRampTime(uint8_t channel);
    uint8_t getPower(uint8_t channel);
    uint8_t getFanRampTime(uint8_t channel);
    uint8_t getFanPower(uint8_t channel);
    bool getBtnState(uint8_t btn_idx);
    bool getOnOff(uint8_t channel);
    bool getFanOnOff(uint8_t channel);
    bool getRelayOnOff(uint8_t channel);
    void set_useMQTT(bool new_useMQTT);
};

#endif
