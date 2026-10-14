// Sudoku - 9x9. Tap a cell, then tap a number to fill it. Fewer seconds = better. Every puzzle has exactly one solution.
#pragma once
#include "hub.h"

namespace Sudoku {

static const int SUDOKU_CELL_PX   = 23;                 // board pitch: 9 * 23 = 207
static const int SUDOKU_BOARD_X   = 4;
static const int SUDOKU_BOARD_Y   = PF_Y + 5;           // board spans y 27..233
static const int SUDOKU_BOARD_PX  = 9 * SUDOKU_CELL_PX;
static const int SUDOKU_PAD_X     = 218;                // numpad of 3x3 buttons, pitch 34
static const int SUDOKU_PAD_Y     = PF_Y + 5;
static const int SUDOKU_PAD_BTN   = 32;
static const int SUDOKU_PAD_PITCH = 34;
static const int SUDOKU_WIDE_X    = 218;                // ERASE / NEW buttons
static const int SUDOKU_WIDE_W    = 100;
static const int SUDOKU_WIDE_H    = 28;
static const int SUDOKU_ERASE_Y   = 130;
static const int SUDOKU_NEW_Y     = 162;
static const int SUDOKU_TIME_Y    = 204;
static const int SUDOKU_DIFF_Y    = 227;
static const int SUDOKU_PANEL_MID = 268;

static const int  SUDOKU_REMOVE_BUDGET = 6000;          // max search nodes per uniqueness check
static const int  SUDOKU_FILL_BUDGET   = 20000;         // max search nodes for the random full grid
static const uint16_t SUDOKU_ALL_DIGITS = 0x1FF;

static const uint16_t SUDOKU_COL_BG       = RGB565(22, 26, 38);
static const uint16_t SUDOKU_COL_GIVEN_BG = RGB565(38, 42, 58);
static const uint16_t SUDOKU_COL_PEER_BG  = RGB565(34, 44, 70);
static const uint16_t SUDOKU_COL_SAME_BG  = RGB565(60, 84, 140);
static const uint16_t SUDOKU_COL_SEL_BG   = RGB565(30, 90, 170);
static const uint16_t SUDOKU_COL_THIN     = RGB565(48, 54, 74);
static const uint16_t SUDOKU_COL_THICK    = RGB565(170, 180, 210);
static const uint16_t SUDOKU_COL_PLAYER   = C_CYAN;
static const uint16_t SUDOKU_COL_BAD      = RGB565(255, 70, 70);
static const uint16_t SUDOKU_COL_PAD      = RGB565(44, 64, 120);
static const uint16_t SUDOKU_COL_PAD_DONE = RGB565(30, 34, 46);

// ------------------------------------------------------------------ state
static uint8_t  gridVal[81];        // 0 = empty, 1..9
static uint8_t  gridGiven[81];      // 1 = part of the puzzle (locked)
static uint16_t cellShadow[81];     // what is currently drawn for each cell (0xFFFF = unknown)
static uint8_t  padDoneShadow[10];  // 1 = number button drawn as "all nine placed"
static int  selCell, state, shownSecs, winSecs;   // state: 0 playing, 1 won (overlay pending), 2 won (wait tap)
static uint32_t tStart, tEnd;

// ------------------------------------------------------------------ solver (iterative, no recursion)
static uint16_t rowUsed[9], colUsed[9], boxUsed[9];
static uint8_t  frameCell[81];
static uint16_t frameCand[81];
static uint8_t  framePlaced[81];

static inline int rowOf(int idx) { return idx / 9; }
static inline int colOf(int idx) { return idx % 9; }
static inline int boxOf(int idx) { return (idx / 27) * 3 + (idx % 9) / 3; }

static inline int bitCount(uint16_t mask) {
  int n = 0;
  while (mask) { mask = (uint16_t)(mask & (mask - 1)); n++; }
  return n;
}

static inline void placeDigit(uint8_t* board, int idx, int digit) {
  uint16_t bit = (uint16_t)(1u << (digit - 1));
  board[idx] = (uint8_t)digit;
  rowUsed[rowOf(idx)] |= bit;
  colUsed[colOf(idx)] |= bit;
  boxUsed[boxOf(idx)] |= bit;
}

static inline void unplaceDigit(uint8_t* board, int idx, int digit) {
  uint16_t bit = (uint16_t)(1u << (digit - 1));
  board[idx] = 0;
  rowUsed[rowOf(idx)] &= (uint16_t)~bit;
  colUsed[colOf(idx)] &= (uint16_t)~bit;
  boxUsed[boxOf(idx)] &= (uint16_t)~bit;
}

static void unwindFrames(uint8_t* board, int depth) {
  for (int i = depth - 1; i >= 0; i--) {
    if (framePlaced[i]) {
      unplaceDigit(board, frameCell[i], framePlaced[i]);
      framePlaced[i] = 0;
    }
  }
}

// Count solutions of `board` up to `limit` (MRV heuristic, bitmask candidates).
//  randomize: try candidates in random order (used to build a random full grid)
//  keepFirst: stop at the first solution and leave it in `board` (otherwise `board` is restored)
// Returns the number of solutions found (<= limit), or -1 if `budget` search nodes were not enough.
static int solverRun(uint8_t* board, int limit, bool randomize, bool keepFirst, int budget) {
  for (int i = 0; i < 9; i++) { rowUsed[i] = 0; colUsed[i] = 0; boxUsed[i] = 0; }
  for (int i = 0; i < 81; i++) {
    if (!board[i]) continue;
    uint16_t bit = (uint16_t)(1u << (board[i] - 1));
    if ((rowUsed[rowOf(i)] & bit) || (colUsed[colOf(i)] & bit) || (boxUsed[boxOf(i)] & bit)) return 0;
    rowUsed[rowOf(i)] |= bit;
    colUsed[colOf(i)] |= bit;
    boxUsed[boxOf(i)] |= bit;
  }
  int depth = 0, count = 0, nodes = 0;
  bool descend = true;
  while (true) {
    if (descend) {
      int bestCell = -1, bestCount = 10;
      uint16_t bestMask = 0;
      for (int i = 0; i < 81; i++) {
        if (board[i]) continue;
        uint16_t mask = (uint16_t)(~(rowUsed[rowOf(i)] | colUsed[colOf(i)] | boxUsed[boxOf(i)]) & SUDOKU_ALL_DIGITS);
        int n = bitCount(mask);
        if (n < bestCount) {
          bestCount = n;
          bestCell = i;
          bestMask = mask;
          if (n <= 1) break;
        }
      }
      if (bestCell < 0) {                       // board is full: one more solution
        count++;
        if (keepFirst) return count;
        if (count >= limit) { unwindFrames(board, depth); return count; }
      } else {
        frameCell[depth] = (uint8_t)bestCell;
        frameCand[depth] = bestMask;
        framePlaced[depth] = 0;
        depth++;
      }
    }
    // advance the top frame to its next candidate (popping exhausted frames)
    bool advanced = false;
    while (depth > 0) {
      int top = depth - 1;
      if (framePlaced[top]) {
        unplaceDigit(board, frameCell[top], framePlaced[top]);
        framePlaced[top] = 0;
      }
      uint16_t cand = frameCand[top];
      if (cand == 0) { depth--; continue; }
      if (++nodes > budget) { unwindFrames(board, depth); return -1; }
      int digit = 0;
      if (randomize) {
        int skip = rnd(bitCount(cand));
        for (int d = 1; d <= 9; d++) {
          if (cand & (1u << (d - 1))) {
            if (skip == 0) { digit = d; break; }
            skip--;
          }
        }
      } else {
        for (int d = 1; d <= 9; d++) {
          if (cand & (1u << (d - 1))) { digit = d; break; }
        }
      }
      frameCand[top] = (uint16_t)(cand & ~(1u << (digit - 1)));
      placeDigit(board, frameCell[top], digit);
      framePlaced[top] = (uint8_t)digit;
      advanced = true;
      break;
    }
    if (!advanced) return count;                // search space exhausted
    descend = true;
  }
}

// Fill `board` with a random valid full grid.
static void randomFullGrid(uint8_t* board) {
  for (int attempt = 0; attempt < 4; attempt++) {
    memset(board, 0, 81);
    if (solverRun(board, 1, true, true, SUDOKU_FILL_BUDGET) == 1) return;
  }
  // guaranteed-valid fallback: shifted pattern with shuffled digits
  uint8_t perm[9];
  for (int i = 0; i < 9; i++) perm[i] = (uint8_t)(i + 1);
  for (int i = 8; i > 0; i--) {
    int j = rnd(i + 1);
    uint8_t tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
  }
  for (int r = 0; r < 9; r++)
    for (int c = 0; c < 9; c++) board[r * 9 + c] = perm[(r * 3 + r / 3 + c) % 9];
}

// Build a puzzle with a unique solution; returns the number of clues left.
static int generatePuzzle(uint8_t* board, int targetClues) {
  randomFullGrid(board);
  uint8_t order[81];
  for (int i = 0; i < 81; i++) order[i] = (uint8_t)i;
  for (int i = 80; i > 0; i--) {
    int j = rnd(i + 1);
    uint8_t tmp = order[i]; order[i] = order[j]; order[j] = tmp;
  }
  int clues = 81;
  for (int k = 0; k < 81 && clues > targetClues; k++) {
    int idx = order[k];
    uint8_t keep = board[idx];
    board[idx] = 0;
    if (solverRun(board, 2, false, false, SUDOKU_REMOVE_BUDGET) == 1) clues--;   // still unique: leave it removed
    else board[idx] = keep;                                                       // 0, 2+ solutions or out of budget
  }
  return clues;
}

// ------------------------------------------------------------------ rules for the player's grid
static inline bool isPeer(int cellA, int cellB) {
  return rowOf(cellA) == rowOf(cellB) || colOf(cellA) == colOf(cellB) || boxOf(cellA) == boxOf(cellB);
}

static bool hasConflict(int idx) {
  int digit = gridVal[idx];
  if (!digit) return false;
  for (int j = 0; j < 81; j++)
    if (j != idx && gridVal[j] == digit && isPeer(idx, j)) return true;
  return false;
}

static int countDigit(int digit) {
  int n = 0;
  for (int i = 0; i < 81; i++) n += (gridVal[i] == digit) ? 1 : 0;
  return n;
}

static bool isSolved() {
  for (int i = 0; i < 81; i++)
    if (!gridVal[i] || hasConflict(i)) return false;
  return true;
}

// ------------------------------------------------------------------ drawing
static void drawGridLines() {
  tft.fillRect(SUDOKU_BOARD_X, SUDOKU_BOARD_Y, SUDOKU_BOARD_PX, SUDOKU_BOARD_PX, SUDOKU_COL_THIN);
  for (int k = 0; k <= 9; k += 3) {
    int pos = k * SUDOKU_CELL_PX;
    int lo = (k == 0) ? 0 : pos - 1;
    int hi = (k == 9) ? pos - 1 : pos;                     // 2 px wide in the middle, 1 px on the outer edge
    tft.fillRect(SUDOKU_BOARD_X + lo, SUDOKU_BOARD_Y, hi - lo + 1, SUDOKU_BOARD_PX, SUDOKU_COL_THICK);
    tft.fillRect(SUDOKU_BOARD_X, SUDOKU_BOARD_Y + lo, SUDOKU_BOARD_PX, hi - lo + 1, SUDOKU_COL_THICK);
  }
}

static uint16_t cellKey(int idx) {
  uint16_t key = gridVal[idx];
  if (gridGiven[idx]) key |= 0x10;
  if (hasConflict(idx)) key |= 0x20;
  if (idx == selCell) key |= 0x40;
  else if (selCell >= 0) {
    if (gridVal[selCell] && gridVal[idx] == gridVal[selCell]) key |= 0x80;
    else if (isPeer(idx, selCell)) key |= 0x100;
  }
  return key;
}

static void drawCell(int idx, uint16_t key) {
  int ix = SUDOKU_BOARD_X + colOf(idx) * SUDOKU_CELL_PX + 1;
  int iy = SUDOKU_BOARD_Y + rowOf(idx) * SUDOKU_CELL_PX + 1;
  uint16_t bg = (key & 0x10) ? SUDOKU_COL_GIVEN_BG : SUDOKU_COL_BG;
  if (key & 0x40) bg = SUDOKU_COL_SEL_BG;
  else if (key & 0x80) bg = SUDOKU_COL_SAME_BG;
  else if (key & 0x100) bg = SUDOKU_COL_PEER_BG;
  tft.fillRect(ix, iy, 21, 21, bg);
  if (key & 0x40) tft.drawRect(ix, iy, 21, 21, C_YELLOW);
  int digit = key & 0x0F;
  if (digit) {
    char label[2] = {(char)('0' + digit), 0};
    uint16_t fg = (key & 0x20) ? SUDOKU_COL_BAD : ((key & 0x10) ? C_WHITE : SUDOKU_COL_PLAYER);
    text(label, ix + 10, iy + 11, 2, fg, MC_DATUM);
    if (key & 0x10) text(label, ix + 11, iy + 11, 2, fg, MC_DATUM);     // faux bold for the givens
  }
}

static void drawPadButton(int digit, bool done) {
  int col = (digit - 1) % 3, row = (digit - 1) / 3;
  char label[2] = {(char)('0' + digit), 0};
  button(SUDOKU_PAD_X + col * SUDOKU_PAD_PITCH, SUDOKU_PAD_Y + row * SUDOKU_PAD_PITCH, SUDOKU_PAD_BTN, SUDOKU_PAD_BTN, label,
         done ? SUDOKU_COL_PAD_DONE : SUDOKU_COL_PAD, done ? C_GREY : C_WHITE);
  padDoneShadow[digit] = done ? 1 : 0;
}

// Redraw whatever differs from what is on screen.
static void refresh() {
  for (int i = 0; i < 81; i++) {
    uint16_t key = cellKey(i);
    if (key != cellShadow[i]) {
      cellShadow[i] = key;
      drawCell(i, key);
    }
  }
  for (int d = 1; d <= 9; d++) {
    uint8_t done = (countDigit(d) >= 9) ? 1 : 0;
    if (done != padDoneShadow[d]) drawPadButton(d, done != 0);
  }
}

static void drawTime(int secs) {
  char label[24];
  snprintf(label, sizeof label, "%02d:%02d", secs / 60, secs % 60);
  textBox(label, SUDOKU_PANEL_MID, SUDOKU_TIME_Y, 100, 26, 4, C_YELLOW, C_BLACK);
}

static void drawPanel() {
  for (int d = 1; d <= 9; d++) drawPadButton(d, false);
  button(SUDOKU_WIDE_X, SUDOKU_ERASE_Y, SUDOKU_WIDE_W, SUDOKU_WIDE_H, "ERASE", RGB565(120, 60, 40));
  button(SUDOKU_WIDE_X, SUDOKU_NEW_Y, SUDOKU_WIDE_W, SUDOKU_WIDE_H, "NEW", RGB565(30, 80, 170));
  text(DIFF_NAMES[gDiff], SUDOKU_PANEL_MID, SUDOKU_DIFF_Y, 2, C_ORANGE, MC_DATUM);
  drawTime(0);
}

static void newPuzzle() {
  hudBegin("Sudoku");
  pfClear(C_BLACK);
  text("Generating...", SCR_W / 2, PF_Y + PF_H / 2, 4, C_WHITE, MC_DATUM);
  memset(gridVal, 0, sizeof gridVal);
  generatePuzzle(gridVal, byDiff(40, 32, 26));
  for (int i = 0; i < 81; i++) gridGiven[i] = gridVal[i] ? 1 : 0;
  selCell = 0;
  for (int i = 0; i < 81; i++)
    if (!gridVal[i]) { selCell = i; break; }
  for (int i = 0; i < 81; i++) cellShadow[i] = 0xFFFF;
  for (int d = 0; d < 10; d++) padDoneShadow[d] = 0;
  state = 0;
  shownSecs = 0;
  winSecs = 0;
  pfClear(C_BLACK);
  drawGridLines();
  drawPanel();
  refresh();
  tStart = millis();
}

static void win() {
  state = 1;
  tEnd = millis();
  winSecs = imax(1, (int)((tEnd - tStart) / 1000));
  drawTime(imin(5999, winSecs));
  beep(1250, 250);
}

static void placeNumber(int digit) {
  if (selCell < 0 || gridGiven[selCell]) { beep(180, 40); return; }
  if (gridVal[selCell] == digit) return;
  gridVal[selCell] = (uint8_t)digit;
  refresh();
  beep(900, 25);
  if (isSolved()) win();
}

static void eraseCell() {
  if (selCell < 0 || gridGiven[selCell] || !gridVal[selCell]) { beep(180, 40); return; }
  gridVal[selCell] = 0;
  refresh();
  beep(600, 25);
}

static void init() { newPuzzle(); }

static void tick() {
  if (state == 1) {
    if (millis() - tEnd >= 600) {
      overlayGameOver(winSecs, " s");
      state = 2;
      tEnd = millis();
    }
    return;
  }
  if (state == 2) {
    if (touch.pressed && touch.y >= PF_Y && millis() - tEnd > 500) newPuzzle();
    return;
  }

  int secs = imin(5999, (int)((millis() - tStart) / 1000));
  if (secs != shownSecs) {
    shownSecs = secs;
    drawTime(secs);
  }

  if (!touch.tap) return;
  int x = touch.sx, y = touch.sy;

  if (inRect(x, y, SUDOKU_BOARD_X, SUDOKU_BOARD_Y, SUDOKU_BOARD_PX, SUDOKU_BOARD_PX)) {
    int col = clampi((x - SUDOKU_BOARD_X) / SUDOKU_CELL_PX, 0, 8);
    int row = clampi((y - SUDOKU_BOARD_Y) / SUDOKU_CELL_PX, 0, 8);
    int idx = row * 9 + col;
    if (idx != selCell) {
      selCell = idx;
      refresh();
      beep(1100, 12);
    }
    return;
  }
  // numpad: each button owns its 34x34 pitch cell so the gaps are not dead zones
  int padW = 3 * SUDOKU_PAD_PITCH;
  if (inRect(x, y, SUDOKU_PAD_X - 2, SUDOKU_PAD_Y - 2, padW + 2, padW + 2)) {
    int col = clampi((x - SUDOKU_PAD_X + 2) / SUDOKU_PAD_PITCH, 0, 2);
    int row = clampi((y - SUDOKU_PAD_Y + 2) / SUDOKU_PAD_PITCH, 0, 2);
    placeNumber(row * 3 + col + 1);
    return;
  }
  if (inRect(x, y, SUDOKU_WIDE_X - 4, SUDOKU_ERASE_Y - 2, SUDOKU_WIDE_W + 8, SUDOKU_WIDE_H + 3)) {
    eraseCell();
    return;
  }
  if (inRect(x, y, SUDOKU_WIDE_X - 4, SUDOKU_NEW_Y - 1, SUDOKU_WIDE_W + 8, SUDOKU_WIDE_H + 4)) {
    beep(700, 30);
    newPuzzle();
    return;
  }
}

static void icon(int cx, int cy) {
  tft.fillRect(cx - 18, cy - 18, 36, 36, C_WHITE);
  for (int k = 0; k < 3; k++)
    for (int j = 0; j < 3; j++) {
      uint16_t shade = ((k + j) & 1) ? RGB565(60, 80, 130) : RGB565(30, 40, 80);
      tft.fillRect(cx - 17 + k * 12, cy - 17 + j * 12, 11, 11, shade);
    }
  text("5", cx - 12, cy - 12, 1, C_WHITE, MC_DATUM);
  text("3", cx, cy, 1, C_CYAN, MC_DATUM);
  text("9", cx + 12, cy + 12, 1, C_WHITE, MC_DATUM);
  tft.drawRect(cx - 18, cy - 18, 36, 36, C_WHITE);
}

}  // namespace Sudoku
