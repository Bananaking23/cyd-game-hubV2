#pragma once
#include <Arduino.h>
#define VSPI 3
class SPIClass { public: SPIClass(uint8_t = VSPI) {} void begin(int8_t = -1, int8_t = -1, int8_t = -1, int8_t = -1) {} };
