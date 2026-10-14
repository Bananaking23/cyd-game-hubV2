// Host-side stress test: runs every game's real init()/tick() against fake hardware with scripted touch input.
// Build+run:  tools/sim/run.sh
#include <stdio.h>
#include <string>
#include <time.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "sim_env.h"

#include "../../firmware/CydGameHub/CydGameHub.ino"

static void setTouchAt(int sx, int sy) {
  // inverse of rawToScreen() for the default calibration (swap = 0)
  g_simRawX = cal.ax0 + (int)((long)(sx - 20) * (cal.ax1 - cal.ax0) / 280);
  g_simRawY = cal.ay0 + (int)((long)(sy - 20) * (cal.ay1 - cal.ay0) / 200);
  g_simTouched = true;
}

struct Script {
  int kind;            // see names
  int phase, left, x0, y0, x1, y1, total;
};
static const char* KIND[] = {"idle", "taps", "drags", "swipes", "hold-spam", "long-press", "edge-poke"};
static const int NKIND = 7;

static void scriptStep(Script& s, int frame) {
  g_simTouched = false;
  switch (s.kind) {
    case 0: return;
    case 1:  // short taps
      if (s.left > 0) { setTouchAt(s.x0, s.y0); s.left--; }
      else if (rand() % 100 < 12) { s.left = 1 + rand() % 3; s.x0 = rand() % 320; s.y0 = 22 + rand() % 218; }
      return;
    case 2:  // continuous wandering drag with pauses
      if (s.left > 0) {
        s.x0 = clampi(s.x0 + (s.x1 - s.x0) / 4 + (rand() % 7 - 3), 0, 319);
        s.y0 = clampi(s.y0 + (s.y1 - s.y0) / 4 + (rand() % 7 - 3), 24, 239);
        setTouchAt(s.x0, s.y0); s.left--;
        if (rand() % 20 == 0) { s.x1 = rand() % 320; s.y1 = 24 + rand() % 216; }
      } else if (rand() % 100 < 4) { s.left = 20 + rand() % 120; s.x0 = rand() % 320; s.y0 = 24 + rand() % 216; s.x1 = rand() % 320; s.y1 = 24 + rand() % 216; }
      return;
    case 3:  // straight swipes
      if (s.left > 0) {
        int t = s.total - s.left;
        setTouchAt(s.x0 + (s.x1 - s.x0) * t / s.total, s.y0 + (s.y1 - s.y0) * t / s.total); s.left--;
      } else if (rand() % 100 < 8) { s.total = s.left = 4 + rand() % 6; s.x0 = 40 + rand() % 240; s.y0 = 50 + rand() % 160; s.x1 = s.x0 + (rand() % 161 - 80); s.y1 = s.y0 + (rand() % 161 - 80); s.x1 = clampi(s.x1, 0, 319); s.y1 = clampi(s.y1, 24, 239); }
      return;
    case 4:  // pressed almost all the time at a few fixed spots
      { static const int px[4] = {60, 160, 260, 160}; static const int py[4] = {120, 200, 120, 60};
        int k = (frame / 90) % 4; if (frame % 90 < 80) setTouchAt(px[k], py[k]); }
      return;
    case 5:  // long presses
      if (s.left > 0) { setTouchAt(s.x0, s.y0); s.left--; }
      else if (rand() % 100 < 3) { s.left = 10 + rand() % 25; s.x0 = rand() % 320; s.y0 = 30 + rand() % 200; }
      return;
    case 6:  // taps hugging the screen edges / corners
      if (s.left > 0) { setTouchAt(s.x0, s.y0); s.left--; }
      else if (rand() % 100 < 15) { s.left = 1 + rand() % 2; s.x0 = (rand() % 2) ? rand() % 6 : 314 + rand() % 6; s.y0 = (rand() % 2) ? 24 + rand() % 6 : 234 + rand() % 6; }
      return;
  }
}

int main(int argc, char** argv) {
  int frames = argc > 1 ? atoi(argv[1]) : 4000;
  int bad = 0;
  prefs.begin("sim");
  settingsLoad();
  cal = TouchCal{0, 400, 3700, 400, 3700};
  printf("%-12s %-5s %-10s %8s %9s %7s %6s %8s\n", "game", "diff", "scenario", "draws", "offscr", "best", "tones", "maxtick");
  double worstTick = 0; const char* worstName = ""; double worstInit = 0; const char* worstInitName = "";
  for (int g = 0; g < NUM_GAMES; g++) {
   for (int d = 0; d < 3; d++) {
    for (int k = 0; k < NKIND; k++) {
      gDiff = (uint8_t)d;
      srand(1234 + g * 31 + k);
      for (int i = 0; i < NUM_GAMES; i++) hiSet(i, 0);
      g_draws = g_offscreen = 0; g_toneCalls = 0;
      memset(g_fb, 0, sizeof g_fb);
      touch = TouchState{false, false, false, false, -1, 160, 120, 160, 120, 0, 0};
      gMiss = 0; g_simTouched = false;
      gCur = g; gLowerBetter = GAMES[g].lowerBetter;
      { struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); GAMES[g].init(); clock_gettime(CLOCK_MONOTONIC, &b);
        double ms = (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
        if (ms > worstInit) { worstInit = ms; worstInitName = GAMES[g].name; } }
      Script s = {k, 0, 0, 0, 0, 0, 0, 1};
      uint32_t t = g_millis;
      double maxTick = 0;
      for (int f = 0; f < frames; f++) {
        g_millis += GAMES[g].frameMs;
        soundTick();
        scriptStep(s, f);
        pollTouch();
        if (touch.pressed && inRect(touch.x, touch.y, SCR_W - 56, 0, 56, HUD_H + 2)) continue;  // hub would exit here
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        GAMES[g].tick();
        clock_gettime(CLOCK_MONOTONIC, &b);
        double ms = (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
        if (ms > maxTick) maxTick = ms;
      }
      (void)t;
      if (maxTick > worstTick) { worstTick = maxTick; worstName = GAMES[g].name; }
      printf("%-12s %-5s %-10s %8ld %9ld %7d %6d %7.2fms\n", GAMES[g].name, DIFF_NAMES[d], KIND[k], g_draws, g_offscreen, hiGet(g), g_toneCalls, maxTick);
      if (g_offscreen >= 1000) { printf("   ^^^ NEGATIVE-SIZE / NULL DRAW CALL\n"); bad++; }
    }
   }
  }
  gDiff = 1;
  printf("\nslowest init() on the host: %.2f ms (%s)\n", worstInit, worstInitName);
  printf("slowest single tick on the host: %.2f ms (%s) - the ESP32 is roughly 20-40x slower\n", worstTick, worstName);
  // menu + settings screens
  for (int p = 0; p < pageCount(); p++) { gPage = p; drawMenu(); }
  drawSettings();
  for (int r = 0; r < SET_ROWS; r++) drawSettingRow(r);
  printf("\nmenu/settings render OK; sanitizer-clean run complete. bad=%d\n", bad);
  return bad ? 1 : 0;
}
