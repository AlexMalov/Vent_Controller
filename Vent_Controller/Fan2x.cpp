#include "Fan2x.h"

Fan2x::Fan2x(uint8_t dimmerPin, uint8_t relayPin1, uint8_t idx): DimmerM(dimmerPin, idx) {
  relayPin = relayPin1;
  pinMode(relayPin, OUTPUT);
  setRampTime(1);
}

void Fan2x::setPower(uint8_t power){
  static bool dimmerState, newRelayState;
  
  if (power != 211) _fanPower = constrain(power, 0, 100);
  static uint32_t endTm1 = 0, endTm2 = 0;
  if (millis() < endTm1) return;                  // при каждом переключении реле выдерживаем паузу 100 мс
  if (dimmerState != _isOn) {                     // после преключения релюхи возвращаем состояние семистора
      if (newRelayState != digitalRead(relayPin)) {
        digitalWrite(relayPin, newRelayState);    // переключаем реле
        endTm2 = millis() + 50;
      }
      if (millis() < endTm2) return;              // ждем завершения переключения реле 50 mc
      setOn();
      _rampCounter = _rampCycles;
  }
  if (_fanPower > 50){
    if (!digitalRead(relayPin)) {                 // при каждом переключении реле отключаем семистор и выдерживаем паузу 100 мс 
      endTm1 = millis() + 100;
      dimmerState = _isOn;
      setOff();
      _rampCounter = _rampCycles;
      newRelayState = true;
      return;                       
    }
    digitalWrite(relayPin, true);
    DimmerM::setPower(_fanPower); 
  } else {
    if (digitalRead(relayPin)) {
      endTm1 = millis() + 100;
      dimmerState = _isOn;
      setOff();
      _rampCounter = _rampCycles;
      newRelayState = false;
      return; 
    }
    digitalWrite(relayPin, false);
    if (_fanPower < 15) DimmerM::setPower(0);
    if ((_fanPower >= 15) and (_fanPower < 25)) DimmerM::setPower(50);
    if (_fanPower >= 25) DimmerM::setPower(_fanPower * 2);
  }
}

/*
void Fan2x::incPower(){
  if (_newPower >= 100) _incDir = false;
  if (_newPower == 0) _incDir = true;
  if (_incDir) _newPower++; else _newPower--;
}

void Fan2x::setRampTime(uint16_t rampTime){
  _rampCycles = rampTime * 2 * acFreq;
  _rampCounter = _rampCycles;
}

uint8_t Fan2x::getRampTime(){
  return _rampCycles /(2 * acFreq);
}

void Fan2x::zeroCross(){
  _power = _rampStartPower + ((int32_t) ((_newPower & _isOn) - _rampStartPower)) * _rampCounter / _rampCycles;
  triacTime = pgm_read_byte(&power2time[_power]);
  if (_rampCounter < _rampCycles ) _rampCounter++;
}
*/

void Fan2x::saveEEPROM(){
  if ( !useMQTT ) EEPROM.update(index*2, _fanPower);
  EEPROM.update(0, EEPROMdataVer);
}

void Fan2x::loadEEPROM(){
  if (EEPROM[0] != EEPROMdataVer) return;  // в EEPROM ничего еще не сохраняли
  setPower(EEPROM[index*2]);
  if (EEPROM[index*2+1]) _isOn = 0xff; else _isOn = 0;
}

uint8_t Fan2x::getPower(){
  return _fanPower;
}


/* 
bool Fan2x::getOnOff(){
  return _isOn;
}

void Fan2x::setOn(){
  _isOn = 0xff;
  _rampCounter = 0;
  _rampStartPower = _power;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);
}

void Fan2x::setOff(){
  _isOn = 0;
  _rampCounter = 0;
  _rampStartPower = _power;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);
}

void Fan2x::toggle(){
  _isOn = ~_isOn;
  _rampCounter = 0;
  _rampStartPower = _power;
  if ( !useMQTT ) EEPROM.update(index*2+1, _isOn);
  EEPROM.update(0, EEPROMdataVer);  
}

*/
