#pragma once
#include <Arduino.h>
extern long g_updWritten; extern bool g_updBegun;
class UpdateClass {
 public:
  bool begin(size_t) { g_updBegun = true; g_updWritten = 0; return true; }
  bool setMD5(const char*) { return true; }
  size_t write(uint8_t*, size_t n) { g_updWritten += (long)n; return n; }
  bool end(bool = false) { g_updBegun = false; return true; }
  void abort() { g_updBegun = false; }
  const char* errorString() { return "sim error"; }
};
extern UpdateClass Update;
