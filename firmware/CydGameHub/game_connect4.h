// Connect 4 - you are red, the AI is yellow. Tap anywhere over a column to drop a disc.
// Score = win streak (a draw keeps it, a loss ends it).
#pragma once
#include "hub.h"

namespace Connect4 {

static const int COLS = 7, ROWS = 6, CS = 30;
static const int BW = COLS * CS, BH = ROWS * CS;
static const int OX = (SCR_W - BW) / 2;
static const int OY = PF_Y + 2 + CS;               // one free row above the board for the falling disc
static const int INF = 1000000, WINSC = 100000;
static const uint16_t COL_BOARD = RGB565(20, 60, 200);
static const uint16_t COL_EDGE  = RGB565(90, 130, 255);
static const uint16_t DISC[3] = {C_BLACK, C_RED, C_YELLOW};
static const uint16_t DARK[3] = {C_BLACK, RGB565(150, 0, 0), RGB565(190, 150, 0)};

enum { ST_PLAY = 0, ST_DROP = 1, ST_AI = 2, ST_END = 3, ST_OVER = 4 };
enum { END_NONE = 0, END_WIN = 1, END_LOSS = 2, END_DRAW = 3 };

static const int8_t ORDER[COLS] = {3, 2, 4, 1, 5, 0, 6};
static const int8_t DR[4] = {0, 1, 1, 1};
static const int8_t DC[4] = {1, 0, 1, -1};

static int8_t g[ROWS][COLS];       // 0 empty, 1 = you (red), 2 = AI (yellow); row 0 is the top
static int8_t fillN[COLS];         // discs in each column
static int moves, streak, roundNo, state, endKind;
static int dropCol, dropTarget, dropRow, dropWho;
static int8_t winR[8], winC[8];
static int winN;
static uint32_t timerAt;
static int aiDepth, aiSlipPct;     // search depth (plies) and chance (percent) of a random "slip" move

static bool inb(int r, int c) { return r >= 0 && r < ROWS && c >= 0 && c < COLS; }

// ------------------------------------------------------------------ drawing
static int discX(int c) { return OX + c * CS + CS / 2; }
static int discY(int r) { return OY + r * CS + CS / 2; }   // r = -1 is the slot above the board

static void drawDiscAt(int r, int c, int who) {
  tft.fillCircle(discX(c), discY(r), 12, DISC[who]);
  tft.drawCircle(discX(c), discY(r), 7, DARK[who]);
}
static void eraseAnim(int r, int c) {
  if (r < 0) tft.fillRect(OX + c * CS, OY - CS, CS, CS, C_BLACK);
  else tft.fillCircle(discX(c), discY(r), 12, C_BLACK);
}
static void drawBoardCell(int r, int c) {
  if (g[r][c] == 0) tft.fillCircle(discX(c), discY(r), 12, C_BLACK);
  else drawDiscAt(r, c, g[r][c]);
}
static void drawBoard() {
  tft.fillRoundRect(OX, OY, BW, BH, 8, COL_BOARD);
  tft.drawRoundRect(OX, OY, BW, BH, 8, COL_EDGE);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) drawBoardCell(r, c);
}
static void drawSide() {
  tft.fillRect(0, PF_Y, OX - 1, PF_H, C_BLACK);
  tft.fillRect(OX + BW + 1, PF_Y, SCR_W - (OX + BW + 1), PF_H, C_BLACK);
  text("STREAK", OX / 2, 84, 1, C_LGREY, MC_DATUM);
  numBox(streak, OX / 2, 110, OX - 4, 30, 4, C_GOLD, C_BLACK);
  int rx = OX + BW + (SCR_W - OX - BW) / 2;
  tft.fillCircle(rx, 100, 9, C_RED);
  text("YOU", rx, 118, 1, C_LGREY, MC_DATUM);
  tft.fillCircle(rx, 150, 9, C_YELLOW);
  text("AI", rx, 168, 1, C_LGREY, MC_DATUM);
}
static void hudTurn() {
  if (state == ST_AI || (state == ST_DROP && dropWho == 2)) hudText("AI thinking", C_YELLOW);
  else hudText("Your turn", C_RED);
}

// ------------------------------------------------------------------ rules
// Is there a run of 4+ for player p through (r,c)? (cheap, used inside the search)
static bool fourAt(int r, int c, int p) {
  for (int d = 0; d < 4; d++) {
    int n = 1;
    int rr = r + DR[d], cc = c + DC[d];
    while (inb(rr, cc) && g[rr][cc] == p) { n++; rr += DR[d]; cc += DC[d]; }
    rr = r - DR[d]; cc = c - DC[d];
    while (inb(rr, cc) && g[rr][cc] == p) { n++; rr -= DR[d]; cc -= DC[d]; }
    if (n >= 4) return true;
  }
  return false;
}
// Same, but records the connected discs in winR/winC. Returns the count (0 = none).
static int findWin(int r, int c, int p) {
  for (int d = 0; d < 4; d++) {
    int br = r, bc = c;
    while (inb(br - DR[d], bc - DC[d]) && g[br - DR[d]][bc - DC[d]] == p) { br -= DR[d]; bc -= DC[d]; }
    int n = 0, rr = br, cc = bc;
    while (inb(rr, cc) && g[rr][cc] == p && n < 8) {
      winR[n] = (int8_t)rr; winC[n] = (int8_t)cc; n++;
      rr += DR[d]; cc += DC[d];
    }
    if (n >= 4) return n;
  }
  return 0;
}

// Static evaluation from the point of view of player p.
static int evalFor(int p) {
  int o = 3 - p, s = 0;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      for (int d = 0; d < 4; d++) {
        int er = r + 3 * DR[d], ec = c + 3 * DC[d];
        if (!inb(er, ec)) continue;
        int mine = 0, opp = 0;
        for (int k = 0; k < 4; k++) {
          int v = g[r + k * DR[d]][c + k * DC[d]];
          if (v == p) mine++;
          else if (v == o) opp++;
        }
        if (opp == 0) { if (mine == 3) s += 6; else if (mine == 2) s += 2; }
        else if (mine == 0) { if (opp == 3) s -= 6; else if (opp == 2) s -= 2; }
      }
    }
    if (g[r][3] == p) s += 3;
    else if (g[r][3] == o) s -= 3;
  }
  return s;
}

// Negamax with alpha-beta; the value is for `player`, the side to move.
static int negamax(int player, int depth, int alpha, int beta) {
  if (moves >= ROWS * COLS) return 0;
  if (depth <= 0) return evalFor(player);
  int best = -INF;
  for (int k = 0; k < COLS; k++) {
    int c = ORDER[k];
    if (fillN[c] >= ROWS) continue;
    int r = ROWS - 1 - fillN[c];
    g[r][c] = (int8_t)player; fillN[c]++; moves++;
    int v;
    if (fourAt(r, c, player)) v = WINSC + depth;
    else v = -negamax(3 - player, depth - 1, -beta, -alpha);
    g[r][c] = 0; fillN[c]--; moves--;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

static int aiPick() {
  int legal[COLS], n = 0;
  for (int c = 0; c < COLS; c++) if (fillN[c] < ROWS) legal[n++] = c;
  if (n == 0) return 0;                       // cannot happen (board not full)
  if (rnd(100) < aiSlipPct) return legal[rnd(n)];    // human-like slip
  int bestC = legal[0], bestV = -2 * INF;
  for (int k = 0; k < COLS; k++) {
    int c = ORDER[k];
    if (fillN[c] >= ROWS) continue;
    int r = ROWS - 1 - fillN[c];
    g[r][c] = 2; fillN[c]++; moves++;
    int v;
    if (fourAt(r, c, 2)) v = WINSC + aiDepth;
    else v = -negamax(1, aiDepth - 1, -INF, (bestV < -INF) ? INF : -bestV);
    g[r][c] = 0; fillN[c]--; moves--;
    if (v > bestV) { bestV = v; bestC = c; }
  }
  return bestC;
}

// ------------------------------------------------------------------ game flow
static void newRound() {
  pfClear();
  memset(g, 0, sizeof g);
  memset(fillN, 0, sizeof fillN);
  moves = 0;
  winN = 0;
  endKind = END_NONE;
  drawBoard();
  drawSide();
  bool aiFirst = (roundNo & 1) != 0;
  roundNo++;
  if (aiFirst) { state = ST_AI; timerAt = millis() + 450; }
  else state = ST_PLAY;
  hudTurn();
}

static void startDrop(int c, int who) {
  dropCol = c;
  dropWho = who;
  dropTarget = ROWS - 1 - fillN[c];
  dropRow = -1;
  state = ST_DROP;
  drawDiscAt(-1, c, who);
}

static void finishRound(int kind) {
  endKind = kind;
  state = ST_END;
  timerAt = millis() + 900;
  for (int i = 0; i < winN; i++) {
    tft.drawCircle(discX(winC[i]), discY(winR[i]), 13, C_WHITE);
    tft.drawCircle(discX(winC[i]), discY(winR[i]), 14, C_WHITE);
  }
  if (kind == END_WIN) {
    streak++;
    numBox(streak, OX / 2, 110, OX - 4, 30, 4, C_GOLD, C_BLACK);
    hudText("You win!", C_GREEN);
    beep(1200, 120);
  } else if (kind == END_LOSS) {
    hudText("AI wins", C_RED);
    beep(160, 300);
  } else {
    hudText("Draw", C_YELLOW);
    beep(500, 100);
  }
}

static void land() {
  int r = dropTarget, c = dropCol;
  g[r][c] = (int8_t)dropWho;
  fillN[c]++;
  moves++;
  beep(dropWho == 1 ? 520 : 400, 30);
  winN = findWin(r, c, dropWho);
  if (winN >= 4) { finishRound(dropWho == 1 ? END_WIN : END_LOSS); return; }
  winN = 0;
  if (moves >= ROWS * COLS) { finishRound(END_DRAW); return; }
  if (dropWho == 1) { state = ST_AI; timerAt = millis() + 350; }
  else state = ST_PLAY;
  hudTurn();
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
  hudBegin("Connect 4");
  aiDepth = byDiff(2, 5, 5);      // depth 6 can exceed ~1 s on an ESP32 in rare positions, so hard = depth 5 without slips
  aiSlipPct = byDiff(30, 15, 0);
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
  if (state == ST_DROP) {
    eraseAnim(dropRow, dropCol);
    dropRow++;
    drawDiscAt(dropRow, dropCol, dropWho);
    if (dropRow >= dropTarget) land();
    return;
  }
  if (state == ST_AI) {
    if ((int32_t)(millis() - timerAt) >= 0) startDrop(aiPick(), 2);
    return;
  }
  // ST_PLAY
  if (touch.tap) {
    int px = touch.x - OX;
    if (px >= -10 && px < BW + 10) {
      int c = clampi(px, 0, BW - 1) / CS;
      if (fillN[c] < ROWS) startDrop(c, 1);
      else beep(200, 40);
    }
  }
}

static void icon(int cx, int cy) {
  tft.fillRoundRect(cx - 17, cy - 15, 34, 30, 4, COL_BOARD);
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      uint16_t col = C_BLACK;
      if ((r + c) == 3 || (r == 2 && c == 0)) col = C_RED;
      else if ((r + c) == 2 || (r == 1 && c == 2)) col = C_YELLOW;
      tft.fillCircle(cx - 9 + c * 9, cy - 8 + r * 8, 3, col);
    }
  }
}

}  // namespace Connect4
