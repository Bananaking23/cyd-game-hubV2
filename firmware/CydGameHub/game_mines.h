// Minesweeper - 12x8 field, 10 / 14 / 20 mines (easy / normal / hard). Tap = dig, hold (or FLAG button) = flag. First dig is always safe.
#pragma once
#include "hub.h"

namespace Mines {

static const int COLS = 12, ROWS = 8, CS = 24, NCELLS = COLS * ROWS;
static const int OX = (SCR_W - COLS * CS) / 2, OY = PF_Y + 1;       // grid x 16..303, y 23..214
static const int BAR_Y = OY + ROWS * CS + 1;                        // bottom bar y 216..239
static const int FBX = 6, FBY = BAR_Y + 1, FBW = 84, FBH = 22;      // FLAG toggle button

static const uint16_t BAR_BG    = HUD_BG;
static const uint16_t HID_FILL  = RGB565(104, 114, 138);
static const uint16_t HID_LIGHT = RGB565(160, 170, 195);
static const uint16_t HID_DARK  = RGB565(60, 66, 84);
static const uint16_t REV_FILL  = RGB565(34, 38, 50);
static const uint16_t GRID_LINE = RGB565(14, 16, 24);
static const uint16_t NUMCOL[9] = {
  0, RGB565(90, 150, 255), RGB565(70, 205, 90), RGB565(255, 85, 85), RGB565(180, 120, 255),
  RGB565(220, 120, 60), RGB565(50, 205, 205), C_WHITE, C_GREY
};

// cell bits: 1 = mine, 2 = revealed, 4 = flagged, high nibble = adjacent mine count
static const uint8_t F_MINE = 1, F_REV = 2, F_FLAG = 4;
static uint8_t cell[ROWS][COLS];
static uint8_t q[NCELLS];            // flood-fill queue: every cell is enqueued at most once
static int state, revealed, flags, shownSecs, winSecs, hitR, hitC;
// state: 0 ready (no mines yet), 1 playing, 2 lost (overlay pending), 3 won (overlay pending), 4 over (wait tap)
static int mineCount;                // 10 / 14 / 20 by difficulty (<= NCELLS - 9, arrays are sized by NCELLS)
static bool flagMode;
static uint32_t tStart, tEnd;

static inline int adjOf(int r, int c) { return cell[r][c] >> 4; }

static void drawMine(int x, int y) {
  int cx = x + CS / 2, cy = y + CS / 2;
  tft.drawLine(cx - 9, cy, cx + 9, cy, C_LGREY);
  tft.drawLine(cx, cy - 9, cx, cy + 9, C_LGREY);
  tft.drawLine(cx - 6, cy - 6, cx + 6, cy + 6, C_LGREY);
  tft.drawLine(cx - 6, cy + 6, cx + 6, cy - 6, C_LGREY);
  tft.fillCircle(cx, cy, 6, C_LGREY);
  tft.fillRect(cx - 3, cy - 3, 2, 2, C_WHITE);
}

static void drawFlag(int x, int y) {
  tft.fillTriangle(x + 11, y + 4, x + 11, y + 13, x + 19, y + 8, C_RED);
  tft.drawFastVLine(x + 11, y + 4, 16, C_WHITE);
  tft.fillRect(x + 7, y + 19, 9, 2, C_WHITE);
}

static void drawHidden(int x, int y) {
  tft.fillRect(x, y, CS, CS, GRID_LINE);
  tft.fillRect(x + 1, y + 1, CS - 2, CS - 2, HID_FILL);
  tft.drawFastHLine(x + 1, y + 1, CS - 2, HID_LIGHT);
  tft.drawFastVLine(x + 1, y + 1, CS - 2, HID_LIGHT);
  tft.drawFastHLine(x + 1, y + CS - 2, CS - 2, HID_DARK);
  tft.drawFastVLine(x + CS - 2, y + 1, CS - 2, HID_DARK);
}

static void drawOpen(int x, int y, uint16_t bg) {
  tft.fillRect(x, y, CS, CS, GRID_LINE);
  tft.fillRect(x + 1, y + 1, CS - 2, CS - 2, bg);
}

// Normal in-game appearance of one cell.
static void drawCell(int r, int c) {
  int x = OX + c * CS, y = OY + r * CS;
  uint8_t v = cell[r][c];
  if (v & F_REV) {
    drawOpen(x, y, REV_FILL);
    int n = v >> 4;
    if (n > 0) {
      char b[2] = {(char)('0' + n), 0};
      text(b, x + CS / 2, y + CS / 2 + 1, 2, NUMCOL[n], MC_DATUM);
    }
  } else {
    drawHidden(x, y);
    if (v & F_FLAG) drawFlag(x, y);
  }
}

static void drawBtn() {
  button(FBX, FBY, FBW, FBH, "FLAG", flagMode ? C_ORANGE : RGB565(50, 60, 96), flagMode ? C_BLACK : C_WHITE);
}
static void drawMinesLeft() {
  char b[16];
  snprintf(b, sizeof b, "Mines: %d", mineCount - flags);
  textBox(b, 150, BAR_Y + 12, 92, 20, 2, C_YELLOW, BAR_BG);
}
static void drawTime(int s) {
  char b[16];
  snprintf(b, sizeof b, "Time: %d", s);
  textBox(b, 254, BAR_Y + 12, 100, 20, 2, C_CYAN, BAR_BG);
}

static void reset() {
  mineCount = byDiff(10, 14, 20);
  hudBegin("Mines");
  hudText("Hold = flag", C_LGREY);
  pfClear(C_BLACK);
  memset(cell, 0, sizeof cell);
  state = 0;
  revealed = 0;
  flags = 0;
  shownSecs = 0;
  winSecs = 0;
  flagMode = false;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) drawCell(r, c);
  tft.fillRect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, BAR_BG);
  drawBtn();
  drawMinesLeft();
  drawTime(0);
}

// Place mines once the first dig is known: the dug cell and its neighbours stay clear.
static void placeMines(int fr, int fc) {
  int placed = 0;
  while (placed < mineCount) {
    int r = rnd(ROWS), c = rnd(COLS);
    if (iabs(r - fr) <= 1 && iabs(c - fc) <= 1) continue;
    if (cell[r][c] & F_MINE) continue;
    cell[r][c] |= F_MINE;
    placed++;
  }
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      int n = 0;
      for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
          int nr = r + dr, nc = c + dc;
          if ((dr == 0 && dc == 0) || nr < 0 || nc < 0 || nr >= ROWS || nc >= COLS) continue;
          if (cell[nr][nc] & F_MINE) n++;
        }
      cell[r][c] = (uint8_t)((cell[r][c] & 0x0F) | (n << 4));
    }
}

// Reveal a (non-mine, unflagged) cell; zeros flood outwards via an explicit queue.
static void revealCell(int r, int c) {
  if (cell[r][c] & (F_REV | F_FLAG | F_MINE)) return;
  int qh = 0, qt = 0;
  cell[r][c] |= F_REV;
  revealed++;
  drawCell(r, c);
  if (adjOf(r, c) == 0) q[qt++] = (uint8_t)(r * COLS + c);
  while (qh < qt) {
    int idx = q[qh++];
    int r0 = idx / COLS, c0 = idx % COLS;
    for (int dr = -1; dr <= 1; dr++)
      for (int dc = -1; dc <= 1; dc++) {
        int nr = r0 + dr, nc = c0 + dc;
        if (nr < 0 || nc < 0 || nr >= ROWS || nc >= COLS) continue;
        if (cell[nr][nc] & (F_REV | F_FLAG | F_MINE)) continue;
        cell[nr][nc] |= F_REV;
        revealed++;
        drawCell(nr, nc);
        if (adjOf(nr, nc) == 0 && qt < NCELLS) q[qt++] = (uint8_t)(nr * COLS + nc);
      }
  }
}

static void toggleFlag(int r, int c) {
  if (cell[r][c] & F_REV) return;
  cell[r][c] ^= F_FLAG;
  flags += (cell[r][c] & F_FLAG) ? 1 : -1;
  drawCell(r, c);
  drawMinesLeft();
  beep((cell[r][c] & F_FLAG) ? 1000 : 700, 30);
}

static void lose(int r, int c) {
  state = 2;
  hitR = r;
  hitC = c;
  tEnd = millis();
  for (int rr = 0; rr < ROWS; rr++)
    for (int cc = 0; cc < COLS; cc++) {
      uint8_t v = cell[rr][cc];
      int x = OX + cc * CS, y = OY + rr * CS;
      if ((v & F_MINE) && !(v & F_FLAG)) {
        drawOpen(x, y, (rr == r && cc == c) ? C_RED : REV_FILL);
        drawMine(x, y);
      } else if (!(v & F_MINE) && (v & F_FLAG)) {          // wrong flag
        drawOpen(x, y, REV_FILL);
        drawMine(x, y);
        tft.drawLine(x + 3, y + 3, x + CS - 4, y + CS - 4, C_RED);
        tft.drawLine(x + 3, y + CS - 4, x + CS - 4, y + 3, C_RED);
      }
    }
  beep(160, 400);
}

static void win() {
  state = 3;
  tEnd = millis();
  winSecs = imax(1, (int)((tEnd - tStart) / 1000));
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if ((cell[r][c] & F_MINE) && !(cell[r][c] & F_FLAG)) {
        cell[r][c] |= F_FLAG;
        drawCell(r, c);
      }
  flags = mineCount;
  drawMinesLeft();
  drawTime(winSecs);
  beep(1250, 250);
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (millis() - tEnd >= 1000) {
      char l1[28];
      snprintf(l1, sizeof l1, "Cleared %d of %d", revealed, NCELLS - mineCount);
      overlayMsg("BOOM!", C_RED, "You hit a mine", l1, "Tap to try again");
      state = 4;
      tEnd = millis();
    }
    return;
  }
  if (state == 3) {
    if (millis() - tEnd >= 600) {
      overlayGameOver(winSecs, " s");
      state = 4;
      tEnd = millis();
    }
    return;
  }
  if (state == 4) {
    if (touch.pressed && touch.y >= PF_Y && millis() - tEnd > 500) reset();
    return;
  }

  if (state == 1) {                                    // elapsed time
    int s = imin(999, (int)((millis() - tStart) / 1000));
    if (s != shownSecs) {
      shownSecs = s;
      drawTime(s);
    }
  }

  if (!touch.released || !touch.tap) return;
  int x = touch.sx, y = touch.sy;

  if (inRect(x, y, 0, BAR_Y, FBX + FBW + 12, SCR_H - BAR_Y)) {   // FLAG toggle
    flagMode = !flagMode;
    drawBtn();
    beep(flagMode ? 900 : 600, 30);
    return;
  }
  if (!inRect(x, y, OX, OY, COLS * CS, ROWS * CS)) return;

  int c = clampi((x - OX) / CS, 0, COLS - 1);
  int r = clampi((y - OY) / CS, 0, ROWS - 1);
  bool longPress = touch.dur >= 400;
  bool doFlag = (longPress != flagMode);               // flag mode swaps tap and hold
  uint8_t v = cell[r][c];

  if (doFlag) {
    toggleFlag(r, c);
    return;
  }
  if (v & (F_REV | F_FLAG)) return;                    // dig on opened / flagged cell: ignore

  if (state == 0) {                                    // first dig: place mines now
    placeMines(r, c);
    state = 1;
    tStart = millis();
  }
  if (cell[r][c] & F_MINE) {
    lose(r, c);
    return;
  }
  revealCell(r, c);
  beep(1100, 15);
  if (revealed == NCELLS - mineCount) win();
}

static void icon(int cx, int cy) {
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) {
      int x = cx - 18 + c * 12, y = cy - 18 + r * 12;
      bool open = (r == 1 && c == 1);
      tft.fillRect(x + 1, y + 1, 10, 10, open ? REV_FILL : HID_FILL);
    }
  text("1", cx, cy + 1, 1, NUMCOL[1], MC_DATUM);
  tft.fillTriangle(cx + 9, cy - 15, cx + 9, cy - 9, cx + 15, cy - 12, C_RED);
  tft.drawFastVLine(cx + 9, cy - 15, 10, C_WHITE);
  tft.fillCircle(cx - 12, cy + 12, 4, C_BLACK);
  tft.drawCircle(cx - 12, cy + 12, 4, C_WHITE);
}

}  // namespace Mines
