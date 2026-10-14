// Host shim of the ESP32 Arduino WiFi API subset used by net.h. Behaviour is scripted through the g_* globals (see sim_env.h).
#pragma once
#include <Arduino.h>
enum { WIFI_OFF = 0, WIFI_STA = 1 };
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };
extern int g_scanCount, g_wifiPoll;
extern bool g_wifiPassOK, g_wifiPending, g_wifiUp;
struct WiFiClass {
  void mode(int) {}
  bool disconnect(bool = false, bool = false) { g_wifiUp = false; g_wifiPending = false; return true; }
  int begin(const char*, const char*) { g_wifiPending = true; g_wifiPoll = 0; return 0; }
  int status() {
    if (g_wifiPending && ++g_wifiPoll >= 3) { g_wifiPending = false; g_wifiUp = g_wifiPassOK; }
    return g_wifiUp ? WL_CONNECTED : WL_DISCONNECTED;
  }
  int scanNetworks() { return g_scanCount; }
  String SSID(int i) { char b[40]; if (i % 9 == 8) b[0] = 0; else snprintf(b, sizeof b, "Net-%d-with-a-rather-long-name-xyz", i % 7); return String(b); }
  int32_t RSSI(int i) { return -35 - (i * 7) % 60; }
  int encryptionType(int i) { return (i % 3 == 1) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK; }
  void scanDelete() {}
};
extern WiFiClass WiFi;
