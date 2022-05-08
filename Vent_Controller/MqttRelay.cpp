#include "MqttRelay.h"
#include <EEPROM.h>
#define EEPROMdataVer 20

MqttRelay::MqttRelay(uint8_t pin1, uint8_t idx) {
  index = idx; 
  pin = pin1;
  pinMode(pin, OUTPUT);
  loadEEPROM();
}

void MqttRelay::loadEEPROM(){
  if (EEPROM[0] != EEPROMdataVer) return;  // в EEPROM ничего еще не сохраняли
  if (EEPROM[index*2+1]) _isOn = 0xff; else _isOn = 0;
}

bool MqttRelay::getOnOff(){
  return _isOn;
}

void MqttRelay::setOn(){
  _isOn = 0xff;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);
}

void MqttRelay::setOff(){
  _isOn = 0;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);
}

void MqttRelay::toggle(){
  _isOn = ~_isOn;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);  
}
