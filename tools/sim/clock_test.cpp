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
    for (int style = 0; style < CLK_NSTYLE; style++)
      for (int h12 = 0; h12 < 2; h12++)
        for (int secs = 0; secs < 2; secs++)
          for (int df = 0; df < CLK_NDATE; df++)
            for (int col = 0; col < CLK_NCOL; col += 3)
              for (int tz = 0; tz < CLK_NTZ; tz += 2) {
                gClk = ClockCfg{(uint8_t)style, (uint8_t)h12, (uint8_t)secs, (uint8_t)df, (uint8_t)col, 2, (uint8_t)tz, 0};
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
    CHECK(g_offscreen == offs0, "clock: every style/format/zone draws only inside the screen");
    printf("      (%d frames rendered)\n", shown);
  }
  // 2. layout: digits never overlap each other or the date, in every digital configuration
  { bool ok = true;
    for (int h12 = 0; h12 < 2; h12++) for (int secs = 0; secs < 2; secs++) for (int df = 0; df < CLK_NDATE; df++) {
      gClk.style = 0; gClk.h12 = h12; gClk.secs = secs; gClk.dateFmt = df;
      ClkLayout L = clkLayout();
      int right = secs ? L.sx + 26 + CLK_SW : (h12 ? L.apx + 36 : L.x0 + 228);
      if (L.x0 < 0 || right > SCR_W) ok = false;
      if (L.y0 + CLK_BH > SCR_H) ok = false;
      if (df && L.dateY - 16 < L.y0 + CLK_BH) ok = false;
      if (df && L.dateY + 16 > SCR_H) ok = false;
    }
    CHECK(ok, "clock: digital layouts fit the 320x240 screen with date below the digits");
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
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{280, 25 + r * 26 + 12});   // forward
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{40, 25 + r * 26 + 12});    // backward
    for (int r = 0; r < CLK_ROWS; r++) v.push_back(Ev{280, 25 + r * 26 + 12});   // forward again
    v.push_back(menu);
    run(v); clockCustomise();
    ClockCfg a = gClk;
    CHECK(a.style == 1 && a.h12 == 1 && a.secs == 0 && a.dateFmt == 2 && a.color == 1 && a.bright == 3 && a.tz == 2 && a.idle == 1,
          "clock set-up: every row changes forward and back; result as expected");
    gClk = ClockCfg{0, 0, 1, 1, 0, 2, 1, 0}; clockCfgLoad();
    CHECK(gClk.style == a.style && gClk.tz == a.tz && gClk.idle == a.idle, "clock set-up: saved on leaving");
    // wrap-around: cycle the time zone more than once
    std::vector<Ev> w; for (int i = 0; i < CLK_NTZ * 2 + 3; i++) w.push_back(Ev{280, 25 + 6 * 26 + 12}); w.push_back(menu);
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
  printf("\n%s (%d failures)\n", fails ? "CLOCK TEST FAILED" : "clock test passed", fails);
  return fails ? 1 : 0;
}
