// v1.2 от 26.04.2020 - Повышена стабильность
// v1.3 от 08.09.2021 - Разбил на .h и .cpp
// v1.4 от 07.01.2022 - Чуть сократил код, поддержка GyverCore
// v1.5 от 18.02.2022 - оптимизация, поддержка esp32

#ifndef _microOneWire_h
#define _microOneWire_h
#include <Arduino.h>

bool oneWire_reset(uint8_t pin, uint8_t wrtpin);
void oneWire_write(uint8_t data, uint8_t wrtpin);
uint8_t oneWire_read(uint8_t pin, uint8_t wrtpin);
#endif