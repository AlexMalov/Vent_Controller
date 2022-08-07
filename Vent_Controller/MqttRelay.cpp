#include "MqttRelay.h"
#include <EEPROM.h>
#define EEPROMdataVer 23

MqttRelay::MqttRelay(uint8_t pin1, uint8_t idx) {
  index = idx; 
  pin = pin1;
  pinMode(pin, OUTPUT);
}

void MqttRelay::loadEEPROM(){
  if (EEPROM[baseRomAddr] != EEPROMdataVer) return;  // в EEPROM ничего еще не сохраняли
  //_isOn = EEPROM[baseRomAddr + index*2+1];
  //if (_isOn) setOn(); else setOff();
}

void MqttRelay::saveEEPROM(){
  //if ( !useMQTT ) EEPROM.update(baseRomAddr + index*2 + 1, _isOn);
  EEPROM.update(baseRomAddr, EEPROMdataVer);
}

bool MqttRelay::getOnOff(){
  return _isOn;
}

void MqttRelay::setOn(){
  static uint32_t endTm2 = 0;
  if (millis() < endTm2) return;              // ждем завершения переключения реле 50 mc
  if (!digitalRead(pin)) {
    digitalWrite(pin, true);    // переключаем реле
    endTm2 = millis() + 50;
    _isOn = 0xff;
  } 
  //saveEEPROM();
}

void MqttRelay::setOff(){
  static uint32_t endTm2 = 0;
  if (millis() < endTm2) return;              // ждем завершения переключения реле 50 mc
  if (digitalRead(pin)) {
    digitalWrite(pin, false);    // переключаем реле
    endTm2 = millis() + 50;
    _isOn = 0;
  } 
  //saveEEPROM();
}

void MqttRelay::toggle(){
  if (_isOn) setOff(); else setOn();
  //saveEEPROM();  
}
