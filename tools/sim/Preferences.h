#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include <vector>
class Preferences {
  std::map<std::string, std::vector<uint8_t> > m;
 public:
  bool begin(const char*, bool = false) { return true; }
  size_t putBytes(const char* k, const void* v, size_t n) { m[k] = std::vector<uint8_t>((const uint8_t*)v, (const uint8_t*)v + n); return n; }
  size_t getBytes(const char* k, void* b, size_t n) { auto it = m.find(k); if (it == m.end()) return 0; size_t c = std::min(n, it->second.size()); memcpy(b, it->second.data(), c); return c; }
  size_t putInt(const char* k, int32_t v) { return putBytes(k, &v, 4); }
  int32_t getInt(const char* k, int32_t d = 0) { int32_t v = d; if (m.count(k)) memcpy(&v, m[k].data(), 4); return v; }
  size_t putBool(const char* k, bool v) { return putBytes(k, &v, 1); }
  bool getBool(const char* k, bool d = false) { bool v = d; if (m.count(k)) v = m[k][0]; return v; }
  size_t putUChar(const char* k, uint8_t v) { return putBytes(k, &v, 1); }
  uint8_t getUChar(const char* k, uint8_t d = 0) { uint8_t v = d; if (m.count(k)) v = m[k][0]; return v; }
};
