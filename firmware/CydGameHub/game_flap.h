// Flap - tap anywhere to flap; fly through the gaps between the pipes.
#pragma once
#include "hub.h"

namespace Flap {

static const int GROUND_Y = SCR_H - 14;                 // top of the ground strip
static const int BX = 80, BR = 8, HB = 6;               // bird x, drawn radius, hit-box half size
static const int PW = 44, CAP_H = 14, MAXP = 4, SPACING = 150;
static const int CEIL_Y = PF_Y + BR + 1;                // highest bird centre
static const int FLOOR_Y = GROUND_Y - BR - 1;           // lowest bird centre (touching = dead)
static const int READY_Y = 110;
static const float GRAV = 0.55f, FLAP_V = -6.4f, VMAX = 8.0f;

static const uint16_t SKY    = RGB565(78, 192, 236);
static const uint16_t PIPE_D = RGB565(30, 90, 30);
static const uint16_t PIPE_M = RGB565(80, 180, 50);
static const uint16_t PIPE_L = RGB565(150, 230, 90);
static const uint16_t CAP_M  = RGB565(110, 205, 70);
static const uint16_t DIRT   = RGB565(222, 200, 120);
static const uint16_t GRASS  = RGB565(90, 170, 50);

struct Pipe {
  int16_t x;        // left edge of the body (caps stick out 2 px each side)
  int16_t gapY;     // top of the gap
  int16_t gap;      // gap height
  uint8_t passed;
};

static Pipe pipes[MAXP];
static int np, score, state, scrollAcc, bobT, oldY;     // state: 0 ready, 1 playing, 2 dead
static float by, vy;
static uint32_t deadAt;
static int gapBase, gapShrinkMax, scrollBase;           // difficulty: opening size, how far it shrinks, scroll speed (1/8 px per tick)
static int gC0, gC1, gC2, gC3;                          // current clip rect (x0,y0,x1,y1)

static void setClip(int x0, int y0, int x1, int y1) {
  gC0 = imax(x0, 0);
  gC1 = imax(y0, PF_Y);
  gC2 = imin(x1, SCR_W);
  gC3 = imin(y1, GROUND_Y);
}
static void clipRect(int x, int y, int w, int h, uint16_t c) {
  int x0 = imax(x, gC0), y0 = imax(y, gC1);
  int x1 = imin(x + w, gC2), y1 = imin(y + h, gC3);
  if (x1 > x0 && y1 > y0) tft.fillRect(x0, y0, x1 - x0, y1 - y0, c);
}
static void skyFill(int x0, int y0, int x1, int y1) {
  setClip(x0, y0, x1, y1);
  clipRect(gC0, gC1, gC2 - gC0, gC3 - gC1, SKY);
}

static void pipeBody(int x, int y, int h) {
  if (h <= 0) return;
  clipRect(x, y, PW, h, PIPE_D);
  clipRect(x + 2, y, PW - 4, h, PIPE_M);
  clipRect(x + 6, y, 5, h, PIPE_L);
}
static void pipeCap(int x, int y) {
  clipRect(x, y, PW + 4, CAP_H, PIPE_D);
  clipRect(x + 2, y + 2, PW, CAP_H - 4, CAP_M);
  clipRect(x + 6, y + 2, 5, CAP_H - 4, PIPE_L);
}
// Paint the part of one pipe that lies inside the clip rect.
static void paintPipe(const Pipe& p, int x0, int y0, int x1, int y1) {
  if (p.x + PW + 2 <= x0 || p.x - 2 >= x1) return;
  setClip(x0, y0, x1, y1);
  int gt = p.gapY, gb = p.gapY + p.gap;
  pipeBody(p.x, PF_Y, gt - CAP_H - PF_Y);
  pipeCap(p.x - 2, gt - CAP_H);
  pipeCap(p.x - 2, gb);
  pipeBody(p.x, gb + CAP_H, GROUND_Y - (gb + CAP_H));
}

static void drawBird(int y) {
  tft.fillCircle(BX, y, BR, C_YELLOW);
  tft.fillEllipse(BX - 3, y + 2, 4, 3, C_GOLD);          // wing
  tft.fillCircle(BX + 3, y - 3, 3, C_WHITE);             // eye
  tft.fillCircle(BX + 4, y - 3, 1, C_BLACK);
  tft.fillRect(BX + 6, y, 7, 4, C_ORANGE);               // beak
}
static void eraseBird() {
  int x0 = BX - 9, x1 = BX + 14, y0 = oldY - 9, y1 = oldY + 10;
  skyFill(x0, y0, x1, y1);
  for (int i = 0; i < np; i++) paintPipe(pipes[i], x0, y0, x1, y1);
}
static void moveBird(int ny) {
  if (ny == oldY) return;
  eraseBird();
  oldY = ny;
  drawBird(ny);
}

static void reset() {
  gapBase = byDiff(106, 88, 76);
  gapShrinkMax = byDiff(20, 20, 16);
  scrollBase = byDiff(16, 20, 26);
  hudBegin("Flap", SKY);
  tft.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, DIRT);
  tft.fillRect(0, GROUND_Y, SCR_W, 3, GRASS);
  tft.drawFastHLine(0, GROUND_Y + 3, SCR_W, PIPE_D);
  np = 0;
  score = 0;
  state = 0;
  scrollAcc = 0;
  bobT = 0;
  vy = 0;
  by = (float)READY_Y;
  oldY = READY_Y;
  hudScore(score);
  text("Tap to flap", SCR_W / 2, 170, 2, C_WHITE, MC_DATUM);
  drawBird(oldY);
}

static void die() {
  state = 2;
  deadAt = millis();
  beep(150, 300);
  overlayGameOver(score);
}

static void spawnPipe() {
  Pipe& p = pipes[np++];
  int gap = gapBase - imin(score, gapShrinkMax);
  p.gap = (int16_t)gap;
  p.gapY = (int16_t)rndRange(PF_Y + CAP_H + 8, GROUND_Y - CAP_H - 8 - gap);
  p.x = (int16_t)(SCR_W + 4);
  p.passed = 0;
}

// Scroll every pipe left by dx pixels, redrawing only the leading and trailing slices.
static void movePipes(int dx) {
  for (int i = 0; i < np; i++) {
    Pipe& p = pipes[i];
    int oldL = p.x - 2, oldR = p.x + PW + 2;
    p.x = (int16_t)(p.x - dx);
    skyFill(p.x + PW + 2, PF_Y, oldR, GROUND_Y);
    paintPipe(p, p.x - 2, PF_Y, oldL, GROUND_Y);
  }
  while (np > 0 && pipes[0].x + PW + 2 <= 0) {
    for (int i = 1; i < np; i++) pipes[i - 1] = pipes[i];
    np--;
  }
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && millis() - deadAt >= 500) reset();
    return;
  }
  if (state == 0) {
    if (!touch.pressed) {                                // idle bobbing
      bobT++;
      int t = (bobT / 2) % 16;
      moveBird(READY_Y + (t < 8 ? t : 16 - t) / 2 - 2);
      return;
    }
    state = 1;
    by = (float)oldY;
    skyFill(90, 158, 230, 184);                          // remove the hint text
  }

  // ---- playing
  if (touch.pressed) {
    vy = FLAP_V;
    beep(700, 20);
  }
  vy += GRAV;
  if (vy > VMAX) vy = VMAX;
  by += vy;
  bool dead = false;
  if (by < (float)CEIL_Y) {
    by = (float)CEIL_Y;
    if (vy < 0) vy = 0;
  }
  if (by >= (float)FLOOR_Y) {
    by = (float)FLOOR_Y;
    dead = true;
  }
  int iy = (int)(by + 0.5f);

  scrollAcc += scrollBase + imin(score, 12);             // 1/8 px per tick (normal: 2.5 .. 4 px)
  int dx = scrollAcc >> 3;
  scrollAcc &= 7;
  if (dx > 0) movePipes(dx);
  if (np == 0 || (np < MAXP && pipes[np - 1].x <= SCR_W + 4 - SPACING)) spawnPipe();

  for (int i = 0; i < np; i++) {
    Pipe& p = pipes[i];
    if (!p.passed && p.x + PW < BX - HB) {
      p.passed = 1;
      score++;
      hudScore(score);
      beep(1500, 40);
    }
    if (BX + HB > p.x - 2 && BX - HB < p.x + PW + 2 &&
        (iy - HB < p.gapY || iy + HB > p.gapY + p.gap)) dead = true;
  }

  moveBird(iy);
  if (dead) die();
}

static void icon(int cx, int cy) {
  tft.fillRect(cx + 6, cy - 18, 12, 14, C_GREEN);
  tft.fillRect(cx + 3, cy - 6, 18, 6, RGB565(150, 230, 90));
  tft.fillRect(cx + 3, cy + 10, 18, 6, RGB565(150, 230, 90));
  tft.fillRect(cx + 6, cy + 16, 12, 4, C_GREEN);
  tft.fillCircle(cx - 8, cy + 3, 9, C_YELLOW);
  tft.fillCircle(cx - 5, cy, 3, C_WHITE);
  tft.fillCircle(cx - 4, cy, 1, C_BLACK);
  tft.fillRect(cx - 1, cy + 3, 6, 4, C_ORANGE);
}

}  // namespace Flap
