// Recording TFT stub: keeps a 320x240 framebuffer + counters. Same public API subset as the real library.
#pragma once
#include <Arduino.h>
#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define CL_DATUM 3
#define MC_DATUM 4
#define CC_DATUM 4
#define MR_DATUM 5
#define CR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8
extern uint16_t g_fb[240 * 320];
extern long g_draws, g_offscreen;
class TFT_eSPI : public Print {
  void px(int x, int y, uint32_t c) { if (x >= 0 && y >= 0 && x < 320 && y < 240) g_fb[y * 320 + x] = (uint16_t)c; }
  void box(int x, int y, int w, int h, uint32_t c) {
    g_draws++;
    if (w < 0 || h < 0) { g_offscreen += 1000; return; }       // negative sizes are a bug
    if (x < 0 || y < 0 || x + w > 320 || y + h > 240) g_offscreen++;
    for (int j = imax_(y, 0); j < imin_(y + h, 240); j++) for (int i = imax_(x, 0); i < imin_(x + w, 320); i++) g_fb[j * 320 + i] = (uint16_t)c;
  }
  static int imax_(int a, int b) { return a > b ? a : b; }
  static int imin_(int a, int b) { return a < b ? a : b; }
 public:
  TFT_eSPI(int16_t = 240, int16_t = 320) {}
  void init(uint8_t = 0) {} void begin(uint8_t = 0) {} void setRotation(uint8_t) {} void invertDisplay(bool) {}
  int16_t width() { return 320; } int16_t height() { return 240; }
  void fillScreen(uint32_t c) { box(0, 0, 320, 240, c); }
  void drawPixel(int32_t x, int32_t y, uint32_t c) { g_draws++; if (x < 0 || y < 0 || x >= 320 || y >= 240) g_offscreen++; px(x, y, c); }
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) { g_draws++; if (x0 < 0 || x1 < 0 || y0 < 0 || y1 < 0 || x0 > 319 || x1 > 319 || y0 > 239 || y1 > 239) g_offscreen++; }
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t c) { box(x, y, 1, h, c); }
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) { box(x, y, w, 1, c); }
  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { g_draws++; if (w < 0 || h < 0) g_offscreen += 1000; }
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { box(x, y, w, h, c); }
  void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) { g_draws++; if (w < 0 || h < 0) g_offscreen += 1000; }
  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) { box(x, y, w, h, c); }
  void drawCircle(int32_t x, int32_t y, int32_t r, uint32_t c) { g_draws++; if (r < 0) g_offscreen += 1000; }
  void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c) { box(x - r, y - r, 2 * r + 1, 2 * r + 1, c); }
  void drawEllipse(int16_t, int16_t, int32_t, int32_t, uint16_t) { g_draws++; }
  void fillEllipse(int16_t x, int16_t y, int32_t rx, int32_t ry, uint16_t c) { box(x - rx, y - ry, 2 * rx + 1, 2 * ry + 1, c); }
  void drawTriangle(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) { g_draws++; }
  void fillTriangle(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) { g_draws++; }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
  void setCursor(int16_t, int16_t) {}
  void setTextColor(uint16_t) {} void setTextColor(uint16_t, uint16_t, bool = false) {}
  void setTextSize(uint8_t) {} void setTextFont(uint8_t) {} void setTextDatum(uint8_t) {} void setTextPadding(uint16_t) {}
  int16_t textWidth(const char* s, uint8_t = 2) { return (int16_t)(strlen(s) * 8); }
  int16_t fontHeight(int16_t = 2) { return 16; }
  int16_t drawString(const char* s, int32_t x, int32_t y, uint8_t f) { if (!s) g_offscreen += 1000; g_draws++; return 0; }
  int16_t drawString(const char* s, int32_t x, int32_t y) { return drawString(s, x, y, 2); }
  int16_t drawString(const String&, int32_t, int32_t, uint8_t) { return 0; }
  int16_t drawString(const String&, int32_t, int32_t) { return 0; }
  int16_t drawCentreString(const char*, int32_t, int32_t, uint8_t) { g_draws++; return 0; }
  int16_t drawRightString(const char*, int32_t, int32_t, uint8_t) { g_draws++; return 0; }
  int16_t drawNumber(long, int32_t, int32_t, uint8_t) { g_draws++; return 0; }
  int16_t drawNumber(long, int32_t, int32_t) { g_draws++; return 0; }
  int16_t drawFloat(float, uint8_t, int32_t, int32_t, uint8_t) { g_draws++; return 0; }
  int16_t drawFloat(float, uint8_t, int32_t, int32_t) { g_draws++; return 0; }
};
