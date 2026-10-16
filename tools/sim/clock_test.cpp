// Host test for the clock screensaver (clock.h): every style/format combination is drawn for many times and dates,
// the Settings buttons, customisation screen, time-sync paths and idle auto-start are driven with scripted taps.
#include <stdio.h>
#include <string>
#include <vector>
#include <time.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "sim_env.h"
#include "../../firmware/CydGameHub/CydGameHub.ino"

struct Ev { int x, y; };
static std::vector<Ev> q;
static size_t qi = 0;
static int hold = 0, gap = 0, idle = 0;
static void tapHook(uint32_t ms) {
  if (ms != 30) return;
  if (hold > 0) { hold--; g_simTouched = true; return; }
  g_simTouched = false;
  if (gap > 0) { gap--; return; }
  if (qi < q.size()) {
    int x = q[qi].x, y = q[qi].y; qi++;
    g_simRawX = cal.ax0 + (int)((long)(x - 20) * (cal.ax1 - cal.ax0) / 280);
    g_simRawY = cal.ay0 + (int)((long)(y - 20) * (cal.ay1 - cal.ay0) / 200);
    hold = 3; gap = 9; g_simTouched = true; hold--;
  } else if (++idle > 20000) { fprintf(stderr, "script exhausted but UI still waiting - stuck?\n"); exit(2); }
}
static void run(const std::vector<Ev>& v, int firstGap = 0) { q = v; qi = 0; hold = 0; gap = firstGap; idle = 0; g_simTouched = false; touch = TouchState{false, false, false, false, -1, 0, 0, 0, 0, 0, 0}; gMiss = 0; }

// fake time source: starts at fakeNow and advances fakeStep seconds per call; 0 = "clock not set yet"
static long fakeNow = 0, fakeStep = 0;
static bool fakeSynced = true;
static time_t fakeTime(time_t* t) { time_t v = (time_t)fakeNow; fakeNow += fakeStep; if (t) *t = v; return v; }

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

int main() {
  g_delayHook = tapHook;
  prefs.begin("sim"); settingsLoad(); clockCfgLoad(); cal = TouchCal{0, 400, 3700, 400, 3700};
  netTimeFn = fakeTime;
  Ev menu = {300, 10};

  // 1. render every combination at many instants (incl. midnight, noon, leap day, year end, DST changes)
  { long offs0 = g_offscreen;
    static const long stamps[] = {1767225600L, 1772323200L + 86399, 1709164800L + 43200, 1740787199L, 1711846800L, 1729994400L,
                                  1735689599L, 1760000000L, 1783000000L, 1800000000L, 1786000000L + 3599};
    int shown = 0;
    for (int light = 0; light < 2; light++)
    for (int style = 0; style < CLK_NSTYLE; style++)
      for (int h12 = 0; h12 < 2; h12++)
        for (int secs = 0; secs < 2; secs++)
          for (int df = 0; df < CLK_NDATE; df++)
            for (int col = 0; col < CLK_NCOL; col += 3)
              for (int tz = 0; tz < CLK_NTZ; tz += 2) {
                gClk = ClockCfg{(uint8_t)style, (uint8_t)h12, (uint8_t)secs, (uint8_t)df, (uint8_t)col, 2, (uint8_t)tz, 0};
                gLight = light; gCur = -1;
                clockApplyTz();
                bool first = true;
                for (size_t k = 0; k < sizeof stamps / sizeof stamps[0]; k++)
                  for (int step = 0; step < 4; step++) {
                    time_t tt = (time_t)(stamps[k] + step * 31);
                    struct tm t; localtime_r(&tt, &t);
                    if (first) { clockDrawFull(t); first = false; }
                    clockUpdate(t);
                    shown++;
                  }
              }
    gLight = false;
    CHECK(g_offscreen == offs0, "clock: every style/format/zone/theme draws only inside the screen");
    printf("      (%d frames rendered)\n", shown);
  }
  // 2. layout: digits never overlap each other, the colon, the seconds or the date, in every digital configuration
  { bool ok = true;
    for (int h12 = 0; h12 < 2; h12++) for (int secs = 0; secs < 2; secs++) for (int df = 0; df < CLK_NDATE; df++) {
      gClk.style = 0; gClk.h12 = h12; gClk.secs = secs; gClk.dateFmt = df;
      ClkLayout L = clkLayout();
      int right = L.xs[3] + L.bw;
      if (secs) right = L.sx + L.sw + 6 + L.sw; else if (h12) right = L.apx + 36;
      if (L.xs[0] < 0 || right > SCR_W) { ok = false; printf("      right edge %d (h12 %d secs %d)\n", right, h12, secs); }
      for (int i = 0; i < 3; i++) if (L.xs[i] + L.bw > L.xs[i + 1]) ok = false;                // digits do not overlap
      if (L.xs[1] + L.bw > L.colonX || L.colonX + L.colonW > L.xs[2]) ok = false;               // colon sits between the pairs
      if (secs && L.xs[3] + L.bw > L.sx) ok = false;                                           // seconds clear of minutes
      if (L.y0 < PF_Y - 20 || L.y0 + L.bh > SCR_H) ok = false;
      if (df && (L.dateY - 16 < L.y0 + L.bh || L.dateY + 16 > SCR_H)) ok = false;
    }
    CHECK(ok, "clock: digital layouts fit the 320x240 screen, nothing overlaps, date sits below the digits");
  }
  // 3. date strings
  { struct tm t; memset(&t, 0, sizeof t); t.tm_year = 126; t.tm_mon = 9; t.tm_mday = 14; t.tm_wday = 3; char b[64];
    clkDateStr(t, 1, b, sizeof b); bool a = !strcmp(b, "Wed 14 Oct");
    clkDateStr(t, 2, b, sizeof b); a &= !strcmp(b, "14/10/2026");
    clkDateStr(t, 3, b, sizeof b); a &= !strcmp(b, "10/14/2026");
    clkDateStr(t, 4, b, sizeof b); a &= !strcmp(b, "2026-10-14");
    clkDateStr(t, 5, b, sizeof b); a &= !strcmp(b, "Wednesday 14 October");
    CHECK(a, "clock: all five date formats");
    t.tm_mon = 8; t.tm_wday = 3; t.tm_mday = 30; clkDateStr(t, 5, b, sizeof b);
    CHECK(tft.textWidth(b, 4) > 0 && strlen(b) < 40, "clock: longest date fits its buffer");
  }
  // 4. time zones: fixed instants must land on the expected local hour (checks the DST rules)
  { time_t summer = 1752580800L;   // 2025-07-15 12:00:00 UTC
    time_t winter = 1768478400L;   // 2026-01-15 12:00:00 UTC
    struct { int tz; int hs, hw; } exp[] = {{0, 12, 12}, {1, 13, 12}, {2, 14, 13}, {13, 8, 7}, {10, 22, 23}, {6, 17, 17}, {12, 9, 9}, {8, 20, 20}};
    bool ok = true;
    for (auto& e : exp) {
      gClk.tz = e.tz; clockApplyTz(); struct tm t;
      localtime_r(&summer, &t); if (t.tm_hour != e.hs) { ok = false; printf("      tz %s summer got %d want %d\n", CLK_TZ_NAMES[e.tz], t.tm_hour, e.hs); }
      localtime_r(&winter, &t); if (t.tm_hour != e.hw) { ok = false; printf("      tz %s winter got %d want %d\n", CLK_TZ_NAMES[e.tz], t.tm_hour, e.hw); }
    }
    CHECK(ok, "clock: time-zone rules incl. daylight saving (UTC, London, Paris, New York, Sydney, Delhi, Auckland, Beijing)");
    gClk.tz = 1; clockApplyTz();
  }
  // 5. config persistence + sanitising
  { gClk = ClockCfg{1, 1, 0, 4, 5, 3, 9, 2}; clockCfgSave();
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0}; clockCfgLoad();
    bool ok = gClk.style == 1 && gClk.h12 == 1 && gClk.secs == 0 && gClk.dateFmt == 4 && gClk.color == 5 && gClk.bright == 3 && gClk.tz == 9 && gClk.idle == 2;
    CHECK(ok, "clock: settings survive save / load");
    ClockCfg bad = {200, 7, 9, 250, 99, 0, 250, 77}; prefs.putBytes("clk", &bad, sizeof bad); clockCfgLoad();
    ok = gClk.style < CLK_NSTYLE && gClk.dateFmt < CLK_NDATE && gClk.color < CLK_NCOL && gClk.bright >= 1 && gClk.bright <= 4 && gClk.tz < CLK_NTZ && gClk.idle < CLK_NIDLE;
    CHECK(ok, "clock: corrupt saved settings are clamped into range");
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0};
  }
  // 6. running screensaver: time advances 37 s per frame, a tap ends it, brightness is restored
  { fakeNow = 1760000000L; fakeStep = 37;
    for (int style = 0; style < 2; style++) for (int secs = 0; secs < 2; secs++) {
      gClk = ClockCfg{(uint8_t)style, (uint8_t)(secs), (uint8_t)secs, (uint8_t)(1 + style * 3), 2, 1, 1, 0};
      gBright = 4; long offs0 = g_offscreen;
      run({Ev{160, 120}}, 400); clockRun(false);
      CHECK(gBright == 4 && g_offscreen == offs0 && qi == 1, style ? "clock run: analog ends on tap, brightness restored" : "clock run: digital ends on tap, brightness restored");
    }
  }
  // 7. time not set yet: no Wi-Fi saved -> message; Wi-Fi saved and works -> clock starts; Wi-Fi fails -> message
  { fakeNow = 0; fakeStep = 0; gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0}; gClkSynced = 0; wifiForget();
    run({Ev{160, 120}}); clockRun(false);
    CHECK(qi == 1 && !clockTimeValid(), "clock start: no time and no Wi-Fi -> explains, does not start");
    // the fake clock becomes valid only after NTP "runs": jump it forward as soon as the Wi-Fi shim connects
    wifiSaveCreds("HomeNet", "secret123"); g_wifiPassOK = true;
    struct Jump { static time_t f(time_t* t) { static int calls = 0; static bool set = false; calls++; if (g_wifiUp) set = true; long v = (set ? 1760000000L + calls : 0); if (t) *t = (time_t)v; return (time_t)v; } };
    netTimeFn = Jump::f;
    run({Ev{160, 120}}, 300); clockRun(false);
    CHECK(gClkSynced != 0 && qi == 1, "clock start: no time + saved Wi-Fi -> syncs, shows clock, tap exits");
    netTimeFn = fakeTime; fakeNow = 0; gClkSynced = 0; g_wifiPassOK = false;
    run({Ev{160, 120}}); clockRun(false);
    CHECK(qi == 1 && gClkSynced == 0, "clock start: Wi-Fi unreachable -> message, no clock");
    g_wifiPassOK = true;
    // automatic start never touches Wi-Fi
    fakeNow = 0; run({}); bool started = clockEnsureTime(false);
    CHECK(!started, "clock auto-start: unknown time -> does not start, no Wi-Fi attempt");
    fakeNow = 1760000000L; fakeStep = 1; gClkSynced = 0; started = clockEnsureTime(false);
    CHECK(started, "clock auto-start: known time -> starts without Wi-Fi");
  }
  // 8. customisation screen: tap every row on the button side and the name side, PREVIEW, MENU; settings persist
  { fakeNow = 1760000000L; fakeStep = 37; gClkSynced = (uint32_t)fakeNow;
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0};
    std::vector<Ev> v;
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{280, 25 + r * CLK_ROW_H + 12});   // forward
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{40, 25 + r * CLK_ROW_H + 12});    // backward
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{280, 25 + r * CLK_ROW_H + 12});   // forward again
    v.push_back(menu);
    run(v); clockCustomise();
    ClockCfg a = gClk;
    CHECK(a.style == 1 && a.h12 == 1 && a.secs == 0 && a.dateFmt == 2 && a.color == 1 && a.bright == 3 && a.tz == 2 && a.wx == 1 && a.place == 1 && a.idle == 1,
          "clock set-up: every row changes forward and back; result as expected");
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0}; clockCfgLoad();
    CHECK(gClk.style == a.style && gClk.tz == a.tz && gClk.idle == a.idle, "clock set-up: saved on leaving");
    // wrap-around: cycle the time zone more than once
    std::vector<Ev> w; for (int i = 0; i < CLK_NTZ * 2 + 3; i++) w.push_back(Ev{280, 25 + 6 * CLK_ROW_H + 12}); w.push_back(menu);
    gClk.tz = 0; run(w); clockCustomise();
    CHECK(gClk.tz == 3, "clock set-up: time zone list wraps around");
    // PREVIEW (opens the clock; a tap closes it), then MENU
    run({Ev{160, 11}, Ev{160, 120}, menu}, 0); long o0 = g_offscreen;
    gClk.style = 0; clockCustomise();
    CHECK(qi == 3 && g_offscreen == o0, "clock set-up: PREVIEW shows the clock and returns to the screen");
  }
  // 9. Settings tile grid: every tile responds, Clock starts the screensaver, Clock style opens set-up, Scores asks first
  { gScreen = 1; drawSettings(); gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0};
    auto tapTile = [&](int i) { int x, y; settingTileRect(i, &x, &y); touch = TouchState{false, false, true, true, -1, x + 36, y + 33, x + 36, y + 33, 0, 0}; settingsTickScreen(); };
    // geometry: all tiles on screen, no overlaps, and clear of the HUD
    bool geo = true;
    for (int i = 0; i < SET_ITEMS; i++) {
      int x, y; settingTileRect(i, &x, &y);
      if (x < 0 || x + SET_TW > SCR_W || y < PF_Y || y + SET_TH > SCR_H) geo = false;
      for (int j = i + 1; j < SET_ITEMS; j++) { int x2, y2; settingTileRect(j, &x2, &y2); if (x < x2 + SET_TW && x2 < x + SET_TW && y < y2 + SET_TH && y2 < y + SET_TH) geo = false; }
    }
    CHECK(geo, "settings: ten tiles fit on screen without overlapping");
    run({menu}); tapTile(S_CLOCKSET);
    CHECK(qi == 1 && gScreen == 1, "settings: Clock style tile opens the set-up screen (MENU returns)");
    run({Ev{160, 120}}, 200); tapTile(S_CLOCK);
    CHECK(qi == 1 && gScreen == 1, "settings: Clock tile runs the screensaver, tap returns to Settings");
    bool snd = gSound; tapTile(S_SOUND); CHECK(gSound != snd, "settings: Sound tile toggles"); gSound = snd;
    int diff0 = gDiff; tapTile(S_DIFF); CHECK(gDiff != diff0, "settings: Difficulty tile cycles"); gDiff = (uint8_t)diff0;
    bool lt = gLight; tapTile(S_THEME); CHECK(gLight != lt && gScreen == 1, "settings: Theme tile switches light/dark");
    settingsSave(); gLight = !gLight; settingsLoad(); CHECK(gLight != lt, "settings: theme is saved"); gLight = false; settingsSave();
    int br = gBright; tapTile(S_BRIGHT); CHECK(gBright != br, "settings: Brightness tile cycles"); gBright = (uint8_t)br;
    bool inv = gInvert; tapTile(S_INVERT); CHECK(gInvert != inv, "settings: Invert tile toggles"); gInvert = inv;
    // Scores: CANCEL keeps them, DELETE clears them
    int w = 220, h = 112, ax = (SCR_W - w) / 2, ay = PF_Y + (PF_H - h) / 2;
    Ev yes = {ax + 10 + 48, ay + 78 + 13}, no = {ax + w - 10 - 48, ay + 78 + 13};
    gDiff = 1; hiSet(0, 77);
    run({no}); tapTile(S_SCORES); CHECK(hiGet(0) == 77, "settings: Scores tile asks first; CANCEL keeps the scores");
    run({yes}); tapTile(S_SCORES); CHECK(hiGet(0) == 0, "settings: Scores tile: DELETE clears the scores");
    gDiff = (uint8_t)diff0;
    // red/blue order (ST7789 build only: this test is compiled with HUB_VARIANT "st7789")
    gBgr = false; gFlip = false; applyDisplaySettings();
    CHECK(tft.lastCmd == 0x36 && tft.lastData == 0x60, "colour order: RGB landscape writes MADCTL 0x60");
    tapTile(S_ORDER); CHECK(gBgr && tft.lastData == 0x68, "colour order: Red/Blue tile switches to BGR (0x68)");
    gFlip = true; applyDisplaySettings(); CHECK(tft.lastData == 0xA8, "colour order: flipped rotation keeps BGR (0xA8)");
    gFlip = false; tapTile(S_ORDER); CHECK(!gBgr && tft.lastData == 0x60, "colour order: back to RGB");
    gBgr = false; settingsSave(); gBgr = true; settingsLoad(); CHECK(!gBgr, "colour order: saved with the settings");
    // a tap in the gaps / below the grid / on MENU
    touch = TouchState{false, false, true, true, -1, 6 + 73 + 2, 60, 79, 60, 0, 0}; settingsTickScreen();
    touch = TouchState{false, false, true, true, -1, 310, 5, 310, 5, 0, 0}; settingsTickScreen();
    CHECK(gScreen == 0, "settings: gap taps do nothing, MENU returns to the game menu");
  }
  // 10. idle timer
  { gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0};
    CHECK(!clockIdleDue(99UL * 60000UL), "idle: OFF never starts");
    gClk.idle = 1; fakeNow = 1760000000L; fakeStep = 0;
    CHECK(!clockIdleDue(59000UL) && clockIdleDue(60000UL), "idle: 1 minute starts after 60 s");
    gClk.idle = 4; CHECK(!clockIdleDue(29UL * 60000UL) && clockIdleDue(30UL * 60000UL), "idle: 30 minutes");
    fakeNow = 0; CHECK(!clockIdleDue(99UL * 60000UL), "idle: not without a known time");
  }

  // 11. weather: parsing, units, scheduling, refresh paths, persistence of the new setting, rendering of every icon/layout
  { gLight = false;
    // 11a. parsing (the real Open-Meteo layout, with units objects that repeat the key names before the real values)
    WxData d; memset(&d, 0, sizeof d);
    const char* good = "{\"latitude\":51.5,\"current_units\":{\"temperature_2m\":\"\\u00b0C\",\"weather_code\":\"wmo code\",\"is_day\":\"\"},"
                       "\"current\":{\"time\":\"2026-10-15T20:45\",\"interval\":900,\"temperature_2m\":-3.6,\"weather_code\":71,\"is_day\":0},"
                       "\"daily_units\":{\"temperature_2m_max\":\"C\"},\"daily\":{\"time\":[\"2026-10-15\"],\"temperature_2m_max\":[1.25],\"temperature_2m_min\":[-7.0]}}";
    CHECK(wxParse(good, d) && d.t10 == -36 && d.code == 71 && d.day == 0 && d.hasHL && d.hi10 == 13 && d.lo10 == -70, "weather: parses current + daily incl. negative values");
    CHECK(wxParse("{\"current\":{\"temperature_2m\":20,\"weather_code\":0}}", d) && d.t10 == 200 && d.day == 1 && !d.hasHL, "weather: missing is_day / daily is tolerated");
    CHECK(!wxParse("<html>nope</html>", d) && !wxParse("{\"current\":{\"temperature_2m\":\"x\",\"weather_code\":1}}", d) && !wxParse("{\"current\":{\"temperature_2m\":5}}", d) &&
          !wxParse("{\"current_units\":{\"temperature_2m\":\"C\"}}", d) && !wxParse("", d), "weather: garbage / incomplete bodies are rejected");
    CHECK(wxParse("{\"current\":{\"temperature_2m\":9999,\"weather_code\":900}}", d) == false || (d.t10 <= 700 && d.code <= 255), "weather: absurd values are clamped or rejected");
    // 11b. units + rounding
    gClk.wx = 1; bool u = wxShow(114) == 11 && wxShow(-4) == 0 && wxShow(-36) == -4 && wxShow(995) == 100 && wxShow(-95) == -9;
    gClk.wx = 2; u = u && wxShow(0) == 32 && wxShow(100) == 50 && wxShow(-400) == -40 && wxShow(-4) == 31;
    CHECK(u, "weather: Celsius / Fahrenheit conversion and rounding");
    // 11c. condition table
    const char* nm; bool k = wxKind(0, &nm) == WXK_CLEAR && !strcmp(nm, "Clear") && wxKind(2, &nm) == WXK_PARTLY && wxKind(3, &nm) == WXK_CLOUD && wxKind(48, &nm) == WXK_FOG &&
                             wxKind(55, &nm) == WXK_RAIN && wxKind(66, &nm) == WXK_RAIN && wxKind(75, &nm) == WXK_SNOW && wxKind(81, &nm) == WXK_RAIN && wxKind(86, &nm) == WXK_SNOW &&
                             wxKind(96, &nm) == WXK_STORM && wxKind(123, &nm) == WXK_CLOUD;
    bool shortNames = true; for (int c = 0; c < 256; c++) { wxKind(c, &nm); if (strlen(nm) > 8) shortNames = false; }
    CHECK(k && shortNames, "weather: WMO codes map to icons; names fit beside the analog face (<= 8 chars)");
    // 11d. activity / validity rules
    fakeNow = 1760000000L; fakeStep = 0;
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1}; gWx = WxData{true, true, 3, 1, 1, 114, 142, 81, (uint32_t)fakeNow};
    bool v1 = wxActive() && wxValid();
    fakeNow += 3 * 3600L - 1; bool v2 = wxValid(); fakeNow += 2; bool v3 = !wxValid();
    fakeNow = 1760000000L; gClk.tz = 2; bool v4 = !wxValid();                 // data belongs to another city
    gClk.tz = 0; bool v5 = !wxActive() && !wxValid();                         // UTC has no city
    gClk.tz = 1; gClk.wx = 0; bool v6 = !wxActive() && !wxValid(); gClk.wx = 1;
    CHECK(v1 && v2 && v3 && v4 && v5 && v6, "weather: shown only for the right city, within 3 h, and when switched on");
    // 11e. schedule
    g_millis = 5000; wxSchedule(true, true); bool s1 = gWxNextMs == 5000 + WX_REFRESH_MS;
    wxSchedule(true, false); bool s2 = gWxNextMs == 5000 + WX_RETRY_MS;
    wxSchedule(false, false); bool s3 = gWxNextMs == 5000 + WX_REFRESH_MS;           // brand-new data: wait the full interval
    gWx.stamp = (uint32_t)fakeNow - 600; wxSchedule(false, false); bool s4 = gWxNextMs == 5000 + WX_REFRESH_MS - 600000UL;
    gWx.stamp = (uint32_t)fakeNow - 4000; wxSchedule(false, false); bool s5 = gWxNextMs == 5000;                    // older than 30 min: due now
    gWx.have = false; wxSchedule(false, false); bool s6 = gWxNextMs == 5000;
    g_millis = 5000; gWxNextMs = 5000; bool s7 = wxDue(); gWxNextMs = 5001; bool s8 = !wxDue(); gWxNextMs = 4999; bool s9 = wxDue();
    CHECK(s1 && s2 && s3 && s4 && s5 && s6 && s7 && s8 && s9, "weather: refresh every 30 min, retry after 10 min, due immediately when nothing fresh is cached");
    // 11f. refresh paths
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1}; gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; fakeNow = 1760000000L; fakeStep = 0;
    wifiForget(); g_wifiPassOK = true; g_httpMode = 0; g_wxBody = 0;
    CHECK(wxRefresh() == 0 && !gWx.have, "weather refresh: no saved Wi-Fi -> fails quietly");
    wifiSaveCreds("HomeNet", "secret123");
    run({}); long dr = g_draws;
    CHECK(wxRefresh() == 1 && gWx.have && gWx.t10 == 114 && gWx.code == 3 && gWx.day == 0 && gWx.hasHL && gWx.hi10 == 142 && gWx.lo10 == 81 && gWx.tz == 1 && gWx.stamp == (uint32_t)fakeNow,
          "weather refresh: connects, downloads, stores the data for the zone's city");
    CHECK(g_draws == dr && !g_wifiUp, "weather refresh: draws nothing while fetching and switches Wi-Fi off afterwards");
    g_wxBody = "garbage"; WxData keep = gWx; int r1 = wxRefresh(); g_wxBody = 0;
    CHECK(r1 == 0 && wxSame(gWx, keep), "weather refresh: unreadable answer keeps the previous data");
    g_httpMode = 3; int r2 = wxRefresh(); g_httpMode = 0;
    CHECK(r2 == 0 && wxSame(gWx, keep) && !g_wifiUp, "weather refresh: server error -> 0, previous data kept, Wi-Fi off");
    g_wifiPassOK = false; int r3 = wxRefresh(); g_wifiPassOK = true;
    CHECK(r3 == 0 && !g_wifiUp, "weather refresh: Wi-Fi unreachable -> 0");
    run({Ev{160, 120}}); int r4 = wxRefresh();
    CHECK(r4 == 2 && !g_wifiUp, "weather refresh: a touch while connecting aborts (2)");
    gClk.tz = 0; WxData none; CHECK(!wxFetch(0, none) && !wxFetch(-1, none) && !wxFetch(CLK_NTZ, none), "weather fetch: UTC / out-of-range zone has no city");
    gClk.tz = 1;
    // 11g. inside the running screensaver: data is fetched after the clock appears; a touch still ends it; failures do not stop the clock
    for (int style = 0; style < 2; style++) {
      gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; gWxNextMs = 0; fakeNow = 1760000000L; fakeStep = 37;
      gClk = ClockCfg{(uint8_t)style, 0, 1, 1, 2, 1, 1, 0, 1}; gBright = 4; long o0 = g_offscreen;
      run({Ev{160, 120}}, 400); clockRun(false);
      CHECK(gWx.have && gWx.tz == 1 && gBright == 4 && g_offscreen == o0 && qi == 1, style ? "clock run (analog): weather arrives, tap ends it" : "clock run (digital): weather arrives, tap ends it");
    }
    { gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; gWxNextMs = 0; g_httpMode = 3; fakeNow = 1760000000L; fakeStep = 37;
      run({Ev{160, 120}}, 400); clockRun(false); g_httpMode = 0;
      CHECK(!gWx.have && qi == 1, "clock run: weather server down -> clock keeps running, tap ends it"); }
    { gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; gWxNextMs = 0; gClk.wx = 0; fakeNow = 1760000000L; fakeStep = 37; long dc = g_draws; (void)dc;
      run({Ev{160, 120}}, 400); clockRun(false); gClk.wx = 1;
      CHECK(!gWx.have && qi == 1, "clock run: weather switched off -> never fetched"); }
    // auto-start path fetches too (silently, with the saved Wi-Fi)
    { gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; gWxNextMs = 0; fakeNow = 1760000000L; fakeStep = 37; gClkSynced = (uint32_t)fakeNow;
      run({Ev{160, 120}}, 400); clockRun(true);
      CHECK(gWx.have && qi == 1, "clock auto-start: fetches the weather with the saved Wi-Fi, tap ends it"); }
    // 11h. rendering: every icon x style x layout x unit x theme x temperature, only inside the screen, and clean repaint when data changes / goes stale
    { long o0 = g_offscreen, frames = 0;
      static const int codes[] = {0, 1, 2, 3, 45, 51, 61, 71, 80, 85, 95, 99, 123};
      static const int temps[] = {-400, -36, -4, 0, 99, 114, 250, 449};
      fakeNow = 1760000000L; fakeStep = 0;
      for (int light = 0; light < 2; light++) for (int style = 0; style < 2; style++) for (int df = 0; df < CLK_NDATE; df += 2)
        for (int secs = 0; secs < 2; secs++) for (int h12 = 0; h12 < 2; h12++) for (int unit = 1; unit <= 2; unit++) {
          gLight = light; gCur = -1;
          gClk = ClockCfg{(uint8_t)style, (uint8_t)h12, (uint8_t)secs, (uint8_t)df, 1, 2, 1, 0, (uint8_t)unit};
          time_t tt = (time_t)fakeNow; struct tm t; localtime_r(&tt, &t);
          clockDrawFull(t);
          for (size_t ci = 0; ci < sizeof codes / sizeof codes[0]; ci++) for (size_t ti = 0; ti < sizeof temps / sizeof temps[0]; ti++) {
            gWx = WxData{true, (ci & 1) != 0, (uint8_t)codes[ci], (uint8_t)(ti & 1), 1, (int16_t)temps[ti], (int16_t)(temps[ti] + 40), (int16_t)(temps[ti] - 70), (uint32_t)fakeNow};
            clockUpdate(t); frames++;
          }
          fakeNow += 3 * 3600L + 5; time_t t2 = (time_t)fakeNow; localtime_r(&t2, &t); clockUpdate(t);      // data is now stale: must be wiped without drawing outside
          CHECK(clkWxShown.on == 0, "weather draw: stale data is removed from the screen");
          fakeNow = 1760000000L;
        }
      gLight = false;
      CHECK(g_offscreen == o0, "weather draw: every icon, unit, layout and theme stays inside the screen");
      printf("      (%ld weather frames rendered)\n", frames);
    }
    // 11i. persistence: the old 8-byte record keeps working and gets weather on; a new record keeps its own value; garbage is clamped
    { uint8_t old8[8] = {1, 1, 0, 4, 5, 3, 9, 2};
      prefs.putBytes("clk", old8, 8); gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 0}; clockCfgLoad();
      CHECK(gClk.style == 1 && gClk.tz == 9 && gClk.idle == 2 && gClk.wx == 1, "weather setting: settings saved by the previous version still load (weather defaults to Celsius)");
      gClk.wx = 2; clockCfgSave(); gClk.wx = 0; clockCfgLoad(); CHECK(gClk.wx == 2, "weather setting: Fahrenheit survives save / load");
      gClk.wx = 0; clockCfgSave(); gClk.wx = 1; clockCfgLoad(); CHECK(gClk.wx == 0, "weather setting: OFF survives save / load");
      uint8_t bad9[9] = {0, 0, 1, 1, 0, 2, 1, 0, 77}; prefs.putBytes("clk", bad9, 9); clockCfgLoad(); CHECK(gClk.wx <= 2, "weather setting: corrupt value is clamped");
      gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1};
    }
    // 11j. set-up screen: the Weather row cycles OFF -> CELSIUS -> FAHRENHEIT, explains itself without a zone, all nine rows fit
    { char b[20]; gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 0};
      clkRowLabel(7, b, sizeof b); bool ok = !strcmp(b, "OFF");
      clkChange(7, 1); clkRowLabel(7, b, sizeof b); ok = ok && !strcmp(b, "CELSIUS");
      clkChange(7, 1); clkRowLabel(7, b, sizeof b); ok = ok && !strcmp(b, "FAHRENHEIT");
      clkChange(7, 1); ok = ok && gClk.wx == 0; clkChange(7, -1); ok = ok && gClk.wx == 2;
      gClk.tz = 0; clkRowLabel(7, b, sizeof b); ok = ok && !strcmp(b, "SET PLACE");
      CHECK(ok, "weather setting: row labels and cycling");
      gClk.place = 0; clkChange(8, 1); clkRowLabel(8, b, sizeof b); bool pl = !strcmp(b, "London") && gClk.place == 1;
      clkChange(8, -1); clkChange(8, -1); clkRowLabel(8, b, sizeof b); pl = pl && !strcmp(b, CLK_PLACES[CLK_NPLACE - 1].name) && gClk.place == CLK_NPLACE - 1;
      clkChange(8, 1); pl = pl && gClk.place == 0; clkRowLabel(8, b, sizeof b); pl = pl && !strcmp(b, "ZONE CITY");
      gClk.place = 2; gClk.tz = 0; clkRowLabel(7, b, sizeof b); pl = pl && !strcmp(b, "FAHRENHEIT");   // a chosen place works without a zone
      CHECK(pl, "weather setting: Place row cycles (and wraps) through the list, ZONE CITY first");
      gClk.place = 0;
      CHECK(25 + CLK_ROWS * CLK_ROW_H <= SCR_H, "clock set-up: all rows fit above the bottom edge");
      long o0 = g_offscreen; gClk.tz = 1; clkDrawScreen(); CHECK(g_offscreen == o0, "clock set-up: screen with ten rows draws inside the screen");
      gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1};
    }

    // 11k. places: Manchester & co. are fetched from their own coordinates, independent of the time zone
    { gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1, 0}; gWx = WxData{false, false, 0, 1, 0, 0, 0, 0, 0}; fakeNow = 1760000000L; fakeStep = 0;
      wifiSaveCreds("HomeNet", "secret123"); g_wifiPassOK = true; g_httpMode = 0; g_wxBody = 0;
      bool a = wxLoc() == 1 && wxRefresh() == 1 && strstr(g_lastUrl, "latitude=51.51") && gWx.tz == 1;
      gClk.place = 2;                                                           // Manchester
      bool b = wxLoc() == 102 && wxActive() && !wxValid();                      // cached London data must not be shown as Manchester
      b = b && wxRefresh() == 1 && strstr(g_lastUrl, "latitude=53.48") && strstr(g_lastUrl, "longitude=-2.24") && gWx.tz == 102 && wxValid();
      gClk.tz = 0; bool c = wxActive() && wxValid() && wxLoc() == 102;          // UTC zone but a place is chosen: still works
      gClk.place = 0; bool d = !wxActive() && !wxValid() && wxLoc() == 0;       // back to ZONE CITY with UTC: nothing
      gClk.tz = 13; bool e = wxLoc() == 13 && wxActive() && !wxValid();         // New York zone city
      gClk.place = 200; bool f = wxLoc() == 13;                                 // out-of-range place is ignored
      gClk.place = 0; gClk.tz = 1;
      bool g = !wxFetch(100, gWx) && !wxFetch(100 + CLK_NPLACE, gWx) && !wxFetch(255, gWx);
      CHECK(a && b && c && d && e && f && g, "weather places: Manchester uses its own coordinates, cache is per place, zone city is the fallback");
      bool all = true; for (int i = 1; i < CLK_NPLACE; i++) all = all && CLK_PLACES[i].lat > -90 && CLK_PLACES[i].lat < 90 && CLK_PLACES[i].lon > -180 && CLK_PLACES[i].lon < 180 && strlen(CLK_PLACES[i].name) <= 11;
      CHECK(all, "weather places: coordinates valid, names fit the button");
      // the previous version's 9-byte record keeps its settings and gets place = ZONE CITY
      uint8_t old9[9] = {1, 1, 0, 4, 5, 3, 9, 2, 2}; prefs.putBytes("clk", old9, 9); gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 0, 5}; clockCfgLoad();
      CHECK(gClk.tz == 9 && gClk.idle == 2 && gClk.wx == 2 && gClk.place == 0, "weather places: settings saved by the previous version still load");
      gClk.place = 3; clockCfgSave(); gClk.place = 0; clockCfgLoad(); CHECK(gClk.place == 3, "weather places: chosen place survives save / load");
      uint8_t bad10[10] = {0, 0, 1, 1, 0, 2, 1, 0, 1, 250}; prefs.putBytes("clk", bad10, 10); clockCfgLoad(); CHECK(gClk.place < CLK_NPLACE, "weather places: corrupt place is clamped");
      gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0, 1, 0};
    }
  }
  printf("\n%s (%d failures)\n", fails ? "CLOCK TEST FAILED" : "clock test passed", fails);
  return fails ? 1 : 0;
}
