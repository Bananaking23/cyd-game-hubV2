// Whack-a-mole - tap the moles as they pop out of the holes. 30 seconds.
#pragma once
#include "hub.h"

namespace Whack {

static const int HOLES = 9;
static const int ROUND_MS = 30000;
static const int FIELD_H = 208;                // field covers y = PF_Y .. PF_Y + 207 (= 229)
static const int ROW_PITCH = 69;
static const int BAR_X = 10, BAR_Y = 232, BAR_W = 300, BAR_H = 6;

static const uint16_t FIELD  = RGB565(40, 130, 50);
static const uint16_t RIM    = RGB565(95, 62, 30);
static const uint16_t PIT    = RGB565(22, 12, 6);
static const uint16_t BROWN  = RGB565(150, 96, 50);
static const uint16_t TAN    = RGB565(214, 172, 120);
static const uint16_t BAR_BG = RGB565(40, 44, 58);

static uint8_t mState[HOLES];                  // 0 empty, 1 mole up, 2 "pow" flash
static uint32_t mUntil[HOLES];
static int score, state;                       // state: 0 ready, 1 playing, 2 over
static int lastScore, lastSec, barW;
static uint16_t barCol;
static uint32_t tStart, tEnd, nextSpawn;
// per-run difficulty knobs (set in reset(); normal = the original numbers)
static int holdStartMs, holdMinMs, spawnStartMs, spawnMinMs, moleStepSec1, moleStepSec2, moleMaxUpLate;

static int holeX(int i) { return 53 + (i % 3) * 107; }
static int holeY(int i) { return PF_Y + 50 + (i / 3) * ROW_PITCH; }

static void drawHole(int i) {                  // also erases anything drawn above the hole
  int x = holeX(i), y = holeY(i);
  tft.fillRect(x - 36, y - 34, 72, 50, FIELD);
  tft.fillEllipse(x, y, 34, 14, RIM);
  tft.fillEllipse(x, y, 30, 11, PIT);
}

static void drawMole(int i) {
  int x = holeX(i), y = holeY(i);
  drawHole(i);
  tft.fillCircle(x - 17, y - 27, 5, BROWN);                 // ears
  tft.fillCircle(x + 17, y - 27, 5, BROWN);
  tft.fillCircle(x, y - 10, 21, BROWN);                     // head
  tft.fillCircle(x - 8, y - 18, 4, C_WHITE);                // eyes
  tft.fillCircle(x + 8, y - 18, 4, C_WHITE);
  tft.fillCircle(x - 8, y - 17, 2, C_BLACK);
  tft.fillCircle(x + 8, y - 17, 2, C_BLACK);
  tft.fillEllipse(x, y - 5, 10, 7, TAN);                    // muzzle
  tft.fillCircle(x, y - 8, 3, RGB565(220, 100, 130));       // nose
  tft.fillRect(x - 3, y - 1, 3, 4, C_WHITE);                // buck teeth
  tft.fillRect(x, y - 1, 3, 4, C_WHITE);
}

static void drawPow(int i) {
  static const int8_t rx[8] = {26, 18, 0, -18, -26, -18, 0, 18};
  static const int8_t ry[8] = {0, 18, 26, 18, 0, -18, -26, -18};
  int x = holeX(i), y = holeY(i) - 8;
  drawHole(i);
  for (int k = 0; k < 8; k++) {
    tft.drawLine(x, y, x + rx[k], y + ry[k], C_YELLOW);
    tft.drawLine(x + 1, y, x + rx[k] + 1, y + ry[k], C_YELLOW);
  }
  tft.fillCircle(x, y, 15, C_ORANGE);
  text("POW", x, y, 2, C_WHITE, MC_DATUM);
}

static void drawAllHoles() {
  tft.fillRect(0, PF_Y, SCR_W, FIELD_H, FIELD);
  for (int i = 0; i < HOLES; i++) drawHole(i);
}

static void updateHud(int sec) {
  if (score == lastScore && sec == lastSec) return;
  lastScore = score;
  lastSec = sec;
  char s[24];
  snprintf(s, sizeof s, "Score %d  %ds", score, sec);
  hudText(s);
}

static void updateBar(int remMs, bool force) {
  int w = clampi((int)((long)remMs * BAR_W / ROUND_MS), 0, BAR_W);
  uint16_t col = remMs > 10000 ? C_GREEN : (remMs > 5000 ? C_YELLOW : C_RED);
  if (force || col != barCol) {
    tft.fillRect(BAR_X, BAR_Y, w, BAR_H, col);
    tft.fillRect(BAR_X + w, BAR_Y, BAR_W - w, BAR_H, BAR_BG);
    barCol = col;
    barW = w;
    return;
  }
  if (w < barW) tft.fillRect(BAR_X + w, BAR_Y, barW - w, BAR_H, BAR_BG);
  barW = w;
}

static void reset() {
  holdStartMs   = byDiff(1700, 1250, 1000);   // how long a mole stays up at the start (shrinks 28 ms per second)
  holdMinMs     = byDiff(800, 450, 380);
  spawnStartMs  = byDiff(900, 700, 550);      // delay between spawns at the start (shrinks 16 ms per second)
  spawnMinMs    = byDiff(350, 200, 160);
  moleStepSec1  = byDiff(12, 8, 5);           // from this second on, 2 moles may be up at once
  moleStepSec2  = byDiff(25, 18, 12);         // from this second on, the late maximum applies
  moleMaxUpLate = byDiff(2, 3, 4);
  hudBegin("Whack");
  pfClear(C_BLACK);
  drawAllHoles();
  memset(mState, 0, sizeof mState);
  score = 0;
  state = 0;
  lastScore = -1;
  lastSec = -1;
  barCol = 0;
  barW = BAR_W;
  updateBar(ROUND_MS, true);
  hudText("Tap to start", C_WHITE);
  int bx = SCR_W / 2 - 100, by = PF_Y + 84;
  tft.fillRoundRect(bx, by, 200, 54, 10, RGB565(14, 16, 28));
  tft.drawRoundRect(bx, by, 200, 54, 10, C_ORANGE);
  text("WHACK-A-MOLE", SCR_W / 2, by + 17, 2, C_YELLOW, MC_DATUM);
  text("Tap to start", SCR_W / 2, by + 38, 2, C_WHITE, MC_DATUM);
}

static void init() { reset(); }

static int upCount() {
  int n = 0;
  for (int i = 0; i < HOLES; i++) n += (mState[i] == 1);
  return n;
}

static void spawnMole(uint32_t now, int sec) {
  int empty = 0;
  for (int i = 0; i < HOLES; i++) if (mState[i] == 0) empty++;
  if (empty == 0) return;
  int pick = rnd(empty);
  for (int i = 0; i < HOLES; i++) {
    if (mState[i] != 0) continue;
    if (pick == 0) {
      mState[i] = 1;
      mUntil[i] = now + (uint32_t)(imax(holdMinMs, holdStartMs - sec * 28) + rnd(250));
      drawMole(i);
      break;
    }
    pick--;
  }
  nextSpawn = now + (uint32_t)(imax(spawnMinMs, spawnStartMs - sec * 16) + rnd(250));
}

static void tick() {
  uint32_t now = millis();

  if (state == 0) {
    if (touch.pressed && touch.y >= PF_Y) {
      drawAllHoles();
      tStart = now;
      nextSpawn = now + 500;
      state = 1;
      updateBar(ROUND_MS, true);
      updateHud(ROUND_MS / 1000);
    }
    return;
  }
  if (state == 2) {
    if (touch.pressed && now - tEnd > 1200) reset();   // lockout: players are still tapping frantically
    return;
  }

  uint32_t el = now - tStart;
  if (el >= (uint32_t)ROUND_MS) {
    state = 2;
    tEnd = now;
    updateBar(0, true);
    hudScore(score);
    beep(300, 300);
    overlayGameOver(score);
    return;
  }
  int sec = (int)(el / 1000);

  // input: the whole cell around a hole is the target
  if (touch.pressed && touch.y >= PF_Y && touch.y < PF_Y + FIELD_H) {
    int col = clampi(touch.x * 3 / SCR_W, 0, 2);
    int row = clampi((touch.y - PF_Y) / ROW_PITCH, 0, 2);
    int i = row * 3 + col;
    if (mState[i] == 1) {
      mState[i] = 2;
      mUntil[i] = now + 170;
      score++;
      drawPow(i);
      beep(1000, 45);
    }
  }

  // expire moles / pow flashes
  for (int i = 0; i < HOLES; i++) {
    if (mState[i] != 0 && (int32_t)(now - mUntil[i]) >= 0) {
      mState[i] = 0;
      drawHole(i);
    }
  }

  // spawn
  int maxUp = sec < moleStepSec1 ? 1 : (sec < moleStepSec2 ? 2 : moleMaxUpLate);
  if ((int32_t)(now - nextSpawn) >= 0 && upCount() < maxUp) spawnMole(now, sec);

  int rem = ROUND_MS - (int)el;
  updateHud((rem + 999) / 1000);
  updateBar(rem, false);
}

static void icon(int cx, int cy) {
  tft.fillEllipse(cx, cy + 12, 17, 7, C_GREEN);
  tft.fillEllipse(cx, cy + 11, 14, 5, PIT);
  tft.fillCircle(cx - 9, cy - 9, 3, BROWN);
  tft.fillCircle(cx + 9, cy - 9, 3, BROWN);
  tft.fillCircle(cx, cy, 12, BROWN);
  tft.fillCircle(cx - 4, cy - 3, 2, C_WHITE);
  tft.fillCircle(cx + 4, cy - 3, 2, C_WHITE);
  tft.fillEllipse(cx, cy + 3, 5, 3, TAN);
  tft.fillCircle(cx, cy + 1, 2, RGB565(220, 100, 130));
}

}  // namespace Whack
