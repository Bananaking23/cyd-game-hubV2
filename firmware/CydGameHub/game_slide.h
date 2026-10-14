// Slide Puzzle - sliding tiles. Tap a tile in the blank's row or column: that whole line slides towards the blank.
// Order the numbers 1..N*N-1 with the blank in the last cell. Grid: Easy 3x3, Normal 4x4, Hard 5x5.
// Score = number of slides (taps that moved tiles); LOWER is better, only recorded on a win.
#pragma once
#include "hub.h"

namespace Slide15 {

static const int MAX_DIM  = 5;
static const int GRID_X0  = 8;
static const int PANEL_X  = 228;                 // side panel x .. SCR_W
static const int PANEL_W  = SCR_W - PANEL_X;
static const int NEW_X = 234, NEW_Y = 176, NEW_W = 80, NEW_H = 48;
static const uint16_t COL_BLANK = RGB565(18, 20, 30);

enum { SL_PLAY = 0, SL_WON = 1, SL_OVER = 2 };

static uint8_t  board[MAX_DIM * MAX_DIM];        // 0 = blank
static uint16_t tileCol[MAX_DIM * MAX_DIM];      // by tile number
static uint16_t tileTxt[MAX_DIM * MAX_DIM];
static int      dim, cellPx, gridY0, blankAt, moves, state;
static int      shuffleSteps;                    // difficulty-dependent
static uint32_t timerAt;

// ------------------------------------------------------------------ colours
static uint16_t hsv565(int h, int s, int v, int* lumaOut) {
  int region = (h / 60) % 6, rem = (h % 60) * 255 / 60;
  int p = v * (255 - s) / 255;
  int q = v * (255 - s * rem / 255) / 255;
  int t = v * (255 - s * (255 - rem) / 255) / 255;
  int r, g, b;
  switch (region) {
    case 0:  r = v; g = t; b = p; break;
    case 1:  r = q; g = v; b = p; break;
    case 2:  r = p; g = v; b = t; break;
    case 3:  r = p; g = q; b = v; break;
    case 4:  r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  *lumaOut = (r * 3 + g * 6 + b) / 10;
  return RGB565(r, g, b);
}
static void buildColours() {
  int total = dim * dim;
  for (int v = 1; v < total; v++) {
    int luma = 0;
    int hue = (v - 1) * 300 / (total - 2);     // total >= 9 so total - 2 >= 7
    tileCol[v] = hsv565(hue, 215, 205, &luma);
    tileTxt[v] = (luma > 135) ? C_BLACK : C_WHITE;
  }
}

// ------------------------------------------------------------------ drawing
static void drawCell(int idx) {
  int x = GRID_X0 + (idx % dim) * cellPx;
  int y = gridY0 + (idx / dim) * cellPx;
  tft.fillRect(x, y, cellPx, cellPx, C_BLACK);
  int v = board[idx];
  if (v == 0) {
    tft.fillRoundRect(x + 3, y + 3, cellPx - 6, cellPx - 6, 6, COL_BLANK);
    return;
  }
  tft.fillRoundRect(x + 2, y + 2, cellPx - 4, cellPx - 4, 7, tileCol[v]);
  tft.drawRoundRect(x + 2, y + 2, cellPx - 4, cellPx - 4, 7, C_WHITE);
  char b[24];
  snprintf(b, sizeof b, "%d", v);
  text(b, x + cellPx / 2, y + cellPx / 2 + 1, 4, tileTxt[v], MC_DATUM, dim == 3 ? 2 : 1);
}
static void drawAll() {
  for (int i = 0; i < dim * dim; i++) drawCell(i);
}
static void drawPanel() {
  tft.fillRect(PANEL_X, PF_Y, PANEL_W, PF_H, C_BLACK);
  char b[24];
  text("GRID", PANEL_X + PANEL_W / 2, PF_Y + 16, 1, C_LGREY, MC_DATUM);
  snprintf(b, sizeof b, "%dx%d", dim, dim);
  text(b, PANEL_X + PANEL_W / 2, PF_Y + 38, 4, C_GOLD, MC_DATUM);
  text("BEST", PANEL_X + PANEL_W / 2, PF_Y + 76, 1, C_LGREY, MC_DATUM);
  int best = bestScore();
  if (best > 0) snprintf(b, sizeof b, "%d", best);
  else          snprintf(b, sizeof b, "--");
  text(b, PANEL_X + PANEL_W / 2, PF_Y + 98, 4, C_CYAN, MC_DATUM);
  button(NEW_X, NEW_Y, NEW_W, NEW_H, "NEW", RGB565(30, 120, 60), C_WHITE, 4);
}

// ------------------------------------------------------------------ logic
static bool isSolved() {
  int total = dim * dim;
  for (int i = 0; i < total - 1; i++) if (board[i] != i + 1) return false;
  return board[total - 1] == 0;
}

// Random legal single-tile moves from the solved state: always solvable, never left solved.
static void shuffleBoard() {
  int total = dim * dim;
  for (int i = 0; i < total - 1; i++) board[i] = (uint8_t)(i + 1);
  board[total - 1] = 0;
  blankAt = total - 1;
  int last = -1;
  for (int s = 0; s < shuffleSteps; s++) {
    int cand[4], nc = 0;
    int r = blankAt / dim, c = blankAt % dim;
    if (r > 0 && blankAt - dim != last)       cand[nc++] = blankAt - dim;
    if (r < dim - 1 && blankAt + dim != last) cand[nc++] = blankAt + dim;
    if (c > 0 && blankAt - 1 != last)         cand[nc++] = blankAt - 1;
    if (c < dim - 1 && blankAt + 1 != last)   cand[nc++] = blankAt + 1;
    if (nc == 0) break;                        // cannot happen for dim >= 3
    int nb = cand[rnd(nc)];
    board[blankAt] = board[nb];
    board[nb] = 0;
    last = blankAt;
    blankAt = nb;
  }
  if (isSolved()) {                            // astronomically unlikely: one more legal move
    int nb = blankAt - dim;
    board[blankAt] = board[nb];
    board[nb] = 0;
    blankAt = nb;
  }
}

static void newGame() {
  dim = byDiff(3, 4, 5);
  cellPx = (dim == 3) ? 64 : (dim == 4 ? 52 : 42);
  gridY0 = PF_Y + (PF_H - dim * cellPx) / 2;
  shuffleSteps = dim * dim * 16;
  moves = 0;
  state = SL_PLAY;
  buildColours();
  shuffleBoard();
  pfClear();
  drawAll();
  drawPanel();
  hudScore(moves, "Moves");
}

// Slide the line between the blank and cell `idx` one step towards the blank. Returns true if tiles moved.
static bool slideTo(int idx) {
  if (idx == blankAt || idx < 0 || idx >= dim * dim) return false;
  int bc = blankAt % dim, br = blankAt / dim;
  int c = idx % dim, r = idx / dim;
  if (r != br && c != bc) return false;
  int step = (r == br) ? ((c > bc) ? 1 : -1) : ((r > br) ? dim : -dim);
  int pos = blankAt;
  while (pos != idx) {
    board[pos] = board[pos + step];
    pos += step;
  }
  board[idx] = 0;
  // redraw every cell from the old blank to the tapped cell
  int from = blankAt;
  blankAt = idx;
  pos = from;
  for (;;) {
    drawCell(pos);
    if (pos == idx) break;
    pos += step;
  }
  return true;
}

static void init() { newGame(); }

static void tick() {
  if (state == SL_OVER) {
    if (touch.pressed) newGame();
    return;
  }
  if (state == SL_WON) {
    if ((int32_t)(millis() - timerAt) >= 0) {
      state = SL_OVER;
      overlayGameOver(moves, " moves");
    }
    return;
  }
  if (!touch.tap) return;
  if (inRect(touch.x, touch.y, NEW_X - 6, NEW_Y - 8, NEW_W + 12, NEW_H + 12)) {
    beep(700, 30);
    newGame();
    return;
  }
  int tx = touch.x - GRID_X0, ty = touch.y - gridY0;
  int span = dim * cellPx;
  if (tx < -6 || ty < -6 || tx >= span + 6 || ty >= span + 6) return;
  tx = clampi(tx, 0, span - 1);
  ty = clampi(ty, 0, span - 1);
  int idx = (ty / cellPx) * dim + (tx / cellPx);
  if (!slideTo(idx)) {
    beep(200, 40);
    return;
  }
  moves++;
  hudScore(moves, "Moves");
  beep(620, 18);
  if (isSolved()) {
    state = SL_WON;
    timerAt = millis() + 800;
    hudText("Solved!", C_GREEN);
    beep(1200, 150);
  }
}

static void icon(int cx, int cy) {
  static const uint16_t cols[8] = {C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_CYAN, C_BLUE, C_MAGENTA, C_PINK};
  int k = 0;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      int x = cx - 17 + c * 12, y = cy - 17 + r * 12;
      if (r == 2 && c == 2) {
        tft.drawRoundRect(x, y, 11, 11, 2, C_GREY);
      } else {
        tft.fillRoundRect(x, y, 11, 11, 2, cols[k++]);
      }
    }
  }
}

}  // namespace Slide15
