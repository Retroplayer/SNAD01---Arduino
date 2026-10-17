#pragma once
#include <stdint.h>
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
void digitalWrite(uint8_t, uint8_t);
void pinMode(uint8_t, uint8_t);
int digitalRead(uint8_t);
void delayMicroseconds(unsigned int);
