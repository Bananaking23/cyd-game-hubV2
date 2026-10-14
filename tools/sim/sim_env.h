// Definitions of every global the host shims declare. Include ONCE, before the sketch, in each sim main.
#pragma once
uint32_t g_millis = 1000;
int g_toneCalls = 0;
HardwareSerial Serial;
bool g_simTouched = false;
int g_simRawX = 0, g_simRawY = 0;
uint16_t g_fb[240 * 320];
long g_draws = 0, g_offscreen = 0;
void (*g_delayHook)(uint32_t) = 0;
int g_restarts = 0;
EspClass ESP;
int g_scanCount = 12, g_wifiPoll = 0;
bool g_wifiPassOK = true, g_wifiPending = false, g_wifiUp = false;
WiFiClass WiFi;
long g_netBytes = 0;
int g_httpMode = 0;
long g_binSize = 300000;
long g_updWritten = 0;
bool g_updBegun = false;
UpdateClass Update;
