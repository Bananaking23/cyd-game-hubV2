// Tetris - 10x20 board. Buttons: < > (left side, hold = auto-repeat), ROTATE and DROP (right side).
// 7-bag randomizer, wall kicks, hard drop, level speeds up every 10 lines.
#pragma once
#include "hub.h"

namespace Tetris {

static const int COLS = 10, ROWS = 20, CS = 10;
static const int BX = 110, BY = PF_Y + 9;      // board top-left (x 110..209, y 31..230)
static const int PX0 = 216;                    // right panel left edge
static const uint16_t BG = RGB565(10, 12, 20);
static const uint16_t GRID = RGB565(30, 34, 52);

// Piece order: I O T S Z J L. Cells (x,y) of rotation 0 inside a SZ x SZ box, y grows downwards.
static const int8_t SH[7][4][2] = {
  {{0, 1}, {1, 1}, {2, 1}, {3, 1}},   // I
  {{0, 0}, {1, 0}, {0, 1}, {1, 1}},   // O
  {{1, 0}, {0, 1}, {1, 1}, {2, 1}},   // T
  {{1, 0}, {2, 0}, {0, 1}, {1, 1}},   // S
  {{0, 0}, {1, 0}, {1, 1}, {2, 1}},   // Z
  {{0, 0}, {0, 1}, {1, 1}, {2, 1}},   // J
  {{2, 0}, {0, 1}, {1, 1}, {2, 1}}    // L
};
static const uint8_t SZ[7] = {4, 2, 3, 3, 3, 3, 3};
static const uint16_t COL[7] = {
  RGB565(0, 220, 230), RGB565(240, 220, 0), RGB565(170, 60, 200), RGB565(60, 200, 70),
  RGB565(225, 50, 50), RGB565(50, 90, 230), RGB565(240, 150, 30)
};
static const uint16_t HI[7] = {
  RGB565(140, 255, 255), RGB565(255, 255, 140), RGB565(220, 140, 240), RGB565(140, 240, 140),
  RGB565(255, 130, 130), RGB565(130, 160, 255), RGB565(255, 200, 120)
};
static const int8_t KICK[5] = {0, -1, 1, -2, 2};
static const uint16_t DROPMS[15] = {800, 720, 640, 560, 480, 400, 330, 270, 220, 170, 130, 100, 80, 70, 60};
static const int PTS[5] = {0, 100, 300, 500, 800};

static uint8_t board[ROWS][COLS];   // 0 empty, else piece type + 1
static uint8_t bag[7];
static int8_t cc[4][2];             // cells of the current piece (rotation applied)
static int bagIdx, type, rot, px, py, nextT, score, level, lines, state, held;
static int dropSlowPct, levelSpeedPct;   // difficulty: gravity interval scale %, how fast gravity ramps per level %
static uint32_t lastDrop, clearUntil, deadAt, nextRep, clearMask;   // state: 0 ready, 1 play, 2 over, 3 clearing

// ------------------------------------------------------------------ drawing
static void blk(int x, int y, int s, int t) {
  tft.fillRect(x, y, s, s, COL[t]);
  tft.drawRect(x, y, s, s, C_BLACK);
  tft.drawFastHLine(x + 1, y + 1, s - 3, HI[t]);
  tft.drawFastVLine(x + 1, y + 1, s - 3, HI[t]);
}
static void cellDraw(int cx, int cy, int t) { blk(BX + cx * CS, BY + cy * CS, CS, t); }
static void cellErase(int cx, int cy) {
  tft.fillRect(BX + cx * CS, BY + cy * CS, CS, CS, BG);
  tft.drawPixel(BX + cx * CS, BY + cy * CS, GRID);
}
static void drawBoardAll() {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (board[r][c]) cellDraw(c, r, board[r][c] - 1);
      else cellErase(c, r);
    }
  }
}

static void calcCells(int t, int r, int8_t out[4][2]) {
  int sz = SZ[t];
  for (int i = 0; i < 4; i++) {
    int x = SH[t][i][0], y = SH[t][i][1];
    for (int k = 0; k < r; k++) {   // rotate clockwise
      int nx = sz - 1 - y;
      int ny = x;
      x = nx;
      y = ny;
    }
    out[i][0] = (int8_t)x;
    out[i][1] = (int8_t)y;
  }
}

static bool fits(int8_t c[4][2], int x0, int y0) {
  for (int i = 0; i < 4; i++) {
    int x = x0 + c[i][0], y = y0 + c[i][1];
    if (x < 0 || x >= COLS || y >= ROWS) return false;
    if (y >= 0 && board[y][x]) return false;
  }
  return true;
}

static void paintPiece(bool on) {
  for (int i = 0; i < 4; i++) {
    int x = px + cc[i][0], y = py + cc[i][1];
    if (y < 0 || y >= ROWS || x < 0 || x >= COLS) continue;
    if (on) cellDraw(x, y, type);
    else cellErase(x, y);
  }
}

static void drawStats() {
  char b[20];
  snprintf(b, sizeof b, "Level %d", level);
  textBox(b, PX0 + 50, 98, 96, 16, 2, C_WHITE, C_BLACK);
  snprintf(b, sizeof b, "Lines %d", lines);
  textBox(b, PX0 + 50, 116, 96, 16, 2, C_LGREY, C_BLACK);
}

static void drawPreview() {
  tft.fillRect(PX0 + 5, 45, 90, 38, C_BLACK);
  int minx = 9, maxx = 0, miny = 9, maxy = 0;
  for (int i = 0; i < 4; i++) {
    minx = imin(minx, SH[nextT][i][0]);
    maxx = imax(maxx, SH[nextT][i][0]);
    miny = imin(miny, SH[nextT][i][1]);
    maxy = imax(maxy, SH[nextT][i][1]);
  }
  int w = (maxx - minx + 1) * 8, h = (maxy - miny + 1) * 8;
  int ox = PX0 + 5 + (90 - w) / 2 - minx * 8;
  int oy = 45 + (38 - h) / 2 - miny * 8;
  for (int i = 0; i < 4; i++) {
    blk(ox + SH[nextT][i][0] * 8, oy + SH[nextT][i][1] * 8, 8, nextT);
  }
}

// ------------------------------------------------------------------ game logic
static int drawBag() {
  if (bagIdx >= 7) {
    for (int i = 0; i < 7; i++) bag[i] = (uint8_t)i;
    for (int i = 6; i > 0; i--) {
      int j = rnd(i + 1);
      uint8_t t = bag[i];
      bag[i] = bag[j];
      bag[j] = t;
    }
    bagIdx = 0;
  }
  return bag[bagIdx++];
}

static void die() {
  state = 2;
  deadAt = millis();
  beep(150, 400);
  overlayGameOver(score);
}

static void spawn() {
  type = nextT;
  nextT = drawBag();
  rot = 0;
  px = (COLS - SZ[type]) / 2;
  py = (type == 0) ? -1 : 0;
  calcCells(type, 0, cc);
  drawPreview();
  if (!fits(cc, px, py)) {   // no room for the new piece
    die();
    return;
  }
  paintPiece(true);
  lastDrop = millis();
}

static bool rowFull(int r) {
  for (int c = 0; c < COLS; c++) {
    if (!board[r][c]) return false;
  }
  return true;
}

static void removeRows() {
  int w = ROWS - 1;
  for (int r = ROWS - 1; r >= 0; r--) {
    if (rowFull(r)) continue;
    if (w != r) memcpy(board[w], board[r], COLS);
    w--;
  }
  for (; w >= 0; w--) memset(board[w], 0, COLS);
}

static void lockPiece() {
  for (int i = 0; i < 4; i++) {
    int x = px + cc[i][0], y = py + cc[i][1];
    if (y >= 0 && y < ROWS && x >= 0 && x < COLS) board[y][x] = (uint8_t)(type + 1);
  }
  clearMask = 0;
  int n = 0;
  for (int r = 0; r < ROWS; r++) {
    if (rowFull(r)) {
      clearMask |= (1UL << r);
      n++;
    }
  }
  if (n > 0) {
    score += PTS[n] * level;
    lines += n;
    int nl = 1 + lines / 10;
    bool up = nl > level;
    level = nl;
    for (int r = 0; r < ROWS; r++) {
      if (clearMask & (1UL << r)) tft.fillRect(BX, BY + r * CS, COLS * CS, CS, C_WHITE);
    }
    hudScore(score);
    drawStats();
    beep(up ? 1300 : 850, 90);
    state = 3;
    clearUntil = millis() + 160;
  } else {
    beep(220, 25);
    spawn();
  }
}

static void shift(int d) {
  if (!fits(cc, px + d, py)) return;
  paintPiece(false);
  px += d;
  paintPiece(true);
}

static void rotate() {
  if (type == 1) return;   // O never changes
  int nr = (rot + 1) & 3;
  int8_t nc[4][2];
  calcCells(type, nr, nc);
  for (int k = 0; k < 5; k++) {
    if (fits(nc, px + KICK[k], py)) {
      paintPiece(false);
      rot = nr;
      px += KICK[k];
      for (int i = 0; i < 4; i++) {
        cc[i][0] = nc[i][0];
        cc[i][1] = nc[i][1];
      }
      paintPiece(true);
      beep(900, 20);
      return;
    }
  }
}

static void hardDrop() {
  int rows = 0;
  paintPiece(false);
  while (fits(cc, px, py + 1)) {
    py++;
    rows++;
  }
  paintPiece(true);
  score += 2 * rows;
  hudScore(score);
  lockPiece();
}

static void reset() {
  dropSlowPct = byDiff(135, 100, 75);
  levelSpeedPct = byDiff(75, 100, 125);
  hudBegin("Tetris");
  memset(board, 0, sizeof board);
  score = 0;
  level = 1;
  lines = 0;
  held = 0;
  state = 0;
  bagIdx = 7;
  nextT = drawBag();
  clearMask = 0;
  tft.drawRect(BX - 1, BY - 1, COLS * CS + 2, ROWS * CS + 2, RGB565(90, 100, 130));
  drawBoardAll();
  button(3, 115, 50, 110, "<", RGB565(40, 80, 170), C_WHITE, 4);
  button(57, 115, 50, 110, ">", RGB565(40, 80, 170), C_WHITE, 4);
  text("NEXT", PX0 + 50, 34, 2, C_LGREY, MC_DATUM);
  tft.drawRect(PX0 + 4, 44, 92, 40, RGB565(60, 66, 90));
  drawPreview();
  drawStats();
  button(PX0 + 4, 128, 92, 50, "ROTATE", RGB565(30, 140, 70));
  button(PX0 + 4, 184, 92, 50, "DROP", RGB565(190, 90, 30));
  hudScore(0);
  text("TAP TO", BX + 50, BY + 85, 4, C_WHITE, MC_DATUM);
  text("START", BX + 50, BY + 115, 4, C_YELLOW, MC_DATUM);
}

static void init() { reset(); }

// 0 none, 1 left, 2 right, 3 rotate, 4 drop
static int zoneAt(int x, int y) {
  if (x >= 212) {
    if (y >= 181) return 4;
    if (y >= 124) return 3;
    return 0;
  }
  if (x < 109 && y >= 100) return (x < 56) ? 1 : 2;
  return 0;
}

static void tick() {
  uint32_t now = millis();
  if (state == 2) {
    if (touch.pressed && touch.y >= PF_Y && now - deadAt >= 600) reset();
    return;
  }
  if (state == 0) {
    if (touch.pressed && touch.y >= PF_Y) {
      state = 1;
      drawBoardAll();
      spawn();
    }
    return;
  }
  if (state == 3) {
    if ((int32_t)(now - clearUntil) >= 0) {
      removeRows();
      drawBoardAll();
      state = 1;
      spawn();
    }
    return;
  }

  // ---- input
  if (!touch.down) held = 0;
  if (touch.pressed && touch.y >= PF_Y) {
    int z = zoneAt(touch.x, touch.y);
    if (z == 1 || z == 2) {
      held = z;
      nextRep = now + 150;
      shift(z == 1 ? -1 : 1);
    } else if (z == 3) {
      rotate();
    } else if (z == 4) {
      hardDrop();
    }
  } else if (held != 0 && touch.down && (int32_t)(now - nextRep) >= 0) {
    nextRep = now + 70;
    shift(held == 1 ? -1 : 1);
  }
  if (state != 1) return;

  // ---- gravity
  int dm = DROPMS[imin((level - 1) * levelSpeedPct / 100, 14)] * dropSlowPct / 100;
  if ((int32_t)(now - lastDrop) >= dm) {
    lastDrop = now;
    if (fits(cc, px, py + 1)) {
      paintPiece(false);
      py++;
      paintPiece(true);
    } else {
      lockPiece();
    }
  }
}

static void ib(int x, int y, uint16_t c) {
  tft.fillRect(x, y, 8, 8, c);
  tft.drawRect(x, y, 8, 8, C_BLACK);
}

static void icon(int cx, int cy) {
  ib(cx - 16, cy - 16, C_YELLOW);
  ib(cx - 8, cy - 16, C_YELLOW);
  ib(cx - 16, cy - 8, C_YELLOW);
  ib(cx - 8, cy - 8, C_YELLOW);
  ib(cx + 8, cy - 16, C_CYAN);
  ib(cx + 8, cy - 8, C_CYAN);
  ib(cx, cy - 8, C_MAGENTA);
  ib(cx - 8, cy, C_MAGENTA);
  ib(cx, cy, C_MAGENTA);
  ib(cx + 8, cy, C_MAGENTA);
  ib(cx - 16, cy + 8, C_GREEN);
  ib(cx - 8, cy + 8, C_GREEN);
  ib(cx, cy + 8, C_ORANGE);
  ib(cx + 8, cy + 8, C_ORANGE);
}

}  // namespace Tetris
