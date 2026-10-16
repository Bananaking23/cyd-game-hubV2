// =============================================================================
//  clock.h - clock screensaver (digital or analog) with optional date.
//
//  * Settings -> Screensaver -> START runs it; SET UP opens the customisation screen (style, 12/24 h, seconds, date format,
//    colour, brightness, time zone, automatic start after a period of no touching on the menu).
//  * The time comes from NTP over the saved Wi-Fi (see net.h). The ESP32 keeps counting after that, so Wi-Fi is only
//    needed once after power-up (and again every few days, to correct drift). Wi-Fi is switched off again right away.
//  * Optional weather (Open-Meteo, no account or key): temperature, condition and today's high/low for the city of the
//    selected time zone, refreshed every 30 minutes over the saved Wi-Fi (switched on briefly, then off again).
//  * A touch anywhere ends the screensaver. Like net.h, it is a blocking sub-screen with its own small loop.
//  * Time zones are POSIX rules, so daylight-saving changes happen by themselves.
// =============================================================================
#pragma once
#include "net.h"

struct ClockCfg { uint8_t style, h12, secs, dateFmt, color, bright, tz, idle, wx; };
static ClockCfg gClk = {0, 0, 1, 1, 0, 2, 1, 0, 1};   // digital, 24 h, seconds on, short date, cyan, brightness 2, London, auto-start off, weather in Celsius

static const int CLK_NSTYLE = 2, CLK_NDATE = 6, CLK_NCOL = 8, CLK_NTZ = 17, CLK_NIDLE = 5;
static const char* const CLK_STYLE_NAMES[CLK_NSTYLE] = {"DIGITAL", "ANALOG"};
static const char* const CLK_DATE_NAMES[CLK_NDATE] = {"OFF", "Wed 14 Oct", "14/10/2026", "10/14/2026", "2026-10-14", "Full date"};
static const char* const CLK_COL_NAMES[CLK_NCOL] = {"CYAN", "GREEN", "GOLD", "ORANGE", "PINK", "MAGENTA", "RED", "WHITE"};   // WHITE is dark ink in the light theme
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
  size_t got = prefs.getBytes("clk", &c, sizeof c);
  if (got < 8) return;                                             // nothing saved: keep the defaults (8 = record from before weather existed)
  gClk.wx      = got >= sizeof c ? (uint8_t)clampi(c.wx, 0, 2) : (uint8_t)1;
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

// ------------------------------------------------------------------ colours (themed)
static const uint16_t CLK_COLS_LIGHT[CLK_NCOL] = {RGB565(0, 120, 175), RGB565(0, 140, 50), RGB565(185, 125, 0), RGB565(215, 85, 0),
                                                  RGB565(205, 45, 115), RGB565(165, 0, 165), RGB565(200, 20, 30), RGB565(20, 24, 36)};
static uint16_t clkCol() {
  int i = clampi(gClk.color, 0, CLK_NCOL - 1);
  return uiLight() ? CLK_COLS_LIGHT[i] : CLK_COLS[i];
}

// ------------------------------------------------------------------ weather (Open-Meteo)
// One city per time zone (the screensaver already knows the zone). UTC has no city, so no weather there.
static const float CLK_TZ_LAT[CLK_NTZ] = {999.f, 51.51f, 48.86f, 37.98f, 55.76f, 25.20f, 28.61f, 13.76f, 39.90f,
                                          35.68f, -33.87f, -36.85f, -23.55f, 40.71f, 41.88f, 39.74f, 34.05f};
static const float CLK_TZ_LON[CLK_NTZ] = {0.f, -0.13f, 2.35f, 23.73f, 37.62f, 55.27f, 77.21f, 100.50f, 116.41f,
                                          139.69f, 151.21f, 174.76f, -46.63f, -74.01f, -87.63f, -104.99f, -118.24f};
static const char* const CLK_WX_UNIT_NAMES[3] = {"OFF", "CELSIUS", "FAHRENHEIT"};
static const uint32_t WX_REFRESH_MS = 30UL * 60UL * 1000UL;   // normal refresh
static const uint32_t WX_RETRY_MS = 10UL * 60UL * 1000UL;     // after a failure
static const uint32_t WX_MAX_AGE_S = 3UL * 3600UL;            // older data is hidden rather than shown wrong

struct WxData { bool have, hasHL; uint8_t code, day, tz; int16_t t10, hi10, lo10; uint32_t stamp; };   // temperatures: tenths of a degree C
static WxData gWx = {false, false, 0, 1, 0, 0, 0, 0, 0};
static uint32_t gWxNextMs = 0;

static bool wxActive() { return gClk.wx != 0 && gClk.tz != 0 && gClk.tz < CLK_NTZ; }
static bool wxValid() {
  if (!wxActive() || !gWx.have || gWx.tz != gClk.tz) return false;
  uint32_t now = (uint32_t)netTimeFn(nullptr);
  return now >= gWx.stamp && now - gWx.stamp < WX_MAX_AGE_S;
}

enum { WXK_CLEAR, WXK_PARTLY, WXK_CLOUD, WXK_FOG, WXK_RAIN, WXK_SNOW, WXK_STORM };
// WMO weather code -> icon kind + short name (<= 8 characters, so it fits beside the analog face)
static int wxKind(int code, const char** name) {
  const char* n = "";
  int k = WXK_CLOUD;
  if (code == 0) { n = "Clear"; k = WXK_CLEAR; }
  else if (code == 1) { n = "Fair"; k = WXK_CLEAR; }
  else if (code == 2) { n = "P.Cloudy"; k = WXK_PARTLY; }
  else if (code == 3) { n = "Overcast"; k = WXK_CLOUD; }
  else if (code == 45 || code == 48) { n = "Fog"; k = WXK_FOG; }
  else if (code >= 51 && code <= 57) { n = "Drizzle"; k = WXK_RAIN; }
  else if (code >= 61 && code <= 67) { n = "Rain"; k = WXK_RAIN; }
  else if (code >= 71 && code <= 77) { n = "Snow"; k = WXK_SNOW; }
  else if (code >= 80 && code <= 82) { n = "Showers"; k = WXK_RAIN; }
  else if (code == 85 || code == 86) { n = "Snow"; k = WXK_SNOW; }
  else if (code >= 95 && code <= 99) { n = "Storm"; k = WXK_STORM; }
  if (name) *name = n;
  return k;
}

// Temperature in the chosen unit, rounded; never "-0".
static int wxShow(int t10) {
  float v = (float)t10 / 10.0f;
  if (gClk.wx == 2) v = v * 1.8f + 32.0f;
  return (int)floorf(v + 0.5f);
}

// ---- parsing (the body is small: a few hundred bytes)
static bool wxNum(const char* js, const char* key, float* out) {
  const char* p;
  if (!jsonFind(js, key, &p)) return false;
  if (*p == '[') p++;
  char* end;
  double v = strtod(p, &end);
  if (end == p || !(v > -100.0 && v < 1000.0)) return false;
  *out = (float)v;
  return true;
}
static int16_t wxTenths(float c) {
  if (c < -90.f) c = -90.f;
  if (c > 70.f) c = 70.f;
  return (int16_t)floorf(c * 10.0f + 0.5f);
}
// Fill d from an Open-Meteo forecast body. Needs current.temperature_2m and current.weather_code.
static bool wxParse(const char* body, WxData& d) {
  const char* cur;
  if (!jsonFind(body, "current", &cur) || *cur != '{') return false;
  float t, code, day = 1;
  if (!wxNum(cur, "temperature_2m", &t) || !wxNum(cur, "weather_code", &code)) return false;
  wxNum(cur, "is_day", &day);
  memset(&d, 0, sizeof d);
  d.t10 = wxTenths(t);
  d.code = (uint8_t)clampi((int)code, 0, 255);
  d.day = day >= 0.5f ? 1 : 0;
  const char* dl;
  float hi, lo;
  if (jsonFind(body, "daily", &dl) && *dl == '{' && wxNum(dl, "temperature_2m_max", &hi) && wxNum(dl, "temperature_2m_min", &lo)) {
    d.hi10 = wxTenths(hi);
    d.lo10 = wxTenths(lo);
    d.hasHL = true;
  }
  d.have = true;
  return true;
}

static bool wxFetch(int tz, WxData& out) {
  if (tz <= 0 || tz >= CLK_NTZ) return false;
  char url[200];
  snprintf(url, sizeof url,
           "https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f&current=temperature_2m,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
           (double)CLK_TZ_LAT[tz], (double)CLK_TZ_LON[tz]);
  WiFiClientSecure client;
  client.setCACertBundle(hubRootBundle);
  HTTPClient http;
  http.setTimeout(7000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String body = http.getString();
  http.end();
  if (body.length() > 4000) return false;
  if (!wxParse(body.c_str(), out)) return false;
  out.tz = (uint8_t)tz;
  out.stamp = (uint32_t)netTimeFn(nullptr);
  return true;
}

// Join the saved Wi-Fi without drawing anything. 1 = connected, 0 = failed, 2 = the user touched the screen (abort).
static int wxConnectSilent(const WifiCreds& c) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(c.ssid, c.pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    netFrame();
    if (touch.pressed || touch.down) return 2;
    if (millis() - t0 > 9000) return 0;
  }
  return 1;
}
// Refresh the weather now (blocking for a few seconds). 1 = updated, 0 = failed, 2 = interrupted by a touch.
static int wxRefresh() {
  WifiCreds c;
  if (!wifiLoadCreds(c)) return 0;
  int up = wxConnectSilent(c);
  int res = up == 2 ? 2 : 0;
  if (up == 1) {
    WxData d;
    if (wxFetch(gClk.tz, d)) { gWx = d; res = 1; }
  }
  wifiOff();
  return res;
}
// When the next refresh is due, given what is cached.
static void wxSchedule(bool afterAttempt, bool ok) {
  if (afterAttempt) { gWxNextMs = millis() + (ok ? WX_REFRESH_MS : WX_RETRY_MS); return; }
  uint32_t now = (uint32_t)netTimeFn(nullptr);
  if (gWx.have && gWx.tz == gClk.tz && now >= gWx.stamp && now - gWx.stamp < WX_REFRESH_MS / 1000UL)
    gWxNextMs = millis() + WX_REFRESH_MS - (now - gWx.stamp) * 1000UL;
  else
    gWxNextMs = millis();
}
static bool wxDue() { return wxActive() && (int32_t)(millis() - gWxNextMs) >= 0; }

// ---- drawing
static void wxCloud(int cx, int cy, uint16_t col) {
  tft.fillCircle(cx - 8, cy + 2, 6, col);
  tft.fillCircle(cx, cy - 3, 8, col);
  tft.fillCircle(cx + 9, cy + 3, 5, col);
  tft.fillRect(cx - 8, cy + 2, 18, 7, col);
}
static void wxSun(int cx, int cy, int r, bool rays, uint16_t col) {
  tft.fillCircle(cx, cy, r, col);
  if (!rays) return;
  for (int i = 0; i < 8; i++) {
    float a = (float)i * CLK_PI / 4.0f;
    int x0 = cx + (int)floorf(cosf(a) * (float)(r + 3) + 0.5f), y0 = cy + (int)floorf(sinf(a) * (float)(r + 3) + 0.5f);
    int x1 = cx + (int)floorf(cosf(a) * (float)(r + 6) + 0.5f), y1 = cy + (int)floorf(sinf(a) * (float)(r + 6) + 0.5f);
    tft.drawLine(x0, y0, x1, y1, col);
  }
}
static void wxMoon(int cx, int cy, int r, uint16_t col) {
  tft.fillCircle(cx, cy, r, col);
  tft.fillCircle(cx + r / 2 + 1, cy - r / 3, r - 1, thBg());
}
// Icon drawn inside a box of about 36 x 32 centred on (cx, cy).
static void wxIcon(int cx, int cy, int kind, bool day) {
  bool lt = uiLight();
  uint16_t sunC = lt ? RGB565(225, 150, 0) : C_GOLD, moonC = lt ? RGB565(110, 120, 165) : RGB565(235, 235, 175);
  uint16_t cloudC = lt ? RGB565(120, 130, 150) : RGB565(190, 200, 215), blueC = lt ? RGB565(0, 100, 200) : RGB565(70, 150, 255);
  uint16_t snowC = lt ? RGB565(70, 130, 180) : C_WHITE;
  switch (kind) {
    case WXK_CLEAR:
      if (day) wxSun(cx, cy, 8, true, sunC); else wxMoon(cx, cy, 10, moonC);
      break;
    case WXK_PARTLY:
      if (day) wxSun(cx - 8, cy - 6, 6, true, sunC); else wxMoon(cx - 8, cy - 6, 7, moonC);
      wxCloud(cx + 3, cy + 4, cloudC);
      break;
    case WXK_CLOUD:
      wxCloud(cx, cy, cloudC);
      break;
    case WXK_FOG:
      for (int i = 0; i < 4; i++) tft.fillRect(cx - 14 + (i % 2) * 3, cy - 10 + i * 6, 24, 3, cloudC);
      break;
    case WXK_RAIN:
      wxCloud(cx, cy - 5, cloudC);
      for (int i = 0; i < 3; i++) tft.drawLine(cx - 9 + i * 8, cy + 7, cx - 11 + i * 8, cy + 13, blueC);
      for (int i = 0; i < 3; i++) tft.drawLine(cx - 8 + i * 8, cy + 7, cx - 10 + i * 8, cy + 13, blueC);
      break;
    case WXK_SNOW:
      wxCloud(cx, cy - 5, cloudC);
      for (int i = 0; i < 3; i++) tft.fillCircle(cx - 8 + i * 8, cy + 10, 2, snowC);
      break;
    default:   // storm
      wxCloud(cx, cy - 5, cloudC);
      tft.fillTriangle(cx + 1, cy + 3, cx - 5, cy + 11, cx + 1, cy + 11, sunC);
      tft.fillTriangle(cx + 1, cy + 8, cx + 7, cy + 8, cx - 2, cy + 15, sunC);
      break;
  }
}
// "12" + small ring + "C" starting at x (left edge), vertically centred on cy. Returns the width used.
static int wxDrawTemp(int x, int cy, int t10, uint16_t col, bool draw) {
  char num[8];
  snprintf(num, sizeof num, "%d", wxShow(t10));
  const char* u = gClk.wx == 2 ? "F" : "C";
  int wn = tft.textWidth(num, 4), wu = tft.textWidth(u, 2);
  if (draw) {
    text(num, x, cy, 4, col, ML_DATUM);
    tft.drawCircle(x + wn + 4, cy - 8, 2, col);
    text(u, x + wn + 8, cy - 2, 2, col, ML_DATUM);
  }
  return wn + 8 + wu;
}
static uint16_t wxHiCol() { return uiLight() ? RGB565(200, 70, 0) : C_ORANGE; }
static uint16_t wxLoCol() { return uiLight() ? RGB565(0, 100, 200) : RGB565(90, 170, 255); }

// What is on the screen now, so it is only repainted when something changed.
static struct { int8_t on; WxData d; uint8_t unit; } clkWxShown;
static bool wxSame(const WxData& a, const WxData& b) {
  return a.have == b.have && a.hasHL == b.hasHL && a.code == b.code && a.day == b.day && a.tz == b.tz &&
         a.t10 == b.t10 && a.hi10 == b.hi10 && a.lo10 == b.lo10;
}

// Digital face: one centred row along the bottom: icon, temperature, condition, high / low.
static void wxDrawRow(int cy) {
  tft.fillRect(0, cy - 16, SCR_W, 34, thBg());
  const char* name;
  int kind = wxKind(gWx.code, &name);
  char hi[12], lo[12];
  snprintf(hi, sizeof hi, "H%d", wxShow(gWx.hi10));
  snprintf(lo, sizeof lo, "L%d", wxShow(gWx.lo10));
  int wT = wxDrawTemp(0, cy, gWx.t10, 0, false);
  int wC = name[0] ? tft.textWidth(name, 2) : 0;
  int wH = gWx.hasHL ? tft.textWidth(hi, 2) + 8 + tft.textWidth(lo, 2) : 0;
  int total = 36 + 8 + wT + (wC ? 14 + wC : 0) + (wH ? 14 + wH : 0);
  if (total > SCR_W - 8) { wC = 0; total = 36 + 8 + wT + (wH ? 14 + wH : 0); }
  int x = (SCR_W - total) / 2;
  wxIcon(x + 18, cy, kind, gWx.day != 0);
  x += 36 + 8;
  wxDrawTemp(x, cy, gWx.t10, thText(), true);
  x += wT;
  if (wC) { x += 14; text(name, x, cy, 2, thMute(), ML_DATUM); x += wC; }
  if (wH) {
    x += 14;
    text(hi, x, cy, 2, wxHiCol(), ML_DATUM);
    x += tft.textWidth(hi, 2) + 8;
    text(lo, x, cy, 2, wxLoCol(), ML_DATUM);
  }
}
static int clkAnaCy();
// Analog face: icon + temperature on the left, condition and high / low on the right of the dial.
static void wxDrawSides(int cy) {
  tft.fillRect(0, cy - 46, 74, 92, thBg());
  tft.fillRect(SCR_W - 74, cy - 46, 74, 92, thBg());
  const char* name;
  int kind = wxKind(gWx.code, &name);
  wxIcon(37, cy - 18, kind, gWx.day != 0);
  int wT = wxDrawTemp(0, cy, gWx.t10, 0, false);
  wxDrawTemp(37 - wT / 2, cy + 20, gWx.t10, thText(), true);
  int rx = SCR_W - 37;
  if (name[0]) text(name, rx, cy - 20, 2, thMute(), MC_DATUM);
  if (gWx.hasHL) {
    char b[12];
    snprintf(b, sizeof b, "H%d", wxShow(gWx.hi10));
    text(b, rx, cy + 4, 2, wxHiCol(), MC_DATUM);
    snprintf(b, sizeof b, "L%d", wxShow(gWx.lo10));
    text(b, rx, cy + 24, 2, wxLoCol(), MC_DATUM);
  }
}
static const int WX_ROW_Y = 212;   // centre of the digital weather row
static void wxClearShown() { memset(&clkWxShown, 0, sizeof clkWxShown); clkWxShown.on = -1; }

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
  tft.fillRect(0, cy - 16, SCR_W, 32, thBg());
  text(b, SCR_W / 2, cy, font, thText(), MC_DATUM);
}

// ------------------------------------------------------------------ digital face (big, solid seven-segment digits)
static const uint8_t CLK_SEG7[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   // bits 0..6 = segments a..g

// One digit cell; d < 0 leaves the cell blank. Only lit segments are drawn (no ghost segments: they made it hard to read).
static void clkSeg7(int x, int y, int w, int h, int t, int d, uint16_t col) {
  tft.fillRect(x, y, w, h, thBg());
  if (d < 0) return;
  uint8_t m = CLK_SEG7[d % 10];
  int mid = y + (h - t) / 2;
  int uy = y + t + 1, uh = mid - (y + t) - 2;                 // upper vertical segments
  int ly = mid + t + 1, lh = (y + h - t) - (mid + t) - 2;     // lower vertical segments
  int bx = x + t + 1, bw = w - 2 * t - 2;                     // horizontal segments
  if (m & 0x01) tft.fillRect(bx, y, bw, t, col);               // a
  if (m & 0x02) tft.fillRect(x + w - t, uy, t, uh, col);       // b
  if (m & 0x04) tft.fillRect(x + w - t, ly, t, lh, col);       // c
  if (m & 0x08) tft.fillRect(bx, y + h - t, bw, t, col);       // d
  if (m & 0x10) tft.fillRect(x, ly, t, lh, col);               // e
  if (m & 0x20) tft.fillRect(x, uy, t, uh, col);               // f
  if (m & 0x40) tft.fillRect(bx, mid, bw, t, col);             // g
}

// Digit geometry depends on what is shown: without seconds the digits are as large as the screen allows.
struct ClkLayout {
  int bw, bh, bt;         // big digit cell + stroke
  int sw, sh, st;         // small (seconds) digit cell + stroke
  int xs[4];              // x of H1 H2 M1 M2
  int colonX, colonW;
  int x0, y0, sx, sy, apx, apy, dateY;
};

static ClkLayout clkLayout() {
  ClkLayout L;
  bool date = gClk.dateFmt != 0;
  if (gClk.secs) {
    L.bw = 44; L.bh = 100; L.bt = 11; L.sw = 26; L.sh = 52; L.st = 7;
    int pitch = 52, colonW = 18;
    int total = 4 * pitch - (pitch - L.bw) * 2 + colonW + 10 + (L.sw + 6) + L.sw;   // digits, colon, :SS
    total = 222 + 10 + 32 + L.sw;                                                    // = 290
    L.x0 = (SCR_W - total) / 2;
    L.xs[0] = L.x0; L.xs[1] = L.x0 + 52; L.xs[2] = L.x0 + 126; L.xs[3] = L.x0 + 178;
    L.colonX = L.x0 + 104; L.colonW = colonW;
    L.sx = L.x0 + 232;
    L.apx = L.sx;
  } else if (gClk.h12) {
    L.bw = 52; L.bh = 108; L.bt = 12; L.sw = L.sh = L.st = 0;
    L.x0 = (SCR_W - 296) / 2;
    L.xs[0] = L.x0; L.xs[1] = L.x0 + 60; L.xs[2] = L.x0 + 144; L.xs[3] = L.x0 + 204;
    L.colonX = L.x0 + 116; L.colonW = 24;
    L.apx = L.x0 + 268; L.sx = 0;
  } else {
    L.bw = 60; L.bh = 112; L.bt = 13; L.sw = L.sh = L.st = 0;
    L.x0 = (SCR_W - 292) / 2;
    L.xs[0] = L.x0; L.xs[1] = L.x0 + 68; L.xs[2] = L.x0 + 164; L.xs[3] = L.x0 + 232;
    L.colonX = L.x0 + 136; L.colonW = 28;
    L.apx = 0; L.sx = 0;
  }
  L.y0 = date ? 30 : (SCR_H - L.bh) / 2;
  L.sy = L.y0 + L.bh - L.sh;
  L.apy = gClk.secs ? L.y0 : L.y0 + L.bh - 16;
  L.dateY = L.y0 + L.bh + 38;
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
  wxClearShown();
}

static void clkDigital(const struct tm& t) {
  ClkLayout L = clkLayout();
  uint16_t col = clkCol();
  int hour = clampi(t.tm_hour, 0, 23), mi = clampi(t.tm_min, 0, 59), se = clampi(t.tm_sec, 0, 59);
  int hr = hour;
  if (gClk.h12) { hr = hour % 12; if (hr == 0) hr = 12; }
  int v[6] = {hr / 10, hr % 10, mi / 10, mi % 10, se / 10, se % 10};
  if (gClk.h12 && v[0] == 0) v[0] = -1;                         // no leading zero in 12 h mode
  int n = gClk.secs ? 6 : 4;
  for (int i = 0; i < n; i++) {
    if (v[i] == clkCell[i]) continue;
    clkCell[i] = (int8_t)v[i];
    if (i < 4)       clkSeg7(L.xs[i], L.y0, L.bw, L.bh, L.bt, v[i], col);
    else if (i == 4) clkSeg7(L.sx, L.sy, L.sw, L.sh, L.st, v[i], col);
    else             clkSeg7(L.sx + L.sw + 6, L.sy, L.sw, L.sh, L.st, v[i], col);
  }
  int colon = (gClk.secs || (se % 2) == 0) ? 1 : 0;              // without seconds the colon blinks
  if (colon != clkColon) {
    clkColon = (int8_t)colon;
    int dot = L.bt + 2;
    int cx = L.colonX + (L.colonW - dot) / 2;
    tft.fillRect(cx, L.y0 + L.bh / 3 - dot / 2, dot, dot, colon ? col : thBg());
    tft.fillRect(cx, L.y0 + 2 * L.bh / 3 - dot / 2, dot, dot, colon ? col : thBg());
  }
  if (gClk.h12) {
    int am = hour < 12 ? 1 : 0;
    if (am != clkAm) {
      clkAm = (int8_t)am;
      tft.fillRect(L.apx, L.apy, 36, 16, thBg());
      text(am ? "AM" : "PM", L.apx, L.apy, 2, thText());
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
static int clkAnaR() { return (gClk.dateFmt != 0 || wxActive()) ? 84 : 108; }
static int clkAnaCy() { return gClk.dateFmt != 0 ? 94 : SCR_H / 2; }
static const int CLK_HAND_W[3] = {7, 5, 2};

static void clkPolar(float deg, int len, int* x, int* y) {
  float a = deg * CLK_PI / 180.0f;
  *x = SCR_W / 2 + (int)floorf(sinf(a) * (float)len + 0.5f);
  *y = clkAnaCy() - (int)floorf(cosf(a) * (float)len + 0.5f);
}
static void clkAnalogFace() {
  uint16_t col = clkCol();
  int R = clkAnaR(), cx = SCR_W / 2, cy = clkAnaCy();
  tft.drawCircle(cx, cy, R, col);
  tft.drawCircle(cx, cy, R - 1, col);
  tft.drawCircle(cx, cy, R - 2, col);
  for (int i = 0; i < 60; i++) {
    int x0, y0, x1, y1;
    if (i % 5 == 0) {
      clkPolar((float)i * 6.0f, R - 16, &x0, &y0);
      clkPolar((float)i * 6.0f, R - 5, &x1, &y1);
      clkThick(x0, y0, x1, y1, (i % 15 == 0) ? 5 : 3, thText());
    } else {
      clkPolar((float)i * 6.0f, R - 8, &x0, &y0);
      tft.fillRect(x0, y0, 2, 2, thMute());
    }
  }
}
static void clkAnalog(const struct tm& t) {
  uint16_t col = clkCol();
  uint16_t secCol = uiLight() ? C_RED : C_WHITE;
  if (gClk.color == CLK_NCOL - 2) secCol = uiLight() ? RGB565(20, 24, 36) : C_WHITE;     // red hands: keep the second hand different
  int R = clkAnaR(), cx = SCR_W / 2, cy = clkAnaCy();
  int hour = clampi(t.tm_hour, 0, 23), mi = clampi(t.tm_min, 0, 59), se = clampi(t.tm_sec, 0, 59);
  int32_t key = hour * 3600 + mi * 60 + (gClk.secs ? se : 0);
  if (key == clkHandKey) return;
  clkHandKey = key;
  if (clkHandOn) {                                   // erase the old hands (they never reach the tick marks)
    for (int i = 0; i < 3; i++)
      if (i < 2 || gClk.secs) clkThick(cx, cy, clkHand[i][0], clkHand[i][1], CLK_HAND_W[i], thBg());
  }
  float degH = (float)(hour % 12) * 30.0f + (float)mi * 0.5f;
  float degM = (float)mi * 6.0f + (float)se * 0.1f;
  float degS = (float)se * 6.0f;
  clkPolar(degH, R * 5 / 10, &clkHand[0][0], &clkHand[0][1]);
  clkPolar(degM, R - 28, &clkHand[1][0], &clkHand[1][1]);
  clkPolar(degS, R - 20, &clkHand[2][0], &clkHand[2][1]);
  clkThick(cx, cy, clkHand[0][0], clkHand[0][1], CLK_HAND_W[0], col);
  clkThick(cx, cy, clkHand[1][0], clkHand[1][1], CLK_HAND_W[1], col);
  if (gClk.secs) clkThick(cx, cy, clkHand[2][0], clkHand[2][1], CLK_HAND_W[2], secCol);
  tft.fillCircle(cx, cy, 7, col);
  clkHandOn = true;
}

// Paint the whole clock for time t (clears the screen first).
static void clockDrawFull(const struct tm& t) {
  tft.fillScreen(thBg());
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
  bool on = wxValid();
  bool was = clkWxShown.on == 1;
  if (on != was || (on && (!wxSame(gWx, clkWxShown.d) || clkWxShown.unit != gClk.wx))) {
    if (on) {
      if (gClk.style == 1) wxDrawSides(clkAnaCy()); else wxDrawRow(WX_ROW_Y);
    } else {                                           // data went stale or was switched off: wipe it
      if (gClk.style == 1) { int cy = clkAnaCy(); tft.fillRect(0, cy - 46, 74, 92, thBg()); tft.fillRect(SCR_W - 74, cy - 46, 74, 92, thBg()); }
      else tft.fillRect(0, WX_ROW_Y - 16, SCR_W, 34, thBg());
    }
    clkWxShown.on = on ? 1 : 0;
    clkWxShown.d = gWx;
    clkWxShown.unit = gClk.wx;
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
  wxSchedule(false, false);
  bool first = true;
  int lastSec = -1;
  for (;;) {
    netFrame();
    if (touch.pressed) break;
    if (!first && wxDue()) {                         // the clock is already up; freshen the weather (a few seconds, blocking)
      int r = wxRefresh();
      if (r == 2) break;                             // touched while connecting
      wxSchedule(true, r == 1);
      netFrame();
      if (touch.pressed || touch.down) break;        // touched during the download
      lastSec = -1;
    }
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
static const int CLK_ROWS = 9, CLK_ROW_H = 23;
static const char* const CLK_ROW_NAMES[CLK_ROWS] = {"Style", "Time format", "Seconds", "Date", "Colour", "Brightness", "Time zone", "Weather", "Auto-start"};

static void clkRowLabel(int row, char* out, size_t n) {
  switch (row) {
    case 0: snprintf(out, n, "%s", CLK_STYLE_NAMES[clampi(gClk.style, 0, CLK_NSTYLE - 1)]); break;
    case 1: snprintf(out, n, "%s", gClk.h12 ? "12 HOUR" : "24 HOUR"); break;
    case 2: snprintf(out, n, "%s", gClk.secs ? "ON" : "OFF"); break;
    case 3: snprintf(out, n, "%s", CLK_DATE_NAMES[clampi(gClk.dateFmt, 0, CLK_NDATE - 1)]); break;
    case 4: snprintf(out, n, "%s", CLK_COL_NAMES[clampi(gClk.color, 0, CLK_NCOL - 1)]); break;
    case 5: snprintf(out, n, "%d / 4", (int)gClk.bright); break;
    case 6: snprintf(out, n, "%s", CLK_TZ_NAMES[clampi(gClk.tz, 0, CLK_NTZ - 1)]); break;
    case 7: snprintf(out, n, "%s", (gClk.wx && gClk.tz == 0) ? "SET ZONE" : CLK_WX_UNIT_NAMES[clampi(gClk.wx, 0, 2)]); break;
    default: snprintf(out, n, "%s", CLK_IDLE_NAMES[clampi(gClk.idle, 0, CLK_NIDLE - 1)]); break;
  }
}
static void clkDrawRow(int row) {
  int y = 25 + row * CLK_ROW_H;
  tft.fillRect(0, y, SCR_W, CLK_ROW_H, thBg());
  tft.fillRoundRect(6, y, SCR_W - 12, CLK_ROW_H - 2, 6, RGB565(22, 26, 38));
  text(CLK_ROW_NAMES[row], 16, y + 10, 2, C_WHITE, ML_DATUM);
  char b[20];
  clkRowLabel(row, b, sizeof b);
  uint16_t bg = RGB565(40, 90, 150), fg = C_WHITE;
  if (row == 4) { bg = CLK_COLS[clampi(gClk.color, 0, CLK_NCOL - 1)]; fg = C_BLACK; }
  button(SCR_W - 112, y + 2, 98, CLK_ROW_H - 6, b, bg, fg);
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
    case 7: gClk.wx = (uint8_t)((gClk.wx + 3 + d) % 3); break;
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
      if (inRect(px, py, 6, 25 + r * CLK_ROW_H, SCR_W - 12, CLK_ROW_H - 1)) {
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
