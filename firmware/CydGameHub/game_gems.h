// Gems - match-3. Tap a gem and then a neighbouring gem (or swipe a gem towards its neighbour) to swap them.
// Line up three or more of a kind to clear them; chains of cascades score more. You have a limited number of moves.
#pragma once
#include "hub.h"

namespace Gems {

static const int COLS = 9, ROWS = 6, CSZ = 34, OX = 7, OY = PF_Y + 6, EMPTY = 255;   // grid spans x 7..312, y 28..231
static const uint16_t CELL_BG = RGB565(20, 24, 40);
static const uint16_t GC[7] = {C_RED, C_GREEN, RGB565(70, 140, 255), C_YELLOW, C_ORANGE, C_MAGENTA, C_CYAN};

static uint8_t g[ROWS][COLS];       // gem type per cell, EMPTY while falling
static uint8_t mk[ROWS][COLS];      // marked for clearing
static uint8_t dirty[ROWS][COLS];   // needs redrawing
static int ntypes, moves, score, chain, state, selR, selC;   // state: 0 idle, 1 flashing matches, 2 falling, 3 game over
static uint32_t tState;

static void drawShape(int cx, int cy, int t, uint16_t c) {
  uint16_t hi = RGB565(255, 255, 255);
  switch (t) {
    case 0:  tft.fillCircle(cx, cy, 12, c); tft.fillCircle(cx - 4, cy - 4, 3, hi); break;
    case 1:  tft.fillRoundRect(cx - 11, cy - 11, 22, 22, 4, c); tft.fillRect(cx - 7, cy - 7, 6, 3, hi); break;
    case 2:  tft.fillTriangle(cx, cy - 13, cx - 13, cy + 11, cx + 13, cy + 11, c); tft.fillRect(cx - 2, cy - 2, 4, 3, hi); break;
    case 3:
      tft.fillTriangle(cx, cy - 14, cx - 12, cy, cx + 12, cy, c);
      tft.fillTriangle(cx, cy + 14, cx - 12, cy, cx + 12, cy, c);
      tft.fillRect(cx - 3, cy - 5, 3, 3, hi);
      break;
    case 4:  tft.fillCircle(cx, cy, 12, c); tft.fillCircle(cx, cy, 6, CELL_BG); tft.fillCircle(cx, cy, 2, c); break;
    case 5:  tft.fillRect(cx - 4, cy - 12, 8, 24, c); tft.fillRect(cx - 12, cy - 4, 24, 8, c); tft.fillRect(cx - 2, cy - 2, 4, 4, hi); break;
    default:
      tft.fillTriangle(cx - 12, cy - 12, cx + 12, cy - 12, cx, cy, c);
      tft.fillTriangle(cx - 12, cy + 12, cx + 12, cy + 12, cx, cy, c);
      break;
  }
}

static void drawCell(int r, int c) {
  int x = OX + c * CSZ, y = OY + r * CSZ;
  tft.fillRoundRect(x + 1, y + 1, CSZ - 2, CSZ - 2, 5, CELL_BG);
  dirty[r][c] = 0;
  if (g[r][c] == EMPTY) return;
  if (mk[r][c]) {
    tft.fillRoundRect(x + 4, y + 4, CSZ - 8, CSZ - 8, 5, C_WHITE);
  } else {
    drawShape(x + CSZ / 2, y + CSZ / 2, g[r][c], GC[g[r][c] % 7]);
  }
  if (r == selR && c == selC) {
    tft.drawRoundRect(x + 1, y + 1, CSZ - 2, CSZ - 2, 5, C_WHITE);
    tft.drawRoundRect(x + 2, y + 2, CSZ - 4, CSZ - 4, 4, C_WHITE);
  }
}
static void drawDirty() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (dirty[r][c]) drawCell(r, c);
}
static void drawAll() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) drawCell(r, c);
}
static void hud() {
  char b[32];
  snprintf(b, sizeof b, "Score %d  Moves %d", score, moves);
  hudText(b);
}

// Marks every cell that belongs to a run of 3+; returns how many cells are marked.
static int findMatches() {
  memset(mk, 0, sizeof mk);
  for (int r = 0; r < ROWS; r++) {
    int c = 0;
    while (c < COLS) {
      if (g[r][c] == EMPTY) { c++; continue; }
      int e = c;
      while (e + 1 < COLS && g[r][e + 1] == g[r][c]) e++;
      if (e - c >= 2) for (int i = c; i <= e; i++) mk[r][i] = 1;
      c = e + 1;
    }
  }
  for (int c = 0; c < COLS; c++) {
    int r = 0;
    while (r < ROWS) {
      if (g[r][c] == EMPTY) { r++; continue; }
      int e = r;
      while (e + 1 < ROWS && g[e + 1][c] == g[r][c]) e++;
      if (e - r >= 2) for (int i = r; i <= e; i++) mk[i][c] = 1;
      r = e + 1;
    }
  }
  int n = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) n += mk[r][c];
  return n;
}
static bool matchAt(int r, int c) {                      // does the gem at (r,c) sit in a run of 3+?
  int t = g[r][c];
  if (t == EMPTY) return false;
  int h = 1, v = 1;
  for (int i = c - 1; i >= 0 && g[r][i] == t; i--) h++;
  for (int i = c + 1; i < COLS && g[r][i] == t; i++) h++;
  for (int i = r - 1; i >= 0 && g[i][c] == t; i--) v++;
  for (int i = r + 1; i < ROWS && g[i][c] == t; i++) v++;
  return h >= 3 || v >= 3;
}
static void swapCells(int r1, int c1, int r2, int c2) {
  uint8_t t = g[r1][c1];
  g[r1][c1] = g[r2][c2];
  g[r2][c2] = t;
}
static bool hasMove() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      if (c + 1 < COLS) {
        swapCells(r, c, r, c + 1);
        bool m = matchAt(r, c) || matchAt(r, c + 1);
        swapCells(r, c, r, c + 1);
        if (m) return true;
      }
      if (r + 1 < ROWS) {
        swapCells(r, c, r + 1, c);
        bool m = matchAt(r, c) || matchAt(r + 1, c);
        swapCells(r, c, r + 1, c);
        if (m) return true;
      }
    }
  return false;
}
static void fillBoard() {                                // random board with no ready-made matches and at least one move
  do {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++) {
        int t, guard = 0;
        do {
          t = rnd(ntypes);
        } while (guard++ < 40 && ((c >= 2 && g[r][c - 1] == t && g[r][c - 2] == t) || (r >= 2 && g[r - 1][c] == t && g[r - 2][c] == t)));
        g[r][c] = (uint8_t)t;
      }
  } while (findMatches() > 0 || !hasMove());
  memset(mk, 0, sizeof mk);
}

static void reset() {
  ntypes = byDiff(5, 6, 7);
  moves = byDiff(30, 25, 20);
  score = 0;
  chain = 0;
  state = 0;
  selR = selC = -1;
  hudBegin("Gems");
  fillBoard();
  drawAll();
  hud();
}

static void startFlash() {                               // matches are already marked
  chain++;
  int cnt = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (mk[r][c]) { cnt++; drawCell(r, c); }
  score += cnt * 10 * chain + (cnt > 3 ? (cnt - 3) * 10 : 0);
  hud();
  beep(500 + imin(chain, 6) * 140, 70);
  state = 1;
  tState = millis();
}

static void trySwap(int r1, int c1, int r2, int c2) {
  swapCells(r1, c1, r2, c2);
  if (findMatches() == 0) {                              // not a match: undo
    swapCells(r1, c1, r2, c2);
    beep(200, 50);
    return;
  }
  moves--;
  chain = 0;
  drawCell(r1, c1);
  drawCell(r2, c2);
  startFlash();
}

static void settle() {                                   // nothing is falling any more
  if (findMatches() > 0) { startFlash(); return; }
  if (moves <= 0) {
    state = 3;
    tState = millis();
    beep(300, 200);
    overlayGameOver(score);
    return;
  }
  if (!hasMove()) {                                      // dead board: shuffle a fresh one for free
    fillBoard();
    drawAll();
    hudText("New board!", C_ORANGE);
  }
  state = 0;
}

static void fallStep() {
  bool empty = false;
  for (int c = 0; c < COLS; c++) {
    for (int r = ROWS - 1; r >= 1; r--) {
      if (g[r][c] == EMPTY && g[r - 1][c] != EMPTY) {
        g[r][c] = g[r - 1][c];
        g[r - 1][c] = EMPTY;
        dirty[r][c] = dirty[r - 1][c] = 1;
      }
    }
    if (g[0][c] == EMPTY) { g[0][c] = (uint8_t)rnd(ntypes); dirty[0][c] = 1; }
    for (int r = 0; r < ROWS; r++) if (g[r][c] == EMPTY) empty = true;
  }
  drawDirty();
  if (!empty) settle();
}

static void init() { reset(); }

static void tick() {
  if (state == 3) {
    if (touch.pressed && millis() - tState >= 500) reset();
    return;
  }
  if (state == 1) {
    if (millis() - tState < 130) return;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (mk[r][c]) { g[r][c] = EMPTY; mk[r][c] = 0; drawCell(r, c); }
    state = 2;
    tState = millis();
    return;
  }
  if (state == 2) {
    if (millis() - tState >= 60) { tState = millis(); fallStep(); }
    return;
  }
  // idle: swipe a gem, or tap one gem and then a neighbour
  if (touch.swipe >= 0 && inRect(touch.sx, touch.sy, OX, OY, COLS * CSZ, ROWS * CSZ)) {
    int c = clampi((touch.sx - OX) / CSZ, 0, COLS - 1), r = clampi((touch.sy - OY) / CSZ, 0, ROWS - 1);
    int r2 = r + (touch.swipe == 2) - (touch.swipe == 0), c2 = c + (touch.swipe == 1) - (touch.swipe == 3);
    if (r2 >= 0 && r2 < ROWS && c2 >= 0 && c2 < COLS) {
      int sr = selR, sc = selC;
      selR = selC = -1;
      if (sr >= 0) drawCell(sr, sc);
      trySwap(r, c, r2, c2);
    }
    return;
  }
  if (!touch.tap || !inRect(touch.x, touch.y, OX, OY, COLS * CSZ, ROWS * CSZ)) return;
  int c = clampi((touch.x - OX) / CSZ, 0, COLS - 1), r = clampi((touch.y - OY) / CSZ, 0, ROWS - 1);
  int sr = selR, sc = selC;
  if (sr < 0) {
    selR = r; selC = c;
    drawCell(r, c);
    beep(900, 15);
  } else if (sr == r && sc == c) {
    selR = selC = -1;
    drawCell(r, c);
  } else if (iabs(sr - r) + iabs(sc - c) == 1) {
    selR = selC = -1;
    drawCell(sr, sc);
    trySwap(sr, sc, r, c);
  } else {
    selR = r; selC = c;
    drawCell(sr, sc);
    drawCell(r, c);
    beep(900, 15);
  }
}

static void icon(int cx, int cy) {
  tft.fillCircle(cx - 10, cy - 9, 7, C_RED);
  tft.fillTriangle(cx + 10, cy - 16, cx + 3, cy - 3, cx + 17, cy - 3, RGB565(70, 140, 255));
  tft.fillTriangle(cx - 10, cy + 3, cx - 17, cy + 11, cx - 3, cy + 11, C_YELLOW);
  tft.fillTriangle(cx - 10, cy + 19, cx - 17, cy + 11, cx - 3, cy + 11, C_YELLOW);
  tft.fillRect(cx + 7, cy + 4, 6, 16, C_MAGENTA);
  tft.fillRect(cx + 2, cy + 9, 16, 6, C_MAGENTA);
}

}  // namespace Gems
