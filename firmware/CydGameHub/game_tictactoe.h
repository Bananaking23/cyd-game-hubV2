// Tic-Tac-Toe - you are X, the AI is O. Tap a cell to play. Score = win streak.
#pragma once
#include "hub.h"

namespace TicTacToe {

static const int CELL = 64, BS = CELL * 3;
static const int OX = (SCR_W - BS) / 2;
static const int OY = PF_Y + (PF_H - BS) / 2;
static const uint16_t COL_X    = C_CYAN;
static const uint16_t COL_O    = C_ORANGE;
static const uint16_t COL_GRID = RGB565(120, 130, 170);
static const uint16_t COL_WIN  = RGB565(0, 90, 40);

enum { ST_PLAY = 0, ST_AI = 1, ST_END = 2, ST_OVER = 3 };
enum { END_NONE = 0, END_WIN = 1, END_LOSS = 2, END_DRAW = 3 };

static const uint8_t LINES[8][3] = {
  {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}
};

static int8_t board[9];                 // 0 empty, 1 = X (you), 2 = O (AI)
static int streak, roundNo, state, endKind, winLine;
static bool aiStarts;
static int aiRandomPct;                 // chance (percent) that the AI plays a random cell instead of the best one
static uint32_t timerAt;

static int cellX(int i) { return OX + (i % 3) * CELL; }
static int cellY(int i) { return OY + (i / 3) * CELL; }

static void drawGrid() {
  for (int i = 1; i < 3; i++) {
    tft.fillRoundRect(OX + i * CELL - 2, OY + 4, 4, BS - 8, 2, COL_GRID);
    tft.fillRoundRect(OX + 4, OY + i * CELL - 2, BS - 8, 4, 2, COL_GRID);
  }
}

static void clearCell(int i, uint16_t bg) {
  tft.fillRect(cellX(i) + 3, cellY(i) + 3, CELL - 6, CELL - 6, bg);
}

static void drawMark(int i, int who, uint16_t bg) {
  int cx = cellX(i) + CELL / 2, cy = cellY(i) + CELL / 2;
  if (who == 1) {
    for (int k = -1; k <= 1; k++) {
      tft.drawLine(cx - 18 + k, cy - 18, cx + 18 + k, cy + 18, COL_X);
      tft.drawLine(cx + 18 + k, cy - 18, cx - 18 + k, cy + 18, COL_X);
    }
  } else if (who == 2) {
    tft.fillCircle(cx, cy, 21, COL_O);
    tft.fillCircle(cx, cy, 14, bg);
  }
}

static void drawSide() {
  // left: streak
  tft.fillRect(0, PF_Y, 62, PF_H, C_BLACK);
  text("STREAK", 31, 84, 1, C_LGREY, MC_DATUM);
  numBox(streak, 31, 110, 58, 30, 4, C_GOLD, C_BLACK);
  // right: legend
  tft.fillRect(258, PF_Y, 62, PF_H, C_BLACK);
  text("X", 289, 92, 4, COL_X, MC_DATUM);
  text("YOU", 289, 112, 1, C_LGREY, MC_DATUM);
  text("O", 289, 150, 4, COL_O, MC_DATUM);
  text("AI", 289, 170, 1, C_LGREY, MC_DATUM);
  text(aiStarts ? "AI starts" : "You start", 289, 205, 1, C_GREY, MC_DATUM);
}

static int winnerOf(int* lineOut) {
  for (int i = 0; i < 8; i++) {
    int a = board[LINES[i][0]];
    if (a != 0 && a == board[LINES[i][1]] && a == board[LINES[i][2]]) {
      if (lineOut) *lineOut = i;
      return a;
    }
  }
  return 0;
}

// Negamax with alpha-beta. Returns the score for `turn` (the side to move).
static int negamax(int turn, int depth, int alpha, int beta) {
  if (winnerOf(0)) return depth - 10;   // the previous mover has won
  int best = -100;
  bool any = false;
  for (int i = 0; i < 9; i++) {
    if (board[i] != 0) continue;
    any = true;
    board[i] = (int8_t)turn;
    int v = -negamax(3 - turn, depth + 1, -beta, -alpha);
    board[i] = 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return any ? best : 0;
}

static void newRound() {
  pfClear();
  drawGrid();
  memset(board, 0, sizeof board);
  winLine = -1;
  endKind = END_NONE;
  aiStarts = (roundNo & 1) != 0;
  roundNo++;
  drawSide();
  if (aiStarts) {
    state = ST_AI;
    timerAt = millis() + 450;
    hudText("AI thinking...", COL_O);
  } else {
    state = ST_PLAY;
    hudText("Your turn", COL_X);
  }
}

static void highlightWin() {
  if (winLine < 0) return;
  for (int k = 0; k < 3; k++) {
    int i = LINES[winLine][k];
    clearCell(i, COL_WIN);
    drawMark(i, board[i], COL_WIN);
  }
}

// After a move: detect the end of the round. Returns true if the round ended.
static bool checkEnd() {
  int line = -1;
  int w = winnerOf(&line);
  if (w) {
    winLine = line;
    endKind = (w == 1) ? END_WIN : END_LOSS;
  } else {
    bool full = true;
    for (int i = 0; i < 9; i++) if (board[i] == 0) full = false;
    if (!full) return false;
    endKind = END_DRAW;
  }
  state = ST_END;
  timerAt = millis() + 800;
  highlightWin();
  if (endKind == END_WIN) {
    streak++;
    numBox(streak, 31, 110, 58, 30, 4, C_GOLD, C_BLACK);
    hudText("You win!", C_GREEN);
    beep(1200, 120);
  } else if (endKind == END_LOSS) {
    hudText("AI wins", C_RED);
    beep(160, 300);
  } else {
    hudText("Draw", C_YELLOW);
    beep(500, 100);
  }
  return true;
}

static void placeMove(int i, int who) {
  board[i] = (int8_t)who;
  clearCell(i, C_BLACK);
  drawMark(i, who, C_BLACK);
  beep(who == 1 ? 880 : 660, 35);
  if (checkEnd()) return;
  if (who == 1) {
    state = ST_AI;
    timerAt = millis() + 400;
    hudText("AI thinking...", COL_O);
  } else {
    state = ST_PLAY;
    hudText("Your turn", COL_X);
  }
}

static void aiMove() {
  int cand[9], n = 0;
  for (int i = 0; i < 9; i++) if (board[i] == 0) cand[n++] = i;
  if (n == 0) return;  // cannot happen: state is only ST_AI while a cell is free
  int pick;
  if (n == 9) {
    static const int8_t first[5] = {0, 2, 4, 6, 8};
    pick = first[rnd(5)];
  } else if (rnd(100) < aiRandomPct) {
    pick = cand[rnd(n)];
  } else {
    int bestV = -100, bestList[9], nb = 0;
    for (int k = 0; k < n; k++) {
      int i = cand[k];
      board[i] = 2;
      int v = -negamax(1, 1, -100, 100);
      board[i] = 0;
      if (v > bestV) { bestV = v; nb = 0; }
      if (v == bestV) bestList[nb++] = i;
    }
    pick = bestList[rnd(nb)];
  }
  placeMove(pick, 2);
}

static void endOverlay() {
  char l1[32], l2[32];
  snprintf(l2, sizeof l2, "%s", ((roundNo & 1) != 0) ? "AI goes first" : "You go first");
  if (endKind == END_WIN) {
    snprintf(l1, sizeof l1, "Streak: %d", streak);
    overlayMsg("YOU WIN", C_GREEN, l1, l2, "Tap to continue");
  } else if (endKind == END_DRAW) {
    snprintf(l1, sizeof l1, "Streak kept: %d", streak);
    overlayMsg("DRAW", C_YELLOW, l1, l2, "Tap to continue");
  } else {
    overlayGameOver(streak);
  }
  state = ST_OVER;
}

static void init() {
  hudBegin("Tic-Tac-Toe");
  aiRandomPct = byDiff(60, 25, 0);        // hard = perfect play (best a human can do is a draw)
  streak = 0;
  roundNo = 0;
  newRound();
}

static void tick() {
  if (state == ST_OVER) {
    if (touch.pressed) {
      if (endKind == END_LOSS) streak = 0;
      newRound();
    }
    return;
  }
  if (state == ST_END) {
    if ((int32_t)(millis() - timerAt) >= 0) endOverlay();
    return;
  }
  if (state == ST_AI) {
    if ((int32_t)(millis() - timerAt) >= 0) aiMove();
    return;
  }
  // ST_PLAY
  if (touch.tap) {
    int tx = touch.x - OX, ty = touch.y - OY;
    if (tx >= -8 && tx < BS + 8 && ty >= -8 && ty < BS + 8) {
      tx = clampi(tx, 0, BS - 1);
      ty = clampi(ty, 0, BS - 1);
      int i = (ty / CELL) * 3 + (tx / CELL);
      if (board[i] == 0) placeMove(i, 1);
      else beep(200, 40);
    }
  }
}

static void icon(int cx, int cy) {
  tft.fillRect(cx - 6, cy - 17, 3, 34, COL_GRID);
  tft.fillRect(cx + 4, cy - 17, 3, 34, COL_GRID);
  tft.fillRect(cx - 17, cy - 6, 34, 3, COL_GRID);
  tft.fillRect(cx - 17, cy + 4, 34, 3, COL_GRID);
  for (int k = -1; k <= 1; k++) {  // X, top-left cell
    tft.drawLine(cx - 14 + k, cy - 14, cx - 8 + k, cy - 8, COL_X);
    tft.drawLine(cx - 8 + k, cy - 14, cx - 14 + k, cy - 8, COL_X);
  }
  tft.fillCircle(cx, cy, 5, COL_O);                // O, centre
  tft.fillCircle(cx, cy, 2, C_DGREY);
  for (int k = -1; k <= 1; k++) {  // X, bottom-right cell
    tft.drawLine(cx + 8 + k, cy + 8, cx + 14 + k, cy + 14, COL_X);
    tft.drawLine(cx + 14 + k, cy + 8, cx + 8 + k, cy + 14, COL_X);
  }
}

}  // namespace TicTacToe
