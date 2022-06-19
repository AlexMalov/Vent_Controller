#include "microOneWire.h"
#ifdef __AVR__
#define MOW_CLI() uint8_t oldSreg = SREG; cli();
#define MOW_SEI() SREG = oldSreg
#else
#define MOW_CLI()
#define MOW_SEI()
#endif
#define _t600 600
#define _t60 60
#define _t5 5
#define _t8 8
#define _t2 3

bool oneWire_reset(uint8_t pin, uint8_t wrtpin) { 
    pinMode(wrtpin, OUTPUT);
    delayMicroseconds(_t600);
    pinMode(wrtpin, INPUT);
    MOW_CLI();
    delayMicroseconds(_t60);
    bool pulse = digitalRead(pin);
    MOW_SEI();
    delayMicroseconds(_t600);
    return !pulse;
}

void oneWire_write(uint8_t data, uint8_t wrtpin) {
    for (uint8_t i = 8; i; i--) {
        pinMode(wrtpin, OUTPUT);
        MOW_CLI();
        if (data & 1) {
            delayMicroseconds(_t5);
            pinMode(wrtpin, INPUT);
            delayMicroseconds(_t60);
        } else {
            delayMicroseconds(_t60);
            pinMode(wrtpin, INPUT);
            delayMicroseconds(_t5);
        }
        MOW_SEI();
        data >>= 1;
    }
}

uint8_t oneWire_read(uint8_t pin, uint8_t wrtpin) {
    uint8_t data = 0;
    for (uint8_t i = 8; i; i--) {
        data >>= 1;
        MOW_CLI();
        pinMode(wrtpin, OUTPUT);
        delayMicroseconds(_t2);
        pinMode(wrtpin, INPUT);
        delayMicroseconds(_t8);
        if (digitalRead(pin)) data |= (1 << 7);
        delayMicroseconds(_t60);
        MOW_SEI();
    }
    return data;
}