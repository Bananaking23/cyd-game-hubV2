// Invaders - drag your finger left/right to move the ship; it auto-fires while you touch.
#pragma once
#include "hub.h"

namespace Invaders {

static const int ROWS = 5, COLS = 8;
static const int AW = 16, AH = 10, GX = 8, GY = 8;       // alien size, gaps
static const int SHIPY = SCR_H - 16, SHIPW = 22, SHIPH = 10;
static const int MAXB = 3, MAXE = 3;

static bool alive[ROWS][COLS];
static int gridX, gridY, gdir, frame, alienCount;
static int shipX, lives, score, wave, state;   // state: 0 ready, 1 play, 2 dead, 3 wave clear
static uint32_t lastMove, lastShot, lastFrame, pauseUntil;
static int bulletX[MAXB], bulletY[MAXB];
static bool bulletOn[MAXB];
static int eX[MAXE], eY[MAXE];
static bool eOn[MAXE];
static int oldShipX;
static int startLives, moveSlowPct, moveFloorMs, fireBasePct;   // difficulty: lives, alien step interval scale/floor, shot chance

static const uint16_t ROWCOL[ROWS] = {C_MAGENTA, C_PURPLE, C_CYAN, C_GREEN, C_GREEN};

static int cellX(int c) { return gridX + c * (AW + GX); }
static int cellY(int r) { return gridY + r * (AH + GY); }

static void drawAlien(int x, int y, uint16_t col, int f) {
  // 16x10 crab-like sprite built from rectangles
  tft.fillRect(x + 3, y, 10, 2, col);
  tft.fillRect(x + 1, y + 2, 14, 4, col);
  tft.fillRect(x + 3, y + 3, 2, 2, C_BLACK);
  tft.fillRect(x + 11, y + 3, 2, 2, C_BLACK);
  if (f) {
    tft.fillRect(x, y + 6, 3, 4, col);
    tft.fillRect(x + 13, y + 6, 3, 4, col);
    tft.fillRect(x + 5, y + 6, 2, 2, col);
    tft.fillRect(x + 9, y + 6, 2, 2, col);
  } else {
    tft.fillRect(x + 1, y + 6, 3, 2, col);
    tft.fillRect(x + 12, y + 6, 3, 2, col);
    tft.fillRect(x + 3, y + 8, 3, 2, col);
    tft.fillRect(x + 10, y + 8, 3, 2, col);
  }
}
static void eraseAlien(int x, int y) { tft.fillRect(x, y, AW, AH, C_BLACK); }

static void drawGrid() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (alive[r][c]) drawAlien(cellX(c), cellY(r), ROWCOL[r], frame);
}
static void eraseGrid() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (alive[r][c]) eraseAlien(cellX(c), cellY(r));
}
static void drawShip(int x, uint16_t col) {
  tft.fillRect(x, SHIPY + 4, SHIPW, 6, col);
  tft.fillRect(x + SHIPW / 2 - 2, SHIPY, 4, 5, col);
}
static void drawLives() {
  char b[16];
  snprintf(b, sizeof b, "Lives %d", lives);
  tft.fillRect(6, SCR_H - 12, 60, 10, C_BLACK);
  text(b, 6, SCR_H - 12, 1, C_LGREY);
}

static void clearShots() {
  for (int i = 0; i < MAXB; i++) bulletOn[i] = false;
  for (int i = 0; i < MAXE; i++) eOn[i] = false;
}

static void startWave() {
  hudBegin("Invaders");
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) alive[r][c] = true;
  alienCount = ROWS * COLS;
  gridX = 12;
  gridY = PF_Y + 8 + imin(wave - 1, 4) * 6;
  gdir = 1;
  frame = 0;
  shipX = SCR_W / 2 - SHIPW / 2;
  oldShipX = shipX;
  clearShots();
  drawGrid();
  drawShip(shipX, C_GREEN);
  drawLives();
  hudScore(score);
  state = 0;
  text("Drag to move - hold to fire", SCR_W / 2, SCR_H - 40, 2, C_WHITE, MC_DATUM);
  lastMove = lastShot = lastFrame = millis();
}
static void startGame() {
  startLives = byDiff(4, 3, 2);
  moveSlowPct = byDiff(125, 100, 78);
  moveFloorMs = byDiff(100, 70, 50);
  fireBasePct = byDiff(1, 2, 3);
  score = 0;
  lives = startLives;
  wave = 1;
  startWave();
}
static void init() { startGame(); }

static void loseLife() {
  lives--;
  beep(150, 300);
  if (lives <= 0) {
    state = 2;
    overlayGameOver(score);
    return;
  }
  drawLives();
  clearShots();
  tft.fillRect(0, PF_Y, SCR_W, SCR_H - PF_Y - 14, C_BLACK);
  drawGrid();
  drawShip(shipX, C_GREEN);
  pauseUntil = millis() + 800;
  state = 3;
}

static void tick() {
  if (state == 2) {
    if (touch.pressed) startGame();
    return;
  }
  if (state == 3) {
    if ((int32_t)(millis() - pauseUntil) >= 0) {
      state = 1;
      lastMove = millis();
    }
    return;
  }
  // ship
  if (touch.down) {
    int target = touch.x - SHIPW / 2;
    int d = clampi(target - shipX, -9, 9);
    shipX = clampi(shipX + d, 0, SCR_W - SHIPW);
    if (state == 0) {
      state = 1;
      tft.fillRect(0, SCR_H - 52, SCR_W, 22, C_BLACK);
      lastMove = millis();
    }
  }
  if (state == 0) return;
  if (shipX != oldShipX) {
    drawShip(oldShipX, C_BLACK);
    drawShip(shipX, C_GREEN);
    oldShipX = shipX;
  }
  uint32_t now = millis();

  // player fire
  if (touch.down && now - lastShot > 380) {
    for (int i = 0; i < MAXB; i++) {
      if (!bulletOn[i]) {
        bulletOn[i] = true;
        bulletX[i] = shipX + SHIPW / 2 - 1;
        bulletY[i] = SHIPY - 6;
        lastShot = now;
        beep(1500, 20);
        break;
      }
    }
  }
  // player bullets
  for (int i = 0; i < MAXB; i++) {
    if (!bulletOn[i]) continue;
    tft.fillRect(bulletX[i], bulletY[i], 2, 6, C_BLACK);
    bulletY[i] -= 7;
    if (bulletY[i] < PF_Y) { bulletOn[i] = false; continue; }
    bool hit = false;
    for (int r = 0; r < ROWS && !hit; r++) {
      for (int c = 0; c < COLS && !hit; c++) {
        if (!alive[r][c]) continue;
        if (inRect(bulletX[i] + 1, bulletY[i] + 2, cellX(c), cellY(r), AW, AH)) {
          alive[r][c] = false;
          eraseAlien(cellX(c), cellY(r));
          alienCount--;
          score += (ROWS - r) * 10;
          hudScore(score);
          beep(500, 30);
          hit = true;
        }
      }
    }
    if (hit) bulletOn[i] = false;
    else tft.fillRect(bulletX[i], bulletY[i], 2, 6, C_YELLOW);
  }
  if (alienCount <= 0) {
    wave++;
    beep(1800, 200);
    startWave();
    state = 1;     // continue straight away; ship keeps being controlled
    return;
  }

  // alien movement: faster when fewer remain and on later waves
  uint32_t moveMs = (uint32_t)imax(moveFloorMs, (90 + alienCount * 12 - wave * 20) * moveSlowPct / 100);
  if (now - lastMove >= moveMs) {
    lastMove = now;
    // find horizontal extent of live columns
    int minC = COLS, maxC = -1, lowR = -1;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (alive[r][c]) {
          if (c < minC) minC = c;
          if (c > maxC) maxC = c;
          if (r > lowR) lowR = r;
        }
    eraseGrid();
    int nx = gridX + gdir * 6;
    if (cellX(minC) + (nx - gridX) < 2 || cellX(maxC) + AW + (nx - gridX) > SCR_W - 2) {
      gdir = -gdir;
      gridY += 8;
    } else {
      gridX = nx;
    }
    frame ^= 1;
    drawGrid();
    if (cellY(lowR) + AH >= SHIPY) {  // invaded
      lives = 1;
      loseLife();
      return;
    }
  }

  // alien fire
  if (rnd(100) < fireBasePct + wave) {
    for (int i = 0; i < MAXE; i++) {
      if (eOn[i]) continue;
      int c = rnd(COLS);
      int rr = -1;
      for (int r = ROWS - 1; r >= 0; r--) if (alive[r][c]) { rr = r; break; }
      if (rr < 0) break;
      eOn[i] = true;
      eX[i] = cellX(c) + AW / 2;
      eY[i] = cellY(rr) + AH;
      break;
    }
  }
  for (int i = 0; i < MAXE; i++) {
    if (!eOn[i]) continue;
    tft.fillRect(eX[i], eY[i], 3, 7, C_BLACK);
    eY[i] += 4 + imin(wave, 3);
    if (eY[i] > SCR_H - 14) { eOn[i] = false; continue; }
    if (inRect(eX[i] + 1, eY[i] + 6, shipX, SHIPY, SHIPW, SHIPH)) {
      eOn[i] = false;
      loseLife();
      return;
    }
    tft.fillRect(eX[i], eY[i], 3, 7, C_RED);
  }
}

static void icon(int cx, int cy) {
  drawAlien(cx - 8, cy - 14, C_GREEN, 1);
  drawAlien(cx - 8, cy, C_MAGENTA, 0);
  tft.fillRect(cx - 9, cy + 13, 18, 4, C_CYAN);
}

}  // namespace Invaders
