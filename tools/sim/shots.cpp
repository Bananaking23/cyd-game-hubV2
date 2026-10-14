// Dumps framebuffer snapshots (menu pages + each game after some scripted play) as PPM files. Text is not rendered by the stub.
#include <stdio.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "sim_env.h"
#include "../../firmware/CydGameHub/CydGameHub.ino"
static void dump(const char* name) {
  FILE* f = fopen(name, "wb"); fprintf(f, "P6\n320 240\n255\n");
  for (int i = 0; i < 320 * 240; i++) { uint16_t c = g_fb[i]; unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static void tapAt(int sx, int sy, int frames) {
  g_simRawX = cal.ax0 + (int)((long)(sx - 20) * (cal.ax1 - cal.ax0) / 280);
  g_simRawY = cal.ay0 + (int)((long)(sy - 20) * (cal.ay1 - cal.ay0) / 200);
  g_simTouched = true; (void)frames;
}
int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : ".";
  char path[256];
  prefs.begin("sim"); settingsLoad(); cal = TouchCal{0, 400, 3700, 400, 3700};
  for (int i = 0; i < NUM_GAMES; i++) hiSet(i, (i * 37) % 200);
  for (int p = 0; p < pageCount(); p++) { gPage = p; drawMenu(); snprintf(path, sizeof path, "%s/menu%d.ppm", dir, p); dump(path); }
  drawSettings(); snprintf(path, sizeof path, "%s/settings.ppm", dir); dump(path);
  for (int g = 0; g < NUM_GAMES; g++) {
    srand(7 + g); gCur = g; gLowerBetter = GAMES[g].lowerBetter; GAMES[g].init();
    for (int f = 0; f < 260; f++) {
      g_millis += GAMES[g].frameMs;
      g_simTouched = false;
      int ph = (f / 12) % 6;
      if (f > 20 && (ph == 1 || ph == 4)) tapAt(40 + (f * 37) % 240, 60 + (f * 53) % 160, 1);
      pollTouch(); GAMES[g].tick();
    }
    snprintf(path, sizeof path, "%s/game%02d.ppm", dir, g); dump(path);
  }
  return 0;
}
