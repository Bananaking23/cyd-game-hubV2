// Host simulation shim for the Arduino-ESP32 core (just what the Game Hub uses).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
using std::min;
using std::max;
#undef abs
#define abs(x) ((x) > 0 ? (x) : -(x))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define PI 3.1415926535897932384626433832795
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define ESP_ARDUINO_VERSION_MAJOR 2
typedef bool boolean;
typedef uint8_t byte;
extern uint32_t g_millis;
inline uint32_t millis() { return g_millis; }
inline uint32_t micros() { return g_millis * 1000u; }
extern void (*g_delayHook)(uint32_t ms);
inline void delay(uint32_t ms) { g_millis += ms; if (g_delayHook) g_delayHook(ms); }
inline void yield() {}
inline long random(long n) { return n <= 0 ? 0 : (long)(rand() % n); }
inline long random(long a, long b) { return b <= a ? a : a + (long)(rand() % (b - a)); }
inline void randomSeed(unsigned long s) { srand((unsigned)s); }
inline uint32_t esp_random() { return (uint32_t)rand(); }
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return 1; }
extern int g_toneCalls;
inline double ledcSetup(uint8_t, double, uint8_t) { return 0; }
inline void ledcAttachPin(uint8_t, uint8_t) {}
inline void ledcWrite(uint8_t, uint32_t) {}
inline double ledcWriteTone(uint8_t, double) { g_toneCalls++; return 0; }
struct HardwareSerial { void begin(unsigned long) {} };
extern HardwareSerial Serial;
class Print {};
#include <string>
#include <time.h>
class String { std::string s_; public: String() {} String(const char* c) : s_(c ? c : "") {} unsigned length() const { return (unsigned)s_.size(); } const char* c_str() const { return s_.c_str(); } };
inline void configTime(long, int, const char*, const char* = 0, const char* = 0) {}
extern int g_restarts;
struct EspClass { void restart() { g_restarts++; } };
extern EspClass ESP;
void setup();
void loop();
