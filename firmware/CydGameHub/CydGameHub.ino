// =============================================================================
//  CYD Game Hub  -  19 touch games for the ESP32-2432S028R "Cheap Yellow Display" (2.8", resistive touch)
//
//  Menu: tap a tile to play, swipe or use < > to change page, tap the difficulty tag to cycle
//  EASY / NORMAL / HARD, SETTINGS for sound / brightness / difficulty / flip / invert / calibration /
//  scores / Wi-Fi + wireless updates. In a game, tap MENU (top right) to leave.
//  Hold the BOOT button while powering on to force touch recalibration.
//
//  Build: arduino-cli with esp32:esp32 core 2.0.17 and libraries TFT_eSPI 2.5.43 + XPT2046_Touchscreen 1.4.
//  TFT_eSPI is configured entirely through build flags (see tools/build.py) - no User_Setup.h edits needed.
// =============================================================================
#include "games.h"
#include "version.h"
#include "net.h"

// ------------------------------------------------------------------ menu layout
static const int TILE_COLS = 4, TILE_ROWS = 2, PER_PAGE = TILE_COLS * TILE_ROWS;
static const int TILE_W = 73, TILE_H = 88, TILE_GAP = 5, TILE_X0 = 6, TILE_Y0 = 26;
static const int BAR_Y = 213, BAR_H = 24;
static int gPage = 0;
static int gScreen = 0;            // 0 menu, 1 settings, 2 game
static uint32_t gLastTick = 0;

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
  tft.fillScreen(C_BLACK);
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

// ------------------------------------------------------------------ settings
static const int SET_ROWS = 8, SET_Y0 = 25, SET_RH = 26;

static void settingLabel(int row, char* out, size_t n) {
  switch (row) {
    case 0: snprintf(out, n, "%s", gSound ? "ON" : "OFF"); break;
    case 1: snprintf(out, n, "%d / 4", (int)gBright); break;
    case 2: snprintf(out, n, "%s", DIFF_NAMES[gDiff]); break;
    case 3: snprintf(out, n, "%s", gFlip ? "FLIPPED" : "NORMAL"); break;
    case 4: snprintf(out, n, "%s", gInvert ? "ON" : "OFF"); break;
    case 5: snprintf(out, n, "START"); break;
    case 6: snprintf(out, n, "RESET"); break;
    default: snprintf(out, n, "OPEN"); break;
  }
}
static void drawSettingRow(int row) {
  static const char* names[SET_ROWS] = {"Sound", "Brightness", "Difficulty", "Screen rotation", "Invert colours",
                                        "Calibrate touch", "Clear high scores", "Wi-Fi & updates"};
  int y = SET_Y0 + row * SET_RH;
  tft.fillRect(0, y, SCR_W, SET_RH, C_BLACK);
  tft.fillRoundRect(6, y, SCR_W - 12, SET_RH - 3, 6, RGB565(22, 26, 38));
  text(names[row], 16, y + (SET_RH - 3) / 2, 2, C_WHITE, ML_DATUM);
  char b[16];
  settingLabel(row, b, sizeof b);
  uint16_t col = RGB565(40, 90, 150);
  if (row == 2) col = gDiff == 0 ? RGB565(30, 120, 60) : (gDiff == 2 ? RGB565(160, 40, 50) : RGB565(150, 120, 20));
  if (row == 6) col = RGB565(150, 40, 50);
  if (row == 7) col = RGB565(30, 120, 80);
  button(SCR_W - 112, y + 2, 98, SET_RH - 7, b, col);
}
static void drawSettings() {
  gScreen = 1;
  hudBegin("Settings");
  for (int i = 0; i < SET_ROWS; i++) drawSettingRow(i);
}

static void applyDisplaySettings() {
  tft.setRotation(gFlip ? 3 : 1);
  tft.invertDisplay(gInvert);
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

static void settingsTap(int row) {
  switch (row) {
    case 0: gSound = !gSound; if (gSound) beep(1200, 40); break;
    case 1: gBright = (uint8_t)(gBright % 4 + 1); applyBrightness(); break;
    case 2: gDiff = (uint8_t)((gDiff + 1) % 3); break;
    case 3:
      gFlip = !gFlip;
      applyDisplaySettings();
      if (!loadCalibration()) doCalibration();
      drawSettings();
      settingsSave();
      return;
    case 4: gInvert = !gInvert; applyDisplaySettings(); break;
    case 5:
      doCalibration();
      drawSettings();
      return;
    case 6:
      clearAllScores();
      beep(500, 80);
      break;
    default:
      netScreen();
      drawSettings();
      return;
  }
  settingsSave();
  drawSettingRow(row);
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
  for (int r = 0; r < SET_ROWS; r++) {
    int y = SET_Y0 + r * SET_RH;
    if (inRect(touch.x, touch.y, 6, y, SCR_W - 12, SET_RH - 1)) { settingsTap(r); return; }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  randomSeed(esp_random());
  prefs.begin("cydhub", false);
  settingsLoad();
  backlightBegin();
  soundBegin();
  tft.init();
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
  }
}
