// Reversi / Othello - you are Black and move first, the AI is White. Tap a dotted cell to place a disc.
// Moves that flip nothing are illegal; a side without a legal move passes. Score = disc margin of a win.
#pragma once
#include "hub.h"

namespace Reversi {

static const int CELL_PX = 26;                       // 8 x 26 = 208 px board
static const int BOARD_PX = 8 * CELL_PX;
static const int ORG_X = 8, ORG_Y = PF_Y + 5;        // top-left of the board (frame is 3 px around it)
static const int PANEL_CX = 270;                     // centre of the side panel (x 224..316)
static const int ROW_Y[3] = {0, PF_Y + 4, PF_Y + 60};  // side-panel rows: [1] you, [2] AI (index = player id)
static const int INF_SCORE = 1000000, WIN_SCORE = 20000;

static const uint16_t FELT_COL  = RGB565(0, 112, 58);
static const uint16_t LINE_COL  = RGB565(0, 52, 24);
static const uint16_t LEGAL_COL = RGB565(175, 245, 150);
static const uint16_t DISC_COL[3] = {C_BLACK, RGB565(18, 18, 22), RGB565(245, 245, 235)};
static const uint16_t RIM_COL[3]  = {C_BLACK, RGB565(90, 100, 110), RGB565(150, 150, 140)};

enum { ST_PLAY = 0, ST_AI = 1, ST_PASS = 2, ST_END = 3, ST_OVER = 4 };

static const int8_t DIR_R[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
static const int8_t DIR_C[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

// Square values: corners high, X/C-squares (next to the corners) negative, edges positive.
static const int8_t WEIGHT[64] = {
  100, -20,  10,   5,   5,  10, -20, 100,
  -20, -50,  -2,  -2,  -2,  -2, -50, -20,
   10,  -2,   1,   1,   1,   1,  -2,  10,
    5,  -2,   1,   0,   0,   1,  -2,   5,
    5,  -2,   1,   0,   0,   1,  -2,   5,
   10,  -2,   1,   1,   1,   1,  -2,  10,
  -20, -50,  -2,  -2,  -2,  -2, -50, -20,
  100, -20,  10,   5,   5,  10, -20, 100
};
static const uint8_t CORNER_SQ[4] = {0, 7, 56, 63};
static const uint8_t CORNER_ADJ[4][3] = {{1, 8, 9}, {6, 14, 15}, {48, 49, 57}, {54, 55, 62}};

static uint8_t bd[64];            // 0 empty, 1 = you (black), 2 = AI (white); index = row * 8 + col
static uint8_t legalMark[64];     // 1 on the cells the player may play now
static uint8_t sqOrder[64];       // squares sorted by WEIGHT, best first (move ordering)
static int cntYou, cntAi, shownYou, shownAi, lastMove, state, passWho;
static uint32_t timerAt;
static int aiDepth, aiRandPct, aiNoise, aiNodeCap, aiMobW, aiCornerW, aiDiscW;
static int nodes;                 // search statistics / budget
static bool budgetOn, aborted;
static uint32_t deadlineMs;

// ------------------------------------------------------------------ rules
// Discs flipped along one ray if `who` plays at (r,c) (0 if the ray is not closed by one of his own discs).
static int flipsDir(const uint8_t* b, int r, int c, int dr, int dc, int who) {
  int n = 0;
  r += dr;
  c += dc;
  while (r >= 0 && r < 8 && c >= 0 && c < 8) {
    int v = b[r * 8 + c];
    if (v == 0) return 0;
    if (v == who) return n;
    n++;
    r += dr;
    c += dc;
  }
  return 0;
}
// Number of discs `who` would flip by playing at sq (0 = illegal or occupied).
static int countFlips(const uint8_t* b, int sq, int who) {
  if (b[sq] != 0) return 0;
  int r = sq >> 3, c = sq & 7, n = 0;
  for (int d = 0; d < 8; d++) n += flipsDir(b, r, c, DIR_R[d], DIR_C[d], who);
  return n;
}
// Plays sq for `who` on board b. Returns the flip count; if it is 0 the move is illegal and b is untouched.
static int applyMove(uint8_t* b, int sq, int who) {
  if (b[sq] != 0) return 0;
  int r = sq >> 3, c = sq & 7, total = 0;
  for (int d = 0; d < 8; d++) {
    int n = flipsDir(b, r, c, DIR_R[d], DIR_C[d], who);
    int rr = r, cc = c;
    for (int k = 0; k < n; k++) {
      rr += DIR_R[d];
      cc += DIR_C[d];
      b[rr * 8 + cc] = (uint8_t)who;
    }
    total += n;
  }
  if (total > 0) b[sq] = (uint8_t)who;
  return total;
}
static bool hasMove(const uint8_t* b, int who) {
  for (int i = 0; i < 64; i++) if (countFlips(b, i, who) > 0) return true;
  return false;
}
static void countDiscs(const uint8_t* b, int& you, int& ai) {
  you = ai = 0;
  for (int i = 0; i < 64; i++) {
    if (b[i] == 1) you++;
    else if (b[i] == 2) ai++;
  }
}

// ------------------------------------------------------------------ AI
// Exact score once neither side can move, from the point of view of `who`.
static int finalScore(int mine, int theirs) {
  int diff = mine - theirs;
  if (diff > 0) return WIN_SCORE + diff;
  if (diff < 0) return -WIN_SCORE + diff;
  return 0;
}

// Static evaluation for the side `who` (positional weights + mobility + corner access + late disc count).
static int evalPos(const uint8_t* b, int who) {
  int opp = 3 - who, pos = 0, mine = 0, theirs = 0;
  for (int i = 0; i < 64; i++) {
    int v = b[i];
    if (v == 0) continue;
    if (v == who) { pos += WEIGHT[i]; mine++; }
    else          { pos -= WEIGHT[i]; theirs++; }
  }
  for (int k = 0; k < 4; k++) {            // once a corner is taken, its X/C-squares are no longer a liability
    if (b[CORNER_SQ[k]] == 0) continue;
    for (int j = 0; j < 3; j++) {
      int a = CORNER_ADJ[k][j], va = b[a];
      if (va == 0) continue;
      int sgn = (va == who) ? 1 : -1;
      pos += sgn * (4 - WEIGHT[a]);
    }
  }
  int mm = 0, om = 0, mc = 0, oc = 0;
  for (int i = 0; i < 64; i++) {
    if (b[i] != 0) continue;
    bool corner = (i == 0 || i == 7 || i == 56 || i == 63);
    if (countFlips(b, i, who) > 0) { mm++; if (corner) mc++; }
    if (countFlips(b, i, opp) > 0) { om++; if (corner) oc++; }
  }
  if (mm == 0 && om == 0) return finalScore(mine, theirs);
  int s = pos + (mm - om) * aiMobW + (mc - oc) * aiCornerW;
  if (64 - mine - theirs <= 14) s += (mine - theirs) * aiDiscW;
  return s;
}

// Negamax with alpha-beta; the value is for `who`, the side to move. Sets `aborted` when the node/time budget runs out.
static int negamax(const uint8_t* b, int who, int depth, int alpha, int beta) {
  nodes++;
  if (budgetOn && (nodes > aiNodeCap || ((nodes & 255) == 0 && (int32_t)(millis() - deadlineMs) >= 0))) {
    aborted = true;
    return 0;
  }
  if (depth <= 0) return evalPos(b, who);
  uint8_t nb[64];
  int opp = 3 - who, best = -INF_SCORE;
  bool any = false;
  for (int k = 0; k < 64; k++) {
    int sq = sqOrder[k];
    if (countFlips(b, sq, who) == 0) continue;
    any = true;
    memcpy(nb, b, 64);
    applyMove(nb, sq, who);
    int v = -negamax(nb, opp, depth - 1, -beta, -alpha);
    if (aborted) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  if (any) return best;
  if (hasMove(b, opp)) {                   // I must pass
    int v = -negamax(b, opp, depth - 1, -beta, -alpha);
    return aborted ? 0 : v;
  }
  int you, ai;
  countDiscs(b, you, ai);
  return who == 1 ? finalScore(you, ai) : finalScore(ai, you);
}

// Easy: greedy by flipped discs (random among ties).
static int pickGreedy(const uint8_t* b, int who) {
  int bestN = 0, nBest = 0, list[64];
  for (int sq = 0; sq < 64; sq++) {
    int f = countFlips(b, sq, who);
    if (f == 0) continue;
    if (f > bestN) { bestN = f; nBest = 0; }
    if (f == bestN && nBest < 64) list[nBest++] = sq;
  }
  return nBest > 0 ? list[rnd(nBest)] : -1;
}

// Iterative-deepening alpha-beta at the root. The result of an aborted (over-budget) iteration is discarded.
static int pickSearch(const uint8_t* b, int who) {
  int moves[64], nm = 0;
  for (int k = 0; k < 64 && nm < 64; k++) {
    int sq = sqOrder[k];
    if (countFlips(b, sq, who) > 0) moves[nm++] = sq;
  }
  if (nm == 0) return -1;
  if (nm == 1) return moves[0];
  nodes = 0;
  aborted = false;
  deadlineMs = millis() + 650;
  int best = moves[0];
  for (int d = 1; d <= aiDepth; d++) {
    budgetOn = (d >= 2 && aiNodeCap > 0);
    int bestV = -INF_SCORE, bestSq = -1, alpha = -INF_SCORE;
    int ord[64], no = 0;                   // previous iteration's best move first, then the rest in weight order
    ord[no++] = best;
    for (int i = 0; i < nm; i++) if (moves[i] != best) ord[no++] = moves[i];
    for (int i = 0; i < nm; i++) {
      int sq = ord[i];
      uint8_t nb[64];
      memcpy(nb, b, 64);
      applyMove(nb, sq, who);
      int v = -negamax(nb, 3 - who, d - 1, -INF_SCORE, -alpha);
      if (aborted) break;
      if (aiNoise > 0) v += rnd(aiNoise + 1);
      if (v > bestV) {
        bestV = v;
        bestSq = sq;
        if (aiNoise == 0) alpha = v;
      }
    }
    if (aborted || bestSq < 0) break;
    best = bestSq;
  }
  budgetOn = false;
  return best;
}

// Chooses a move for `who` on board b according to the difficulty knobs; -1 if he has no legal move.
static int aiPick(const uint8_t* b, int who) {
  if (!hasMove(b, who)) return -1;
  if (aiRandPct > 0 && rnd(100) < aiRandPct) {
    int list[64], n = 0;
    for (int sq = 0; sq < 64 && n < 64; sq++) if (countFlips(b, sq, who) > 0) list[n++] = sq;
    return list[rnd(n)];
  }
  if (aiDepth <= 1) return pickGreedy(b, who);
  return pickSearch(b, who);
}

static void setKnobs() {
  aiDepth    = byDiff(1, 2, 4);          // plies searched (1 = greedy flip count)
  aiRandPct  = byDiff(50, 0, 0);         // chance (percent) of a random legal move
  aiNoise    = byDiff(0, 5, 0);          // random tie-breaking noise on root scores
  aiNodeCap  = byDiff(0, 0, 25000);      // search budget (nodes) from depth 2 on
  aiMobW     = byDiff(0, 4, 6);          // mobility weight
  aiCornerW  = byDiff(0, 0, 30);         // corner-access weight
  aiDiscW    = byDiff(0, 0, 5);          // disc-count weight in the last 14 empties
  // sort squares by weight (stable insertion sort) for move ordering
  for (int i = 0; i < 64; i++) {
    int j = i;
    while (j > 0 && WEIGHT[sqOrder[j - 1]] < WEIGHT[i]) { sqOrder[j] = sqOrder[j - 1]; j--; }
    sqOrder[j] = (uint8_t)i;
  }
}

// ------------------------------------------------------------------ drawing
static void drawCell(int i) {
  int x = ORG_X + (i & 7) * CELL_PX, y = ORG_Y + (i >> 3) * CELL_PX;
  tft.fillRect(x, y, CELL_PX, CELL_PX, FELT_COL);
  tft.drawFastHLine(x, y, CELL_PX, LINE_COL);
  tft.drawFastVLine(x, y, CELL_PX, LINE_COL);
  int cx = x + CELL_PX / 2 + 1, cy = y + CELL_PX / 2 + 1;
  int v = bd[i];
  if (v != 0) {
    tft.fillCircle(cx, cy, 10, DISC_COL[v]);
    tft.drawCircle(cx, cy, 10, RIM_COL[v]);
    if (i == lastMove) tft.fillCircle(cx, cy, 2, C_RED);
  } else if (legalMark[i]) {
    tft.drawCircle(cx, cy, 6, LEGAL_COL);
    tft.fillCircle(cx, cy, 2, LEGAL_COL);
  }
}
static void drawBoard() {
  tft.fillRect(ORG_X - 3, ORG_Y - 3, BOARD_PX + 6, BOARD_PX + 6, LINE_COL);
  for (int i = 0; i < 64; i++) drawCell(i);
}
static void drawTurnFrame(int who) {
  for (int p = 1; p <= 2; p++)
    tft.drawRoundRect(224, ROW_Y[p], 92, 52, 6, p == who ? C_GOLD : C_BLACK);
}
static void drawCounts(bool force) {
  if (force || cntYou != shownYou) numBox(cntYou, 285, ROW_Y[1] + 20, 50, 30, 4, C_WHITE, C_BLACK);
  if (force || cntAi != shownAi) numBox(cntAi, 285, ROW_Y[2] + 20, 50, 30, 4, C_WHITE, C_BLACK);
  shownYou = cntYou;
  shownAi = cntAi;
}
static void drawPassBox(int who) {            // who = 0 clears the indicator
  tft.fillRect(224, 122, 92, 44, C_BLACK);
  if (who == 0) {
    text("PASS", PANEL_CX, 138, 2, RGB565(60, 60, 70), MC_DATUM);
  } else {
    tft.fillRoundRect(232, 122, 76, 24, 6, C_ORANGE);
    text("PASS", PANEL_CX, 134, 2, C_BLACK, MC_DATUM);
    text(who == 1 ? "(you)" : "(AI)", PANEL_CX, 156, 2, C_ORANGE, MC_DATUM);
  }
}
static void drawPanel() {
  tft.fillRect(220, PF_Y, SCR_W - 220, PF_H, C_BLACK);
  for (int p = 1; p <= 2; p++) {
    tft.fillCircle(240, ROW_Y[p] + 20, 12, DISC_COL[p]);
    tft.drawCircle(240, ROW_Y[p] + 20, 12, RIM_COL[p]);
    text(p == 1 ? "YOU" : "AI", 240, ROW_Y[p] + 41, 1, C_LGREY, MC_DATUM);
  }
  drawPassBox(0);
  text(DIFF_NAMES[gDiff], PANEL_CX, 190, 2, C_GREY, MC_DATUM);
  text("Tap a dot", PANEL_CX, 214, 1, C_GREY, MC_DATUM);
  text("Black moves first", PANEL_CX, 226, 1, C_GREY, MC_DATUM);
}

// ------------------------------------------------------------------ game flow
static bool due() { return (int32_t)(millis() - timerAt) >= 0; }

// Hand the move to `who`: mark the legal cells for the player, or start the AI's thinking pause.
static void setTurn(int who) {
  drawTurnFrame(who);
  if (who == 1) {
    for (int i = 0; i < 64; i++) {
      uint8_t m = (countFlips(bd, i, 1) > 0) ? 1 : 0;
      if (m != legalMark[i]) { legalMark[i] = m; drawCell(i); }
    }
    state = ST_PLAY;
    hudText("Your turn", C_WHITE);
  } else {
    state = ST_AI;
    timerAt = millis() + 350;
    hudText("AI thinking", C_YELLOW);
  }
}

static void playMove(int sq, int who) {
  uint8_t old[64];
  memcpy(old, bd, 64);
  applyMove(bd, sq, who);
  int prev = lastMove;
  lastMove = sq;
  for (int i = 0; i < 64; i++) {
    bool wasMark = legalMark[i] != 0;
    legalMark[i] = 0;
    if (bd[i] != old[i] || i == sq || i == prev || wasMark) drawCell(i);
  }
  countDiscs(bd, cntYou, cntAi);
  drawCounts(false);
  beep(who == 1 ? 700 : 520, 30);
}

static void startPass(int who) {
  passWho = who;
  state = ST_PASS;
  timerAt = millis() + 1200;
  drawTurnFrame(0);
  drawPassBox(who);
  hudText("No move - pass", C_ORANGE);
  beep(300, 120);
}

static void endGame() {
  state = ST_END;
  timerAt = millis() + 900;
  drawTurnFrame(0);
  hudText("Game over", C_YELLOW);
  beep(500, 100);
}

// `who` has just moved: find out who plays next (or whether somebody must pass / the game is over).
static void afterMove(int who) {
  int next = 3 - who;
  if (hasMove(bd, next)) setTurn(next);
  else if (hasMove(bd, who)) startPass(next);
  else endGame();
}

static void endOverlay() {
  char l1[40], l2[40];
  int margin = cntYou - cntAi;
  snprintf(l1, sizeof l1, "Final: %d - %d", cntYou, cntAi);
  if (margin > 0) {
    bool nb = submitScore(margin);
    if (nb) snprintf(l2, sizeof l2, "NEW BEST! +%d discs", margin);
    else    snprintf(l2, sizeof l2, "+%d discs  Best: %d", margin, bestScore());
    overlayMsg("YOU WIN", C_GREEN, l1, l2, "Tap to play again");
    beep(1200, 150);
  } else if (margin == 0) {
    overlayMsg("DRAW", C_YELLOW, l1, "No score for a draw", "Tap to play again");
  } else {
    snprintf(l2, sizeof l2, "Lost by %d discs", -margin);
    overlayMsg("YOU LOSE", C_RED, l1, l2, "Tap to play again");
    beep(160, 300);
  }
  state = ST_OVER;
}

static void newGame() {
  pfClear();
  memset(bd, 0, sizeof bd);
  memset(legalMark, 0, sizeof legalMark);
  bd[27] = 2; bd[28] = 1;                  // d4 white, e4 black
  bd[35] = 1; bd[36] = 2;                  // d5 black, e5 white
  lastMove = -1;
  passWho = 0;
  countDiscs(bd, cntYou, cntAi);
  drawBoard();
  drawPanel();
  drawCounts(true);
  setTurn(1);
}

static void init() {
  hudBegin("Reversi");
  setKnobs();
  newGame();
}

static void tick() {
  if (state == ST_OVER) {
    if (touch.released) newGame();
    return;
  }
  if (state == ST_END) {
    if (due()) endOverlay();
    return;
  }
  if (state == ST_PASS) {
    if (due()) {
      drawPassBox(0);
      setTurn(3 - passWho);
    }
    return;
  }
  if (state == ST_AI) {
    if (due()) {
      int sq = aiPick(bd, 2);
      if (sq < 0) afterMove(1);            // cannot happen (AI only gets the turn with a legal move); stay safe
      else { playMove(sq, 2); afterMove(2); }
    }
    return;
  }
  // ST_PLAY
  if (touch.tap) {
    int tx = touch.x - ORG_X, ty = touch.y - ORG_Y;
    if (tx >= -6 && tx < BOARD_PX + 6 && ty >= -6 && ty < BOARD_PX + 6) {
      int sq = (clampi(ty, 0, BOARD_PX - 1) / CELL_PX) * 8 + clampi(tx, 0, BOARD_PX - 1) / CELL_PX;
      if (legalMark[sq]) { playMove(sq, 1); afterMove(1); }
      else beep(200, 40);
    }
  }
}

static void icon(int cx, int cy) {
  tft.fillRoundRect(cx - 17, cy - 17, 34, 34, 4, FELT_COL);
  tft.drawFastHLine(cx - 16, cy, 33, LINE_COL);
  tft.drawFastVLine(cx, cy - 16, 33, LINE_COL);
  tft.fillCircle(cx - 8, cy - 8, 6, C_WHITE);
  tft.fillCircle(cx + 8, cy - 8, 6, C_BLACK);
  tft.drawCircle(cx + 8, cy - 8, 6, C_GREY);
  tft.fillCircle(cx - 8, cy + 8, 6, C_BLACK);
  tft.drawCircle(cx - 8, cy + 8, 6, C_GREY);
  tft.fillCircle(cx + 8, cy + 8, 6, C_WHITE);
}

}  // namespace Reversi
