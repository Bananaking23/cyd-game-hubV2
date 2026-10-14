// =============================================================================
//  clock.h - clock screensaver (digital or analog) with optional date.
//
//  * Settings -> Screensaver -> START runs it; SET UP opens the customisation screen (style, 12/24 h, seconds, date format,
//    colour, brightness, time zone, automatic start after a period of no touching on the menu).
//  * The time comes from NTP over the saved Wi-Fi (see net.h). The ESP32 keeps counting after that, so Wi-Fi is only
//    needed once after power-up (and again every few days, to correct drift). Wi-Fi is switched off again right away.
//  * A touch anywhere ends the screensaver. Like net.h, it is a blocking sub-screen with its own small loop.
//  * Time zones are POSIX rules, so daylight-saving changes happen by themselves.
// =============================================================================
#pragma once
#include "net.h"

struct ClockCfg { uint8_t style, h12, secs, dateFmt, color, bright, tz, idle; };
static ClockCfg gClk = {0, 0, 1, 1, 0, 2, 1, 0};   // digital, 24 h, seconds on, short date, cyan, brightness 2, London, auto-start off

static const int CLK_NSTYLE = 2, CLK_NDATE = 6, CLK_NCOL = 8, CLK_NTZ = 17, CLK_NIDLE = 5;
static const char* const CLK_STYLE_NAMES[CLK_NSTYLE] = {"DIGITAL", "ANALOG"};
static const char* const CLK_DATE_NAMES[CLK_NDATE] = {"OFF", "Wed 14 Oct", "14/10/2026", "10/14/2026", "2026-10-14", "Full date"};
static const char* const CLK_COL_NAMES[CLK_NCOL] = {"CYAN", "GREEN", "GOLD", "ORANGE", "PINK", "MAGENTA", "RED", "WHITE"};
static const uint16_t CLK_COLS[CLK_NCOL] = {C_CYAN, C_GREEN, C_GOLD, C_ORANGE, C_PINK, C_MAGENTA, C_RED, C_WHITE};
static const char* const CLK_IDLE_NAMES[CLK_NIDLE] = {"OFF", "1 MIN", "5 MIN", "10 MIN", "30 MIN"};
static const uint8_t CLK_IDLE_MIN[CLK_NIDLE] = {0, 1, 5, 10, 30};
static const char* const CLK_TZ_NAMES[CLK_NTZ] = {"UTC", "London", "Paris", "Athens", "Moscow", "Dubai", "Delhi", "Bangkok", "Beijing",
                                                  "Tokyo", "Sydney", "Auckland", "Sao Paulo", "New York", "Chicago", "Denver", "Los Ang."};
static const char* const CLK_TZ_RULES[CLK_NTZ] = {
  "UTC0", "GMT0BST,M3.5.0/1,M10.5.0", "CET-1CEST,M3.5.0,M10.5.0/3", "EET-2EEST,M3.5.0/3,M10.5.0/4", "MSK-3", "<+04>-4", "IST-5:30",
  "<+07>-7", "CST-8", "JST-9", "AEST-10AEDT,M10.1.0,M4.1.0/3", "NZST-12NZDT,M9.5.0,M4.1.0/3", "<-03>3",
  "EST5EDT,M3.2.0,M11.1.0", "CST6CDT,M3.2.0,M11.1.0", "MST7MDT,M3.2.0,M11.1.0", "PST8PDT,M3.2.0,M11.1.0"};

static const float CLK_PI = 3.14159265f;
static uint32_t gClkSynced = 0;          // epoch seconds of the last successful NTP sync this session (0 = none yet)

static void clockCfgLoad() {
  ClockCfg c;
  memset(&c, 0, sizeof c);
  if (prefs.getBytes("clk", &c, sizeof c) != sizeof c) return;     // nothing saved: keep the defaults
  gClk.style   = (uint8_t)clampi(c.style, 0, CLK_NSTYLE - 1);
  gClk.h12     = c.h12 ? 1 : 0;
  gClk.secs    = c.secs ? 1 : 0;
  gClk.dateFmt = (uint8_t)clampi(c.dateFmt, 0, CLK_NDATE - 1);
  gClk.color   = (uint8_t)clampi(c.color, 0, CLK_NCOL - 1);
  gClk.bright  = (uint8_t)clampi(c.bright, 1, 4);
  gClk.tz      = (uint8_t)clampi(c.tz, 0, CLK_NTZ - 1);
  gClk.idle    = (uint8_t)clampi(c.idle, 0, CLK_NIDLE - 1);
}
static void clockCfgSave() { prefs.putBytes("clk", &gClk, sizeof gClk); }

static void clockApplyTz() {
  setenv("TZ", CLK_TZ_RULES[clampi(gClk.tz, 0, CLK_NTZ - 1)], 1);
  tzset();
}
static bool clockTimeValid() { return (long)netTimeFn(nullptr) >= 1700000000L; }

// Make sure the system clock is set. interactive = the user asked for the clock (may connect to Wi-Fi and show messages);
// otherwise (automatic start) it only reports whether the time is already known.
static bool clockEnsureTime(bool interactive) {
  bool valid = clockTimeValid();
  uint32_t nowS = (uint32_t)netTimeFn(nullptr);
  bool stale = valid && (gClkSynced == 0 || nowS - gClkSynced > 3UL * 86400UL);
  if (valid && !stale) return true;
  if (!interactive) return valid;
  WifiCreds c;
  if (!wifiLoadCreds(c)) {
    if (!valid) netMessage("NO TIME", C_ORANGE, "Set up Wi-Fi first:", "Settings > Wi-Fi");
    return valid;
  }
  bool ok = wifiConnect(c.ssid, c.pass);
  if (ok) {
    netBusy("Clock", "Getting the time...");
    ok = netSyncClock();
  }
  wifiOff();
  if (ok) {
    gClkSynced = (uint32_t)netTimeFn(nullptr);
    return true;
  }
  if (!valid) netMessage("NO TIME", C_RED, "Could not get the", "time. Try again.");
  return valid;
}

// ------------------------------------------------------------------ date text
static void clkDateStr(const struct tm& t, int fmt, char* out, size_t n) {
  static const char* const WD[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* const WDL[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  static const char* const MO[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const char* const MOL[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                                      "October", "November", "December"};
  int wd = clampi(t.tm_wday, 0, 6), mo = clampi(t.tm_mon, 0, 11), d = clampi(t.tm_mday, 1, 31);
  int y = clampi(t.tm_year + 1900, 0, 9999);
  switch (fmt) {
    case 1:  snprintf(out, n, "%s %d %s", WD[wd], d, MO[mo]); break;
    case 2:  snprintf(out, n, "%02d/%02d/%04d", d, mo + 1, y); break;
    case 3:  snprintf(out, n, "%02d/%02d/%04d", mo + 1, d, y); break;
    case 4:  snprintf(out, n, "%04d-%02d-%02d", y, mo + 1, d); break;
    default: snprintf(out, n, "%s %d %s", WDL[wd], d, MOL[mo]); break;
  }
}
static void clkDrawDate(const struct tm& t, int cy) {
  char b[64];
  clkDateStr(t, gClk.dateFmt, b, sizeof b);
  uint8_t font = (tft.textWidth(b, 4) <= 300) ? 4 : 2;
  tft.fillRect(0, cy - 16, SCR_W, 32, C_BLACK);
  text(b, SCR_W / 2, cy, font, C_LGREY, MC_DATUM);
}

// ------------------------------------------------------------------ digital face (seven-segment digits)
static const uint8_t CLK_SEG7[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   // bits 0..6 = segments a..g

// One digit cell; d < 0 leaves the cell blank. Unlit segments are drawn very dark so the digit looks like an LCD.
static void clkSeg7(int x, int y, int w, int h, int t, int d, uint16_t col) {
  tft.fillRect(x, y, w, h, C_BLACK);
  if (d < 0) return;
  uint8_t m = CLK_SEG7[d % 10];
  uint16_t dim = RGB565(14, 17, 26);
  int mid = y + (h - t) / 2;
  int uy = y + t + 1, uh = mid - (y + t) - 2;                 // upper vertical segments
  int ly = mid + t + 1, lh = (y + h - t) - (mid + t) - 2;     // lower vertical segments
  int bx = x + t + 1, bw = w - 2 * t - 2;                     // horizontal segments
  tft.fillRect(bx, y, bw, t, (m & 0x01) ? col : dim);               // a
  tft.fillRect(x + w - t, uy, t, uh, (m & 0x02) ? col : dim);       // b
  tft.fillRect(x + w - t, ly, t, lh, (m & 0x04) ? col : dim);       // c
  tft.fillRect(bx, y + h - t, bw, t, (m & 0x08) ? col : dim);       // d
  tft.fillRect(x, ly, t, lh, (m & 0x10) ? col : dim);               // e
  tft.fillRect(x, uy, t, uh, (m & 0x20) ? col : dim);               // f
  tft.fillRect(bx, mid, bw, t, (m & 0x40) ? col : dim);             // g
}

struct ClkLayout { int x0, y0, sx, sy, apx, apy, dateY; };
static const int CLK_BW = 46, CLK_BH = 84, CLK_BT = 10, CLK_SW = 22, CLK_SH = 40, CLK_ST = 5;

static ClkLayout clkLayout() {
  ClkLayout L;
  int total = 228;                                   // HH:MM
  if (gClk.secs) total += 12 + 2 * CLK_SW + 4;       // + :SS in small digits
  else if (gClk.h12) total += 8 + 36;                // + AM/PM
  L.x0 = (SCR_W - total) / 2;
  bool date = gClk.dateFmt != 0;
  L.y0 = date ? 42 : (SCR_H - CLK_BH) / 2;
  L.sx = L.x0 + 240;
  L.sy = L.y0 + CLK_BH - CLK_SH;
  if (gClk.secs) { L.apx = L.x0 + 240; L.apy = L.y0; }
  else           { L.apx = L.x0 + 236; L.apy = L.y0 + CLK_BH - 16; }
  L.dateY = L.y0 + CLK_BH + 44;
  return L;
}

// Redraw caches: what is currently on the screen, so that only changes are painted.
static int8_t  clkCell[6];
static int8_t  clkColon, clkAm;
static int32_t clkDateKey;
static int32_t clkHandKey;
static int     clkHand[3][2];
static bool    clkHandOn;

static void clkResetCaches() {
  for (int i = 0; i < 6; i++) clkCell[i] = -2;
  clkColon = clkAm = -1;
  clkDateKey = -1;
  clkHandKey = -1;
  clkHandOn = false;
}

static void clkDigital(const struct tm& t) {
  ClkLayout L = clkLayout();
  uint16_t col = CLK_COLS[clampi(gClk.color, 0, CLK_NCOL - 1)];
  int hour = clampi(t.tm_hour, 0, 23), mi = clampi(t.tm_min, 0, 59), se = clampi(t.tm_sec, 0, 59);
  int hr = hour;
  if (gClk.h12) { hr = hour % 12; if (hr == 0) hr = 12; }
  int v[6] = {hr / 10, hr % 10, mi / 10, mi % 10, se / 10, se % 10};
  if (gClk.h12 && v[0] == 0) v[0] = -1;                         // no leading zero in 12 h mode
  int xs[6] = {L.x0, L.x0 + 54, L.x0 + 128, L.x0 + 182, L.sx, L.sx + 26};
  int n = gClk.secs ? 6 : 4;
  for (int i = 0; i < n; i++) {
    if (v[i] == clkCell[i]) continue;
    clkCell[i] = (int8_t)v[i];
    if (i < 4) clkSeg7(xs[i], L.y0, CLK_BW, CLK_BH, CLK_BT, v[i], col);
    else       clkSeg7(xs[i], L.sy, CLK_SW, CLK_SH, CLK_ST, v[i], col);
  }
  int colon = (gClk.secs || (se % 2) == 0) ? 1 : 0;              // without seconds the colon blinks
  if (colon != clkColon) {
    clkColon = (int8_t)colon;
    int cx = L.x0 + 108 + (16 - CLK_BT) / 2;
    tft.fillRect(cx, L.y0 + CLK_BH / 3 - CLK_BT / 2, CLK_BT, CLK_BT, colon ? col : C_BLACK);
    tft.fillRect(cx, L.y0 + 2 * CLK_BH / 3 - CLK_BT / 2, CLK_BT, CLK_BT, colon ? col : C_BLACK);
  }
  if (gClk.h12) {
    int am = hour < 12 ? 1 : 0;
    if (am != clkAm) {
      clkAm = (int8_t)am;
      tft.fillRect(L.apx, L.apy, 36, 16, C_BLACK);
      text(am ? "AM" : "PM", L.apx, L.apy, 2, C_LGREY);
    }
  }
}

// ------------------------------------------------------------------ analog face
static void clkThick(int x0, int y0, int x1, int y1, int w, uint16_t c) {
  tft.drawLine(x0, y0, x1, y1, c);
  bool horiz = iabs(x1 - x0) > iabs(y1 - y0);
  for (int i = 1; i <= w / 2; i++) {
    if (horiz) { tft.drawLine(x0, y0 - i, x1, y1 - i, c); tft.drawLine(x0, y0 + i, x1, y1 + i, c); }
    else       { tft.drawLine(x0 - i, y0, x1 - i, y1, c); tft.drawLine(x0 + i, y0, x1 + i, y1, c); }
  }
}
static int clkAnaR() { return gClk.dateFmt != 0 ? 84 : 108; }
static int clkAnaCy() { return gClk.dateFmt != 0 ? 94 : SCR_H / 2; }
static const int CLK_HAND_W[3] = {5, 3, 1};

static void clkPolar(float deg, int len, int* x, int* y) {
  float a = deg * CLK_PI / 180.0f;
  *x = SCR_W / 2 + (int)floorf(sinf(a) * (float)len + 0.5f);
  *y = clkAnaCy() - (int)floorf(cosf(a) * (float)len + 0.5f);
}
static void clkAnalogFace() {
  uint16_t col = CLK_COLS[clampi(gClk.color, 0, CLK_NCOL - 1)];
  int R = clkAnaR(), cx = SCR_W / 2, cy = clkAnaCy();
  tft.drawCircle(cx, cy, R, col);
  tft.drawCircle(cx, cy, R - 1, col);
  for (int i = 0; i < 60; i++) {
    int x0, y0, x1, y1;
    if (i % 5 == 0) {
      clkPolar((float)i * 6.0f, R - 14, &x0, &y0);
      clkPolar((float)i * 6.0f, R - 4, &x1, &y1);
      clkThick(x0, y0, x1, y1, (i % 15 == 0) ? 3 : 1, col);
    } else {
      clkPolar((float)i * 6.0f, R - 6, &x0, &y0);
      tft.fillRect(x0, y0, 2, 2, C_GREY);
    }
  }
}
static void clkAnalog(const struct tm& t) {
  uint16_t col = CLK_COLS[clampi(gClk.color, 0, CLK_NCOL - 1)];
  uint16_t secCol = (gClk.color == CLK_NCOL - 1) ? C_RED : C_WHITE;
  int R = clkAnaR(), cx = SCR_W / 2, cy = clkAnaCy();
  int hour = clampi(t.tm_hour, 0, 23), mi = clampi(t.tm_min, 0, 59), se = clampi(t.tm_sec, 0, 59);
  int32_t key = hour * 3600 + mi * 60 + (gClk.secs ? se : 0);
  if (key == clkHandKey) return;
  clkHandKey = key;
  if (clkHandOn) {                                   // erase the old hands (they never reach the tick marks)
    for (int i = 0; i < 3; i++)
      if (i < 2 || gClk.secs) clkThick(cx, cy, clkHand[i][0], clkHand[i][1], CLK_HAND_W[i], C_BLACK);
  }
  float degH = (float)(hour % 12) * 30.0f + (float)mi * 0.5f;
  float degM = (float)mi * 6.0f + (float)se * 0.1f;
  float degS = (float)se * 6.0f;
  clkPolar(degH, R * 5 / 10, &clkHand[0][0], &clkHand[0][1]);
  clkPolar(degM, R - 26, &clkHand[1][0], &clkHand[1][1]);
  clkPolar(degS, R - 18, &clkHand[2][0], &clkHand[2][1]);
  clkThick(cx, cy, clkHand[0][0], clkHand[0][1], CLK_HAND_W[0], col);
  clkThick(cx, cy, clkHand[1][0], clkHand[1][1], CLK_HAND_W[1], col);
  if (gClk.secs) clkThick(cx, cy, clkHand[2][0], clkHand[2][1], CLK_HAND_W[2], secCol);
  tft.fillCircle(cx, cy, 5, col);
  clkHandOn = true;
}

// Paint the whole clock for time t (clears the screen first).
static void clockDrawFull(const struct tm& t) {
  tft.fillScreen(C_BLACK);
  clkResetCaches();
  if (gClk.style == 1) clkAnalogFace();
}
// Bring the screen up to date for time t (cheap when nothing changed).
static void clockUpdate(const struct tm& t) {
  if (gClk.style == 1) clkAnalog(t); else clkDigital(t);
  if (gClk.dateFmt != 0) {
    int32_t key = (clampi(t.tm_year, 0, 9000) * 400 + clampi(t.tm_yday, 0, 365)) * 8 + gClk.dateFmt;
    if (key != clkDateKey) {
      clkDateKey = key;
      clkDrawDate(t, gClk.style == 1 ? 208 : clkLayout().dateY);
    }
  }
}

// The screensaver itself. autostart = started by the idle timer (never asks for Wi-Fi). A touch ends it.
static void clockRun(bool autostart) {
  if (!clockEnsureTime(!autostart)) return;
  clockApplyTz();
  uint8_t keepBright = gBright;
  gBright = (uint8_t)clampi(gClk.bright, 1, 4);
  applyBrightness();
  netWaitRelease();
  bool first = true;
  int lastSec = -1;
  for (;;) {
    netFrame();
    if (touch.pressed) break;
    time_t now = netTimeFn(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (!first && t.tm_sec == lastSec) continue;
    lastSec = t.tm_sec;
    if (first) { clockDrawFull(t); first = false; }
    clockUpdate(t);
  }
  gBright = keepBright;
  applyBrightness();
  netWaitRelease();
}

// ------------------------------------------------------------------ customisation screen
static const int CLK_ROWS = 8;
static const char* const CLK_ROW_NAMES[CLK_ROWS] = {"Style", "Time format", "Seconds", "Date", "Colour", "Brightness", "Time zone", "Auto-start"};

static void clkRowLabel(int row, char* out, size_t n) {
  switch (row) {
    case 0: snprintf(out, n, "%s", CLK_STYLE_NAMES[clampi(gClk.style, 0, CLK_NSTYLE - 1)]); break;
    case 1: snprintf(out, n, "%s", gClk.h12 ? "12 HOUR" : "24 HOUR"); break;
    case 2: snprintf(out, n, "%s", gClk.secs ? "ON" : "OFF"); break;
    case 3: snprintf(out, n, "%s", CLK_DATE_NAMES[clampi(gClk.dateFmt, 0, CLK_NDATE - 1)]); break;
    case 4: snprintf(out, n, "%s", CLK_COL_NAMES[clampi(gClk.color, 0, CLK_NCOL - 1)]); break;
    case 5: snprintf(out, n, "%d / 4", (int)gClk.bright); break;
    case 6: snprintf(out, n, "%s", CLK_TZ_NAMES[clampi(gClk.tz, 0, CLK_NTZ - 1)]); break;
    default: snprintf(out, n, "%s", CLK_IDLE_NAMES[clampi(gClk.idle, 0, CLK_NIDLE - 1)]); break;
  }
}
static void clkDrawRow(int row) {
  int y = 25 + row * 26;
  tft.fillRect(0, y, SCR_W, 26, C_BLACK);
  tft.fillRoundRect(6, y, SCR_W - 12, 23, 6, RGB565(22, 26, 38));
  text(CLK_ROW_NAMES[row], 16, y + 11, 2, C_WHITE, ML_DATUM);
  char b[16];
  clkRowLabel(row, b, sizeof b);
  uint16_t bg = RGB565(40, 90, 150), fg = C_WHITE;
  if (row == 4) { bg = CLK_COLS[clampi(gClk.color, 0, CLK_NCOL - 1)]; fg = C_BLACK; }
  button(SCR_W - 112, y + 2, 98, 19, b, bg, fg);
}
static void clkDrawScreen() {
  hudBegin("Clock style");
  button(120, 2, 84, HUD_H - 5, "PREVIEW", RGB565(30, 120, 80), C_WHITE, 1);
  for (int i = 0; i < CLK_ROWS; i++) clkDrawRow(i);
}
// Change a row by +1 (tap on the button side) or -1 (tap on the name side).
static void clkChange(int row, int dir) {
  int d = dir < 0 ? -1 : 1;
  switch (row) {
    case 0: gClk.style = (uint8_t)((gClk.style + CLK_NSTYLE + d) % CLK_NSTYLE); break;
    case 1: gClk.h12 = !gClk.h12; break;
    case 2: gClk.secs = !gClk.secs; break;
    case 3: gClk.dateFmt = (uint8_t)((gClk.dateFmt + CLK_NDATE + d) % CLK_NDATE); break;
    case 4: gClk.color = (uint8_t)((gClk.color + CLK_NCOL + d) % CLK_NCOL); break;
    case 5: gClk.bright = (uint8_t)((gClk.bright - 1 + 4 + d) % 4 + 1); break;
    case 6: gClk.tz = (uint8_t)((gClk.tz + CLK_NTZ + d) % CLK_NTZ); break;
    default: gClk.idle = (uint8_t)((gClk.idle + CLK_NIDLE + d) % CLK_NIDLE); break;
  }
}
static void clockCustomise() {
  clkDrawScreen();
  netWaitRelease();
  for (;;) {
    netFrame();
    if (menuHit()) { netWaitRelease(); break; }
    if (!touch.tap) continue;
    int px = touch.x, py = touch.y;
    if (inRect(px, py, 114, 0, 96, HUD_H + 2)) {
      beep(900, 25);
      touchClear();
      clockRun(false);
      clkDrawScreen();
      continue;
    }
    for (int r = 0; r < CLK_ROWS; r++) {
      if (inRect(px, py, 6, 25 + r * 26, SCR_W - 12, 25)) {
        clkChange(r, px < 150 ? -1 : 1);
        beep(1000, 20);
        clkDrawRow(r);
        break;
      }
    }
  }
  clockCfgSave();
  touchClear();
}

// Menu idle timer: true when the screensaver should start now (menu screen only, time already known).
static bool clockIdleDue(uint32_t idleMs) {
  uint8_t i = (uint8_t)clampi(gClk.idle, 0, CLK_NIDLE - 1);
  if (i == 0) return false;
  return idleMs >= (uint32_t)CLK_IDLE_MIN[i] * 60000UL && clockTimeValid();
}
