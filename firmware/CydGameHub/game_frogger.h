// Frogger - hop across the road and the river to the top. Tap (or swipe) in the direction you want to hop.
// Cars are deadly, water is deadly, logs carry you. Three lives; every crossing scores one and the traffic speeds up.
#pragma once
#include "hub.h"

namespace Frogger {

static const int RH = 24, ROWS = 9, FW = 20, MAXO = 5;
static const int Y0 = PF_Y + 1;                         // top of row 0; the nine rows span y 23..238
static const uint16_t GRASS = RGB565(34, 120, 44), GRASS_D = RGB565(22, 84, 34);
static const uint16_t WATER = RGB565(18, 58, 150), ROAD = RGB565(52, 54, 62);
static const uint16_t CAR_COL[6] = {C_RED, C_YELLOW, RGB565(70, 150, 255), C_ORANGE, C_MAGENTA, C_WHITE};

struct Lane {
  int8_t dir;          // +1 right, -1 left
  uint8_t kind;        // 0 none (grass), 1 cars, 2 logs
  int16_t w, n, s, period;
  int sp, acc, dxNow;  // speed in 1/4 px per tick, fraction accumulator, pixels moved this tick
  int16_t x[MAXO];     // left edge of each object, in [-w, period - w)
};
static Lane ln[ROWS];
static int lives, score, level, frow, fx, state;       // state: 0 playing, 1 dying, 2 game over
static bool hintOn;
static uint32_t tDie;

static int rowY(int r) { return Y0 + r * RH; }
static uint16_t laneBg(int r) {
  if (r == 0) return GRASS_D;
  if (r >= 1 && r <= 3) return WATER;
  if (r >= 5 && r <= 7) return ROAD;
  return GRASS;
}
static void fillC(int x, int y, int w, int h, uint16_t c) {
  int x0 = imax(x, 0), y0 = imax(y, PF_Y), x1 = imin(x + w, SCR_W), y1 = imin(y + h, SCR_H);
  if (x1 > x0 && y1 > y0) tft.fillRect(x0, y0, x1 - x0, y1 - y0, c);
}

static void drawObj(int r, int k) {
  const Lane& l = ln[r];
  int x = l.x[k], y = rowY(r), w = l.w;
  if (l.kind == 1) {
    uint16_t c = CAR_COL[(r * 2 + k) % 6];
    fillC(x, y + 4, w, RH - 8, c);
    fillC(x + 3, y + 2, w - 6, 3, c);
    int fxn = (l.dir > 0) ? x + w - 9 : x + 3;                       // windscreen at the front
    fillC(fxn, y + 6, 6, RH - 12, RGB565(170, 220, 255));
    fillC(x + 4, y + RH - 5, 6, 3, C_BLACK);                         // wheels
    fillC(x + w - 10, y + RH - 5, 6, 3, C_BLACK);
    fillC(x + 4, y + 2, 6, 2, C_BLACK);
    fillC(x + w - 10, y + 2, 6, 2, C_BLACK);
  } else if (l.kind == 2) {
    fillC(x, y + 3, w, RH - 6, RGB565(120, 78, 40));
    fillC(x, y + 3, w, 3, RGB565(150, 100, 56));
    for (int b = 12; b < w - 6; b += 18) fillC(x + b, y + 6, 2, RH - 12, RGB565(86, 54, 28));
  }
}

static void repaint(int x0, int y0, int x1, int y1) {          // lane background + objects inside a region
  for (int r = 0; r < ROWS; r++) {
    int ry0 = rowY(r), ry1 = ry0 + RH;
    if (ry1 <= y0 || ry0 >= y1) continue;
    fillC(x0, imax(ry0, y0), x1 - x0, imin(ry1, y1) - imax(ry0, y0), laneBg(r));
    const Lane& l = ln[r];
    if (!l.kind) continue;
    for (int k = 0; k < l.n; k++)
      if (l.x[k] < x1 && l.x[k] + l.w > x0) drawObj(r, k);
  }
}

static void setupLane(int r, int dir, int kind, int w, int n, int baseSp, int extraGap) {
  Lane& l = ln[r];
  l.dir = (int8_t)dir;
  l.kind = (uint8_t)kind;
  l.w = (int16_t)w;
  l.n = (int16_t)n;
  l.s = (int16_t)((SCR_W + w + extraGap) / n);
  l.period = (int16_t)(l.s * n);
  l.sp = baseSp * byDiff(8, 10, 13) / 10 + imin(level, 8);
  l.acc = 0;
  l.dxNow = 0;
  int off = rnd(l.period);
  for (int k = 0; k < n; k++) l.x[k] = (int16_t)(((k * l.s + off) % l.period) - w);
}
static void setupLanes() {
  memset(ln, 0, sizeof ln);
  int cars = byDiff(2, 3, 3), logAdj = byDiff(-8, 0, 12);
  setupLane(1, -1, 2, 72 - logAdj, 3, 6, 30);
  setupLane(2, 1, 2, 56 - logAdj, 3, 8, 30);
  setupLane(3, -1, 2, 88 - logAdj, 3, 5, 30);
  setupLane(5, 1, 1, 28, imin(cars + (gDiff == 2 ? 1 : 0), 4), 7, 40);
  setupLane(6, -1, 1, 44, cars, 10, 40);
  setupLane(7, 1, 1, 28, cars, 6, 40);
}

static void hud() {
  char b[32];
  snprintf(b, sizeof b, "Score %d  Lives %d", score, lives);
  hudText(b);
}
static void drawBoard() {
  for (int r = 0; r < ROWS; r++) fillC(0, rowY(r), SCR_W, RH, laneBg(r));
  for (int x = 6; x < SCR_W; x += 64) fillC(x, rowY(0) + 8, 30, 8, GRASS);            // goal "hedges"
  for (int r = 5; r <= 7; r++)
    if (r < 7) for (int x = 0; x < SCR_W; x += 24) fillC(x, rowY(r) + RH - 1, 12, 1, RGB565(120, 120, 90));
  for (int r = 1; r <= 7; r++)
    for (int k = 0; k < ln[r].n; k++) drawObj(r, k);
}

static void drawFrog(bool dead) {
  int x = fx, y = rowY(frow) + (RH - FW) / 2;
  if (dead) {
    fillC(x + 2, y + 2, FW - 4, FW - 4, C_RED);
    for (int i = 0; i < FW - 6; i++) { fillC(x + 3 + i, y + 3 + i, 2, 2, C_WHITE); fillC(x + FW - 5 - i, y + 3 + i, 2, 2, C_WHITE); }
    return;
  }
  uint16_t g = RGB565(110, 235, 70), d = RGB565(50, 150, 40);
  fillC(x + 3, y + 4, FW - 6, FW - 7, g);
  fillC(x + 1, y + 2, 4, 6, g);  fillC(x + FW - 5, y + 2, 4, 6, g);            // front legs
  fillC(x, y + FW - 7, 5, 6, d); fillC(x + FW - 5, y + FW - 7, 5, 6, d);       // back legs
  fillC(x + 4, y + 1, 4, 4, C_WHITE); fillC(x + FW - 8, y + 1, 4, 4, C_WHITE); // eyes
  fillC(x + 5, y + 2, 2, 2, C_BLACK); fillC(x + FW - 7, y + 2, 2, 2, C_BLACK);
}

static void startFrog() {
  frow = ROWS - 1;
  fx = SCR_W / 2 - FW / 2;
}

static void reset() {
  hudBegin("Frogger", GRASS);
  lives = 3;
  score = 0;
  level = 0;
  state = 0;
  setupLanes();
  startFrog();
  drawBoard();
  drawFrog(false);
  hud();
  hintOn = true;
  text("Tap towards where the frog should hop", SCR_W / 2, rowY(8) + 12, 1, C_WHITE, MC_DATUM);
}

static void killFrog() {
  state = 1;
  tDie = millis();
  lives--;
  beep(150, 250);
  hud();
}

// Collision / riding for the frog's current row; moves the frog with its log.
static void checkFrog() {
  int cx = fx + FW / 2;
  if (frow >= 1 && frow <= 3) {
    const Lane& l = ln[frow];
    bool onLog = false;
    for (int k = 0; k < l.n; k++)
      if (cx >= l.x[k] + 2 && cx < l.x[k] + l.w - 2) onLog = true;
    if (!onLog || cx < 0 || cx >= SCR_W) killFrog();
  } else if (frow >= 5 && frow <= 7) {
    const Lane& l = ln[frow];
    for (int k = 0; k < l.n; k++)
      if (fx + 3 < l.x[k] + l.w - 2 && fx + FW - 3 > l.x[k] + 2) { killFrog(); break; }
  }
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && millis() - tDie >= 500) reset();
    return;
  }
  int oldX = fx, oldRow = frow;
  // lanes
  for (int r = 1; r <= 7; r++) {
    Lane& l = ln[r];
    l.dxNow = 0;
    if (!l.kind) continue;
    l.acc += l.sp;
    int dx = l.acc >> 2;
    l.acc &= 3;
    if (dx == 0) continue;
    l.dxNow = dx;
    int y = rowY(r);
    for (int k = 0; k < l.n; k++) {
      int ox = l.x[k];
      if (l.dir > 0) fillC(ox, y, dx, RH, laneBg(r)); else fillC(ox + l.w - dx, y, dx, RH, laneBg(r));
      int nx = ox + l.dir * dx;
      if (l.dir > 0 && nx >= l.period - l.w) nx -= l.period;
      if (l.dir < 0 && nx < -l.w) nx += l.period;
      l.x[k] = (int16_t)nx;
      drawObj(r, k);
    }
  }
  if (state == 0) {
    if (frow >= 1 && frow <= 3) fx += ln[frow].dir * ln[frow].dxNow;       // the log carries the frog
    int dir = -1;
    if (touch.swipe >= 0) {
      dir = touch.swipe;
    } else if (touch.tap) {
      int dx = touch.x - (fx + FW / 2), dy = touch.y - (rowY(frow) + RH / 2);
      if (iabs(dx) > iabs(dy)) dir = dx > 0 ? 1 : 3; else dir = dy > 0 ? 2 : 0;
    }
    if (dir == 0 && frow > 0) frow--;
    else if (dir == 2 && frow < ROWS - 1) frow++;
    else if (dir == 1) fx = imin(fx + RH, SCR_W - FW);
    else if (dir == 3) fx = imax(fx - RH, 0);
    if (dir >= 0) {
      beep(900, 10);
      if (hintOn) { hintOn = false; fillC(0, rowY(ROWS - 1), SCR_W, RH, laneBg(ROWS - 1)); }
    }
    if (frow == 0) {                                                        // made it across
      score++;
      level++;
      beep(1400, 80);
      if (level <= 8) for (int r = 1; r <= 7; r++) if (ln[r].kind) ln[r].sp += 1;
      startFrog();
      hud();
    } else {
      checkFrog();
    }
  } else if (state == 1 && millis() - tDie >= 700) {
    if (lives <= 0) {
      state = 2;
      tDie = millis();
      repaint(imin(oldX, fx) - 2, rowY(oldRow) - 1, imax(oldX, fx) + FW + 2, rowY(oldRow) + RH + 1);
      overlayGameOver(score);
      return;
    }
    startFrog();
    state = 0;
  }
  // repaint where the frog was and is, then draw it on top
  int x0 = imin(oldX, fx) - 2, x1 = imax(oldX, fx) + FW + 2;
  int r0 = imin(oldRow, frow), r1 = imax(oldRow, frow);
  repaint(x0, rowY(r0), x1, rowY(r1) + RH);
  drawFrog(state == 1);
}

static void icon(int cx, int cy) {
  tft.fillRect(cx - 18, cy - 18, 36, 11, WATER);
  tft.fillRect(cx - 12, cy - 16, 20, 7, RGB565(120, 78, 40));
  tft.fillRect(cx - 18, cy + 9, 36, 10, ROAD);
  tft.fillRect(cx + 2, cy + 11, 14, 6, C_RED);
  tft.fillRect(cx - 18, cy - 7, 36, 16, GRASS);
  tft.fillRect(cx - 6, cy - 5, 12, 11, RGB565(110, 235, 70));
  tft.fillRect(cx - 5, cy - 6, 3, 3, C_WHITE);
  tft.fillRect(cx + 2, cy - 6, 3, 3, C_WHITE);
}

}  // namespace Frogger
