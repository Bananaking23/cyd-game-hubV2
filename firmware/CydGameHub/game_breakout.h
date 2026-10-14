// Breakout - drag to move the paddle, tap to launch the ball. Clear all the bricks.
#pragma once
#include "hub.h"
#include <math.h>

namespace Breakout {

static const int COLS = 10, ROWS = 6, BW = 32, BH = 12;   // brick cell (the drawn brick is 30x10)
static const int BRY0 = 48;                              // top of the brick block
static const int WALL_Y = PF_Y + 13;                     // top wall line; lives / level strip above it
static const int PAD_Y = 214, PAD_H = 7, PAD_SPD = 20;
static const int BALL_R = 3;                                 // ball half size (7x7 drawn)
static const int LOSE_Y = 232;

static const uint16_t ROW_COL[ROWS] = {C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_CYAN, C_MAGENTA};
static const uint16_t PAD_COL = RGB565(200, 210, 235);

static uint8_t grid[ROWS * COLS];
static int remain, score, lives, level, hits, state;     // state: 0 ready, 1 playing, 2 over
static int padX, padW, lastPadX, lastPadW;
static int lbx, lby;                                     // last drawn ball centre
static float bx, by, vx, vy;
static bool scoreDirty;
static int padStartW, padMinW, startLives, speedPct;   // difficulty: paddle widths, lives, ball speed in percent
static uint32_t deadAt;

static float curSpeed() {
  float s = 3.2f + 0.35f * (float)(level - 1) + 0.04f * (float)hits;
  if (s > 6.2f) s = 6.2f;
  return s * ((float)speedPct / 100.0f);
}

static void drawBrick(int i) {
  int r = i / COLS, c = i % COLS;
  tft.fillRect(c * BW + 1, BRY0 + r * BH + 1, BW - 2, BH - 2, ROW_COL[r]);
}
static void drawBricks() {
  for (int i = 0; i < ROWS * COLS; i++) if (grid[i]) drawBrick(i);
}
static void newBricks() {
  for (int i = 0; i < ROWS * COLS; i++) grid[i] = 1;
  remain = ROWS * COLS;
  drawBricks();
}

static void drawLives() {
  tft.fillRect(0, PF_Y, 110, WALL_Y - PF_Y, C_BLACK);
  for (int i = 0; i < lives; i++) tft.fillCircle(9 + i * 14, PF_Y + 6, 4, C_RED);
}
static void drawLevel() {
  char b[16];
  snprintf(b, sizeof b, "Level %d", level);
  tft.fillRect(SCR_W - 90, PF_Y, 90, WALL_Y - PF_Y, C_BLACK);
  text(b, SCR_W - 6, PF_Y + 3, 1, C_LGREY, TR_DATUM);
}

static void drawBall(int x, int y) { tft.fillCircle(x, y, BALL_R, C_WHITE); }
static void eraseBall() { tft.fillRect(lbx - BALL_R, lby - BALL_R, 2 * BALL_R + 1, 2 * BALL_R + 1, C_BLACK); }

static void drawPaddle() {
  tft.fillRoundRect(padX, PAD_Y, padW, PAD_H, 3, PAD_COL);
  tft.fillRect(padX + padW / 2 - 5, PAD_Y + 1, 10, PAD_H - 2, C_CYAN);
}
// Move the paddle to its new x: erase only the vacated part, then redraw it.
static void paddleRedraw() {
  int e0 = lastPadX, e1 = lastPadX + lastPadW;
  if (padX > e0) tft.fillRect(e0, PAD_Y, imin(e1, padX) - e0, PAD_H, C_BLACK);
  if (padX + padW < e1) {
    int s = imax(e0, padX + padW);
    tft.fillRect(s, PAD_Y, e1 - s, PAD_H, C_BLACK);
  }
  drawPaddle();
  lastPadX = padX;
  lastPadW = padW;
}

static void hint() {
  char b[40];
  if (level > 1) snprintf(b, sizeof b, "Level %d - tap to launch", level);
  else snprintf(b, sizeof b, "Tap to launch");
  textBox(b, SCR_W / 2, 170, 220, 22, 2, C_WHITE, C_BLACK);
}
static void hintClear() { tft.fillRect(SCR_W / 2 - 110, 159, 220, 22, C_BLACK); }

static void placeBallOnPaddle() {
  bx = (float)(padX + padW / 2);
  by = (float)(PAD_Y - BALL_R - 1);
  vx = vy = 0;
}

static void ready() {
  state = 0;
  placeBallOnPaddle();
  hint();
}

static void reset() {
  hudBegin("Breakout");
  tft.drawFastHLine(0, WALL_Y, SCR_W, RGB565(70, 80, 100));
  padStartW = byDiff(66, 54, 44);
  padMinW = byDiff(46, 36, 30);
  startLives = byDiff(4, 3, 2);
  speedPct = byDiff(85, 100, 125);
  score = 0;
  lives = startLives;
  level = 1;
  hits = 0;
  padW = padStartW;
  padX = (SCR_W - padW) / 2;
  lastPadX = padX;
  lastPadW = padW;
  lbx = (int)bx;
  lby = (int)by;
  newBricks();
  drawLives();
  drawLevel();
  hudScore(score);
  scoreDirty = false;
  drawPaddle();
  ready();
  lbx = (int)(bx + 0.5f);
  lby = (int)(by + 0.5f);
  drawBall(lbx, lby);
}

static void killBrick(int i) {
  int r = i / COLS, c = i % COLS;
  grid[i] = 0;
  remain--;
  hits++;
  score += 7 - r;
  scoreDirty = true;
  tft.fillRect(c * BW, BRY0 + r * BH, BW, BH, C_BLACK);
  beep(400 + 110 * (ROWS - 1 - r), 30);
}
static bool hitCell(int px, int py) {
  if (py < BRY0 || px < 0) return false;
  int r = (py - BRY0) / BH, c = px / BW;
  if (r >= ROWS || c >= COLS) return false;
  int i = r * COLS + c;
  if (!grid[i]) return false;
  killBrick(i);
  return true;
}
// Would a ball centred at (fx,fy) overlap a live brick cell? Kills every brick it touches.
static bool ballHits(float fx, float fy) {
  int x = (int)(fx + 0.5f), y = (int)(fy + 0.5f);
  bool h = false;
  if (hitCell(x - BALL_R, y - BALL_R)) h = true;
  if (hitCell(x + BALL_R, y - BALL_R)) h = true;
  if (hitCell(x - BALL_R, y + BALL_R)) h = true;
  if (hitCell(x + BALL_R, y + BALL_R)) h = true;
  return h;
}

static void launch() {
  float sp = curSpeed();
  vx = (rnd(2) ? 1.0f : -1.0f) * sp * 0.35f;
  vy = -sqrtf(sp * sp - vx * vx);
  state = 1;
  hintClear();
}

static void loseLife() {
  beep(160, 280);
  lives--;
  drawLives();
  eraseBall();
  if (lives <= 0) {
    state = 2;
    deadAt = millis();
    overlayGameOver(score);
    return;
  }
  ready();
  lbx = (int)(bx + 0.5f);
  lby = (int)(by + 0.5f);
}

static void levelClear() {
  level++;
  hits = 0;
  padW = imax(padMinW, padStartW - 3 * (level - 1));
  padX = clampi(padX, 0, SCR_W - padW);
  eraseBall();
  newBricks();
  drawLevel();
  paddleRedraw();
  ready();
  lbx = (int)(bx + 0.5f);
  lby = (int)(by + 0.5f);
  beep(1000, 60);
}

static void physics() {
  float m = fabsf(vx) > fabsf(vy) ? fabsf(vx) : fabsf(vy);
  int n = (int)(m / 2.0f) + 1;
  float sx = vx / (float)n, sy = vy / (float)n;
  for (int k = 0; k < n && state == 1; k++) {
    // horizontal axis
    float nx = bx + sx;
    if (nx - BALL_R < 0) { nx = (float)BALL_R; vx = -vx; sx = -sx; }
    else if (nx + BALL_R > SCR_W - 1) { nx = (float)(SCR_W - 1 - BALL_R); vx = -vx; sx = -sx; }
    if (ballHits(nx, by)) { vx = -vx; sx = -sx; }
    else bx = nx;
    // vertical axis
    float ny = by + sy;
    if (ny - BALL_R < WALL_Y + 1) { ny = (float)(WALL_Y + 1 + BALL_R); vy = -vy; sy = -sy; }
    if (ballHits(bx, ny)) { vy = -vy; sy = -sy; }
    else by = ny;
    // paddle
    if (vy > 0 && by + BALL_R >= PAD_Y && by <= PAD_Y + PAD_H &&
        bx + BALL_R >= padX && bx - BALL_R <= padX + padW) {
      float t = (bx - (float)(padX + padW / 2)) / (float)(padW / 2 + BALL_R);
      if (t > 1.0f) t = 1.0f;
      if (t < -1.0f) t = -1.0f;
      float sp = curSpeed();
      vx = t * 0.85f * sp;
      if (fabsf(vx) < 0.4f) vx = (vx < 0 ? -0.4f : 0.4f);
      vy = -sqrtf(sp * sp - vx * vx);
      by = (float)(PAD_Y - BALL_R - 1);
      sx = vx / (float)n;
      sy = vy / (float)n;
      beep(300, 25);
    }
    if (by > (float)LOSE_Y) { loseLife(); return; }
    if (remain <= 0) { levelClear(); return; }
  }
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && millis() - deadAt >= 500) reset();
    return;
  }
  // paddle follows the finger
  if (touch.down) {
    int target = clampi(touch.x - padW / 2, 0, SCR_W - padW);
    int d = target - padX;
    if (iabs(d) > 1) padX += clampi(d, -PAD_SPD, PAD_SPD);
  }
  if (state == 0) {
    placeBallOnPaddle();
    if (touch.tap) launch();
  } else {
    physics();
  }
  if (state == 2) return;                    // game over overlay was just drawn
  eraseBall();
  paddleRedraw();
  lbx = (int)(bx + 0.5f);
  lby = (int)(by + 0.5f);
  drawBall(lbx, lby);
  if (scoreDirty) {
    hudScore(score);
    scoreDirty = false;
  }
}

static void icon(int cx, int cy) {
  static const uint16_t col[3] = {C_RED, C_YELLOW, C_GREEN};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      tft.fillRect(cx - 17 + c * 12, cy - 16 + r * 7, 10, 5, col[r]);
  tft.fillCircle(cx + 6, cy + 6, 3, C_WHITE);
  tft.fillRoundRect(cx - 12, cy + 13, 24, 5, 2, C_LGREY);
}

}  // namespace Breakout
