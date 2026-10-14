// Snake - touch anywhere: the snake turns towards your finger (relative to its head).
#pragma once
#include "hub.h"

namespace Snake {

static const int CS = 10, COLS = 32, ROWS = 21;
static const int OX = 0, OY = PF_Y + 4;
static const int MAXLEN = COLS * ROWS;

static uint8_t bx[MAXLEN], by[MAXLEN];   // ring buffer of body cells
static uint8_t occ[ROWS][COLS];          // 1 where the snake is
static int head, len, dir, nextDir, fx, fy, score, state;  // state: 0 ready, 1 playing, 2 dead
static uint32_t lastStep;
static int stepStartMs, stepFloorMs, stepRampMs;   // difficulty: first step interval, fastest interval, ms saved per point
static const int DX[4] = {0, 1, 0, -1};
static const int DY[4] = {-1, 0, 1, 0};

static void drawCell(int x, int y, uint16_t c) {
  tft.fillRoundRect(OX + x * CS + 1, OY + y * CS + 1, CS - 2, CS - 2, 2, c);
}
static void eraseCell(int x, int y) { tft.fillRect(OX + x * CS, OY + y * CS, CS, CS, C_BLACK); }
static void drawFood() { tft.fillCircle(OX + fx * CS + CS / 2, OY + fy * CS + CS / 2, CS / 2 - 1, C_RED); }

static void placeFood() {
  do {
    fx = rnd(COLS);
    fy = rnd(ROWS);
  } while (occ[fy][fx]);
  drawFood();
}

static void reset() {
  stepStartMs = byDiff(160, 130, 96);
  stepFloorMs = byDiff(90, 55, 50);
  stepRampMs = byDiff(3, 3, 4);
  hudBegin("Snake");
  tft.drawRect(OX, OY - 1, COLS * CS, ROWS * CS + 2, RGB565(40, 90, 40));
  memset(occ, 0, sizeof occ);
  len = 4;
  head = len - 1;
  for (int i = 0; i < len; i++) {
    bx[i] = (uint8_t)(8 + i);
    by[i] = (uint8_t)(ROWS / 2);
    occ[by[i]][bx[i]] = 1;
    drawCell(bx[i], by[i], i == head ? C_YELLOW : C_GREEN);
  }
  dir = nextDir = 1;
  score = 0;
  state = 0;
  placeFood();
  hudScore(score);
  text("Touch to steer", SCR_W / 2, OY + ROWS * CS - 24, 2, C_WHITE, MC_DATUM);
}

static void die() {
  state = 2;
  beep(160, 300);
  overlayGameOver(score);
}

static void step() {
  dir = nextDir;
  int nx = bx[head] + DX[dir];
  int ny = by[head] + DY[dir];
  if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) { die(); return; }
  bool eat = (nx == fx && ny == fy);
  if (!eat) {  // the tail moves away this step
    int t = (head - len + 1 + MAXLEN) % MAXLEN;
    occ[by[t]][bx[t]] = 0;
    eraseCell(bx[t], by[t]);
  }
  if (occ[ny][nx]) { die(); return; }
  drawCell(bx[head], by[head], C_GREEN);  // old head becomes body
  head = (head + 1) % MAXLEN;
  bx[head] = (uint8_t)nx;
  by[head] = (uint8_t)ny;
  occ[ny][nx] = 1;
  drawCell(nx, ny, C_YELLOW);
  if (eat) {
    len++;
    score++;
    hudScore(score);
    beep(1200, 40);
    if (len >= MAXLEN) { die(); return; }
    placeFood();
  }
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed) reset();
    return;
  }
  // steering
  int hx = OX + bx[head] * CS + CS / 2;
  int hy = OY + by[head] * CS + CS / 2;
  if (touch.down) {
    int ddx = touch.x - hx, ddy = touch.y - hy;
    if (iabs(ddx) + iabs(ddy) > 8) {
      int nd = (iabs(ddx) > iabs(ddy)) ? (ddx > 0 ? 1 : 3) : (ddy > 0 ? 2 : 0);
      if (((nd + 2) & 3) != dir) nextDir = nd;
      if (state == 0) {
        state = 1;
        lastStep = millis();
        tft.fillRect(SCR_W / 2 - 70, OY + ROWS * CS - 36, 140, 24, C_BLACK);
      }
    }
  }
  if (touch.swipe >= 0 && ((touch.swipe + 2) & 3) != dir) nextDir = touch.swipe;
  if (state != 1) return;
  uint32_t stepMs = (uint32_t)imax(stepFloorMs, stepStartMs - score * stepRampMs);
  if (millis() - lastStep >= stepMs) {
    lastStep = millis();
    step();
  }
}

static void icon(int cx, int cy) {
  tft.fillRoundRect(cx - 16, cy + 4, 12, 8, 2, C_GREEN);
  tft.fillRoundRect(cx - 6, cy + 4, 12, 8, 2, C_GREEN);
  tft.fillRoundRect(cx - 6, cy - 6, 8, 12, 2, C_GREEN);
  tft.fillRoundRect(cx - 6, cy - 12, 20, 8, 2, C_YELLOW);
  tft.fillCircle(cx + 14, cy + 8, 4, C_RED);
}

}  // namespace Snake
