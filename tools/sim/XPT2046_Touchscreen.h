#pragma once
#include <Arduino.h>
#include <SPI.h>
extern bool g_simTouched; extern int g_simRawX, g_simRawY;
class TS_Point { public: TS_Point() : x(0), y(0), z(0) {} TS_Point(int16_t X, int16_t Y, int16_t Z) : x(X), y(Y), z(Z) {} int16_t x, y, z; };
class XPT2046_Touchscreen {
 public:
  XPT2046_Touchscreen(uint8_t, uint8_t = 255) {}
  bool begin(SPIClass&) { return true; }
  bool begin() { return true; }
  bool touched() { return g_simTouched; }
  TS_Point getPoint() { return TS_Point((int16_t)g_simRawX, (int16_t)g_simRawY, 500); }
  void setRotation(uint8_t) {}
};
