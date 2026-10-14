#pragma once
#include <Arduino.h>
extern long g_netBytes;
class WiFiClient {
 public:
  int available() { return g_netBytes > 4096 ? 4096 : (int)g_netBytes; }
  int read(uint8_t* b, size_t n) { size_t c = (long)n > g_netBytes ? (size_t)g_netBytes : n; for (size_t i = 0; i < c; i++) b[i] = (uint8_t)i; g_netBytes -= (long)c; return (int)c; }
};
class WiFiClientSecure : public WiFiClient {
 public:
  void setCACertBundle(const uint8_t*) {}
  void setInsecure() {}
};
