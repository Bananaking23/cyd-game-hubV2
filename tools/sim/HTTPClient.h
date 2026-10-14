#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
enum { HTTP_CODE_OK = 200, HTTP_CODE_NOT_FOUND = 404 };
enum { HTTPC_DISABLE_FOLLOW_REDIRECTS = 0, HTTPC_STRICT_FOLLOW_REDIRECTS = 1, HTTPC_FORCE_FOLLOW_REDIRECTS = 2 };
extern int g_httpMode;     // 0 normal, 1 json garbage, 2 size mismatch, 3 server error, 4 stream stalls
extern long g_binSize;
class HTTPClient {
  std::string url_;
  WiFiClient c_;
 public:
  bool begin(WiFiClient&, const String& u) { url_ = u.c_str(); return url_.rfind("https://", 0) == 0; }
  void setTimeout(uint16_t) {}
  void setFollowRedirects(int) {}
  int GET() {
    if (g_httpMode == 3) return 503;
    if (url_.size() > 5 && url_.compare(url_.size() - 5, 5, ".json") == 0) return 200;
    g_netBytes = (g_httpMode == 4) ? g_binSize / 2 : g_binSize;
    return 200;
  }
  int getSize() { return g_httpMode == 2 ? (int)g_binSize - 1 : (int)g_binSize; }
  String getString() {
    if (g_httpMode == 1) return String("<html>not json</html>");
    char b[256];
    snprintf(b, sizeof b, "{\n \"version\": \"1.0.99\",\n \"build\": 99,\n \"size\": %ld,\n \"md5\": \"0123456789abcdef0123456789abcdef\",\n \"path\": \"firmware/app-ili9341.bin\"\n}\n", g_binSize);
    return String(b);
  }
  WiFiClient* getStreamPtr() { return &c_; }
  bool connected() { return g_netBytes > 0; }
  void end() {}
};
