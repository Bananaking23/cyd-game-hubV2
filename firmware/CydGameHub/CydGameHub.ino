// =============================================================================
//  CYD Game Hub  -  24 touch games for the ESP32-2432S028R "Cheap Yellow Display" (2.8", resistive touch)
//
//  Menu: tap a tile to play, swipe or use < > to change page, tap the difficulty tag to cycle
//  EASY / NORMAL / HARD, SETTINGS for sound / brightness / difficulty / flip / invert / calibration /
//  scores / Wi-Fi + wireless updates / clock screensaver. In a game, tap MENU (top right) to leave.
//  Hold the BOOT button while powering on to force touch recalibration.
//
//  Build: arduino-cli with esp32:esp32 core 2.0.17 and libraries TFT_eSPI 2.5.43 + XPT2046_Touchscreen 1.4.
//  TFT_eSPI is configured entirely through build flags (see tools/build.py) - no User_Setup.h edits needed.
// =============================================================================
#include "games.h"
#include "version.h"
#include "net.h"
#include "clock.h"

// ------------------------------------------------------------------ menu layout
static const int TILE_COLS = 4, TILE_ROWS = 2, PER_PAGE = TILE_COLS * TILE_ROWS;
static const int TILE_W = 73, TILE_H = 88, TILE_GAP = 5, TILE_X0 = 6, TILE_Y0 = 26;
static const int BAR_Y = 213, BAR_H = 24;
static int gPage = 0;
static int gScreen = 0;            // 0 menu, 1 settings, 2 game
static uint32_t gLastTick = 0;
static uint32_t gLastActive = 0;     // last time the screen was touched (for the screensaver idle timer)

static int pageCount() { return (NUM_GAMES + PER_PAGE - 1) / PER_PAGE; }

static uint16_t diffColor() { return gDiff == 0 ? C_GREEN : (gDiff == 2 ? C_RED : C_YELLOW); }

static void drawTile(int idx) {
  int slot = idx - gPage * PER_PAGE;
  int col = slot % TILE_COLS, row = slot / TILE_COLS;
  int x = TILE_X0 + col * (TILE_W + TILE_GAP);
  int y = TILE_Y0 + row * (TILE_H + TILE_GAP);
  const GameDef& g = GAMES[idx];
  tft.fillRoundRect(x, y, TILE_W, TILE_H, 8, RGB565(22, 26, 38));
  tft.drawRoundRect(x, y, TILE_W, TILE_H, 8, g.color);
  tft.fillRect(x + 3, y + 3, TILE_W - 6, 3, g.color);
  g.icon(x + TILE_W / 2, y + 34);
  uint8_t font = (tft.textWidth(g.name, 2) <= TILE_W - 6) ? 2 : 1;   // long names use the small font
  text(g.name, x + TILE_W / 2, y + 64, font, C_WHITE, MC_DATUM);
  char b[24];
  int best = hiGet(idx);
  if (best > 0) snprintf(b, sizeof b, "Best %d", best);
  else snprintf(b, sizeof b, "-");
  text(b, x + TILE_W / 2, y + 80, 1, C_GREY, MC_DATUM);
}

static void drawDiffTag() {
  tft.fillRect(96, 1, 130, HUD_H - 3, HUD_BG);
  tft.fillRoundRect(106, 2, 110, HUD_H - 5, 6, RGB565(30, 36, 54));
  tft.drawRoundRect(106, 2, 110, HUD_H - 5, 6, diffColor());
  text(DIFF_NAMES[gDiff], 161, (HUD_H - 1) / 2, 2, diffColor(), MC_DATUM);
}

static void drawMenu() {
  gScreen = 0;
  gCur = -1;
  tft.fillScreen(thBg());
  tft.fillRect(0, 0, SCR_W, HUD_H, HUD_BG);
  tft.drawFastHLine(0, HUD_H - 1, SCR_W, RGB565(70, 80, 100));
  text("CYD HUB", 8, 3, 2, C_YELLOW);
  drawDiffTag();
  char b[16];
  snprintf(b, sizeof b, "%d/%d", gPage + 1, pageCount());
  text(b, SCR_W - 8, (HUD_H - 1) / 2, 2, C_LGREY, MR_DATUM);
  int first = gPage * PER_PAGE;
  int last = imin(first + PER_PAGE, NUM_GAMES);
  for (int i = first; i < last; i++) drawTile(i);
  // bottom bar
  button(5, BAR_Y, 70, BAR_H, "<", RGB565(40, 50, 80));
  button(100, BAR_Y, 120, BAR_H, "SETTINGS", RGB565(40, 50, 80), C_WHITE, 2);
  button(245, BAR_Y, 70, BAR_H, ">", RGB565(40, 50, 80));
}

static void changePage(int d) {
  int n = pageCount();
  gPage = (gPage + d + n) % n;
  drawMenu();
}

static void launch(int idx) {
  gCur = idx;
  gScreen = 2;
  gLowerBetter = GAMES[idx].lowerBetter;
  beep(900, 30);
  GAMES[idx].init();
  gLastTick = millis();
}

// ------------------------------------------------------------------ settings (a grid of tiles, like the game menu)
static const int SET_ITEMS = 12, SET_COLS = 4, SET_TW = 73, SET_TH = 66, SET_GAP = 5, SET_X0 = 6, SET_Y0 = 28;
enum { S_SOUND = 0, S_BRIGHT, S_DIFF, S_FLIP, S_INVERT, S_CALIB, S_SCORES, S_WIFI, S_CLOCK, S_CLOCKSET, S_ORDER, S_THEME };

static bool isSt7789() { return strcmp(HUB_VARIANT, "st7789") == 0; }

static void settingTileRect(int i, int* x, int* y) {
  *x = SET_X0 + (i % SET_COLS) * (SET_TW + SET_GAP);
  *y = SET_Y0 + (i / SET_COLS) * (SET_TH + SET_GAP);
}
static void settingLabel(int i, char* out, size_t n) {
  switch (i) {
    case S_SOUND:    snprintf(out, n, "%s", gSound ? "ON" : "OFF"); break;
    case S_BRIGHT:   snprintf(out, n, "%d / 4", (int)gBright); break;
    case S_DIFF:     snprintf(out, n, "%s", DIFF_NAMES[gDiff]); break;
    case S_FLIP:     snprintf(out, n, "%s", gFlip ? "FLIPPED" : "NORMAL"); break;
    case S_INVERT:   snprintf(out, n, "%s", gInvert ? "ON" : "OFF"); break;
    case S_CALIB:    snprintf(out, n, "START"); break;
    case S_SCORES:   snprintf(out, n, "CLEAR"); break;
    case S_WIFI:     snprintf(out, n, "OPEN"); break;
    case S_CLOCK:    snprintf(out, n, "START"); break;
    case S_CLOCKSET: snprintf(out, n, "SET UP"); break;
    case S_ORDER:    snprintf(out, n, "%s", isSt7789() ? (gBgr ? "BGR" : "RGB") : "-"); break;
    default:         snprintf(out, n, "%s", gLight ? "LIGHT" : "DARK"); break;
  }
}
static void drawSettingTile(int i) {
  static const char* const names[SET_ITEMS] = {"Sound", "Brightness", "Difficulty", "Rotation", "Invert", "Calibrate",
                                               "Scores", "Wi-Fi", "Clock", "Clock style", "Red/Blue", "Theme"};
  static const uint16_t accent[SET_ITEMS] = {C_GREEN, C_YELLOW, C_YELLOW, C_CYAN, C_MAGENTA, C_ORANGE, C_RED, C_BLUE, C_GOLD, C_PINK, C_TEAL, C_LGREY};
  int x, y;
  settingTileRect(i, &x, &y);
  uint16_t col = (i == S_DIFF) ? diffColor() : accent[i];
  tft.fillRoundRect(x, y, SET_TW, SET_TH, 8, RGB565(22, 26, 38));
  tft.drawRoundRect(x, y, SET_TW, SET_TH, 8, col);
  tft.fillRect(x + 3, y + 3, SET_TW - 6, 3, col);
  uint8_t font = (tft.textWidth(names[i], 2) <= SET_TW - 6) ? 2 : 1;
  text(names[i], x + SET_TW / 2, y + 22, font, C_WHITE, MC_DATUM);
  char b[16];
  settingLabel(i, b, sizeof b);
  uint16_t vc = (i == S_DIFF || i == S_SOUND || i == S_INVERT) ? col : C_LGREY;
  if (i == S_SOUND && !gSound) vc = C_GREY;
  if (i == S_INVERT && !gInvert) vc = C_GREY;
  text(b, x + SET_TW / 2, y + 46, 2, vc, MC_DATUM);
}
static void drawSettings() {
  gScreen = 1;
  hudBegin("Settings");
  for (int i = 0; i < SET_ITEMS; i++) drawSettingTile(i);
}

static void applyDisplaySettings() {
  tft.setRotation(gFlip ? 3 : 1);
  tft.invertDisplay(gInvert);
  if (isSt7789()) {   // same value TFT_eSPI writes for this rotation, plus our own red/blue order bit
    tft.writecommand(0x36);
    tft.writedata((uint8_t)((gFlip ? 0xA0 : 0x60) | (gBgr ? 0x08 : 0)));
  }
  applyBrightness();
}

static void doCalibration() {
  runCalibration();
  touch.down = false;
  gMiss = 0;
}

static void clearAllScores() {
  uint8_t keep = gDiff;
  for (uint8_t d = 0; d < 3; d++) {
    gDiff = d;
    for (int i = 0; i < NUM_GAMES; i++) hiSet(i, 0);
  }
  gDiff = keep;
}

static void settingsTap(int i) {
  switch (i) {
    case S_SOUND: gSound = !gSound; if (gSound) beep(1200, 40); break;
    case S_BRIGHT: gBright = (uint8_t)(gBright % 4 + 1); applyBrightness(); break;
    case S_DIFF: gDiff = (uint8_t)((gDiff + 1) % 3); break;
    case S_FLIP:
      gFlip = !gFlip;
      applyDisplaySettings();
      if (!loadCalibration()) doCalibration();
      settingsSave();
      drawSettings();
      return;
    case S_INVERT: gInvert = !gInvert; applyDisplaySettings(); break;
    case S_CALIB:
      doCalibration();
      drawSettings();
      return;
    case S_SCORES:
      if (netAsk("CLEAR SCORES", C_RED, "Delete every score?", "(all difficulties)", "DELETE", "CANCEL")) {
        clearAllScores();
        beep(500, 80);
      }
      drawSettings();
      return;
    case S_WIFI:
      netScreen();
      drawSettings();
      return;
    case S_CLOCK:
      beep(800, 20);
      touchClear();
      clockRun(false);
      drawSettings();
      return;
    case S_CLOCKSET:
      beep(800, 20);
      touchClear();
      clockCustomise();
      drawSettings();
      return;
    case S_ORDER:   // red/blue order, ST7789 build only
      if (!isSt7789()) { beep(300, 60); return; }
      gBgr = !gBgr;
      applyDisplaySettings();
      drawSettings();     // the whole screen is repainted so the new colours are visible straight away
      break;
    default:        // S_THEME
      gLight = !gLight;
      settingsSave();
      beep(1000, 25);
      drawSettings();
      return;
  }
  settingsSave();
  drawSettingTile(i);
}

// ------------------------------------------------------------------ main loop pieces
static void menuTick() {
  if (!touch.released) return;
  if (touch.swipe == 3) { changePage(1); return; }
  if (touch.swipe == 1) { changePage(-1); return; }
  if (!touch.tap) return;
  int px = touch.x, py = touch.y;
  if (inRect(px, py, 96, 0, 130, HUD_H + 2)) {          // difficulty tag: cycle EASY / NORMAL / HARD
    gDiff = (uint8_t)((gDiff + 1) % 3);
    settingsSave();
    beep(1000, 25);
    drawMenu();
    return;
  }
  if (inRect(px, py, 0, BAR_Y - 2, 90, BAR_H + 6)) { beep(800, 20); changePage(-1); return; }
  if (inRect(px, py, 230, BAR_Y - 2, 90, BAR_H + 6)) { beep(800, 20); changePage(1); return; }
  if (inRect(px, py, 90, BAR_Y - 2, 140, BAR_H + 6)) { beep(800, 20); drawSettings(); return; }
  for (int i = gPage * PER_PAGE; i < imin(gPage * PER_PAGE + PER_PAGE, NUM_GAMES); i++) {
    int slot = i - gPage * PER_PAGE;
    int x = TILE_X0 + (slot % TILE_COLS) * (TILE_W + TILE_GAP);
    int y = TILE_Y0 + (slot / TILE_COLS) * (TILE_H + TILE_GAP);
    if (inRect(px, py, x, y, TILE_W, TILE_H)) { launch(i); return; }
  }
}

static void settingsTickScreen() {
  if (!touch.tap) return;
  if (inRect(touch.x, touch.y, SCR_W - 56, 0, 56, HUD_H + 4)) { beep(800, 20); drawMenu(); return; }
  for (int i = 0; i < SET_ITEMS; i++) {
    int x, y;
    settingTileRect(i, &x, &y);
    if (inRect(touch.x, touch.y, x, y, SET_TW, SET_TH)) { settingsTap(i); return; }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  randomSeed(esp_random());
  prefs.begin("cydhub", false);
  settingsLoad();
  if (isSt7789()) {   // the 2-USB panel needs inverted colours and BGR order unless the user chose otherwise
    if (!prefs.isKey("inv")) gInvert = true;
    if (!prefs.isKey("bgr")) gBgr = true;
  }
  clockCfgLoad();
  autoUpdLoad();
  clockApplyTz();
  soundBegin();
  tft.init();
  backlightBegin();      // AFTER tft.init(): the driver must not own the backlight pin, or it would cancel the PWM
  applyDisplaySettings();
  tft.fillScreen(C_BLACK);
  text("CYD GAME HUB", SCR_W / 2, SCR_H / 2 - 12, 4, C_YELLOW, MC_DATUM);
  char vb[40];
  snprintf(vb, sizeof vb, "v%s", HUB_VERSION);
  text(vb, SCR_W / 2, SCR_H / 2 + 22, 2, C_GREY, MC_DATUM);
  touchBegin();
  delay(500);
  bool force = (digitalRead(0) == LOW);
  if (force || !loadCalibration()) doCalibration();
  drawMenu();
  if (!force && netAutoCheck()) drawMenu();      // pop-up only appears when a newer build exists
  gLastActive = millis();
}

void loop() {
  soundTick();
  uint32_t now = millis();
  uint32_t interval = (gScreen == 2 && gCur >= 0) ? GAMES[gCur].frameMs : 33;
  if (now - gLastTick < interval) {
    delay(1);
    return;
  }
  gLastTick = now;
  pollTouch();
  if (touch.down || touch.pressed) gLastActive = now;

  if (gScreen == 2) {
    // MENU button (top right) always leaves the running game
    if (touch.pressed && inRect(touch.x, touch.y, SCR_W - 56, 0, 56, HUD_H + 2)) {
      toneOut(0);
      gBeepEnd = 0;
      drawMenu();
      return;
    }
    GAMES[gCur].tick();
  } else if (gScreen == 1) {
    settingsTickScreen();
  } else {
    menuTick();
    if (gScreen == 0 && clockIdleDue(now - gLastActive)) {   // nobody touched the menu for a while: start the clock
      clockRun(true);
      drawMenu();
      gLastActive = gLastTick = millis();
    }
  }
}
