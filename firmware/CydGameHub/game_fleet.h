// Fleet - Battleship against the board. Tap a square of the big grid to fire at the hidden enemy fleet; the CPU fires
// back at yours (small map on the right). First to sink all five ships wins. Fewer shots = better.
// Easy: the CPU fires at random. Normal: it hunts down ships it has hit. Hard: it also uses a chessboard search pattern
// and follows lines of hits.
#pragma once
#include "hub.h"

namespace Fleet {

static const int N = 8, CELL = 24, GX = 8, GY = PF_Y + 8;        // enemy grid spans x 8..199, y 30..221
static const int MX = 226, MY = PF_Y + 20, MC = 9;               // your fleet's map: 8 x 8 cells of 9 px
static const uint8_t LEN[6] = {0, 5, 4, 3, 3, 2};                // ship lengths by id 1..5
static const uint16_t WATER = RGB565(20, 52, 110), SHIP_COL = RGB565(130, 150, 180);

static uint8_t eShip[N][N], eShot[N][N];    // enemy layout / shots at it: 0 unknown, 1 miss, 2 hit, 3 hit on a sunk ship
static uint8_t pShip[N][N], pShot[N][N];    // your layout / the CPU's shots
static uint8_t eHits[6], pHits[6];
static int shots, eLeft, pLeft, state;      // state: 0 your turn, 1 CPU about to fire, 2 finished
static uint32_t tTurn;
static char msgYou[20], msgFoe[20];
static uint16_t colYou, colFoe;

static void drawEnemy(int r, int c) {
  int x = GX + c * CELL, y = GY + r * CELL, cx = x + CELL / 2, cy = y + CELL / 2;
  uint8_t s = eShot[r][c];
  if (s == 3) {
    tft.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, RGB565(100, 26, 26));
    tft.drawRect(x + 3, y + 3, CELL - 6, CELL - 6, RGB565(190, 70, 60));
    return;
  }
  tft.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, s == 2 ? RGB565(210, 70, 36) : WATER);
  if (s == 1) tft.fillCircle(cx, cy, 3, RGB565(170, 200, 235));
  if (s == 2) { tft.fillCircle(cx, cy, 7, RGB565(255, 190, 40)); tft.fillCircle(cx, cy, 3, C_WHITE); }
}
static void drawMini(int r, int c) {
  int x = MX + c * MC, y = MY + r * MC;
  tft.fillRect(x, y, MC - 1, MC - 1, RGB565(16, 40, 86));
  if (pShip[r][c]) tft.fillRect(x + 1, y + 1, MC - 3, MC - 3, SHIP_COL);
  if (pShot[r][c] == 1) tft.fillRect(x + 3, y + 3, 2, 2, C_WHITE);
  if (pShot[r][c] >= 2) tft.fillRect(x + 1, y + 1, MC - 3, MC - 3, pShot[r][c] == 3 ? RGB565(120, 24, 24) : RGB565(240, 90, 40));
}
static void drawStats() {
  tft.fillRect(212, 124, 108, 56, C_BLACK);
  char b[24];
  snprintf(b, sizeof b, "Shots %d", shots);
  text(b, 266, 132, 2, C_WHITE, MC_DATUM);
  snprintf(b, sizeof b, "Enemy %d left", eLeft);
  text(b, 266, 152, 2, RGB565(255, 140, 120), MC_DATUM);
  snprintf(b, sizeof b, "You %d left", pLeft);
  text(b, 266, 172, 2, RGB565(140, 200, 255), MC_DATUM);
}
static void drawMsgs() {
  tft.fillRect(212, 184, 108, 52, C_BLACK);
  text(msgYou, 266, 196, 2, colYou, MC_DATUM);
  text(msgFoe, 266, 218, 2, colFoe, MC_DATUM);
}
static void setMsg(char* dst, uint16_t* col, const char* s, uint16_t c) {
  snprintf(dst, 20, "%s", s);
  *col = c;
}

static void placeFleet(uint8_t ship[N][N]) {
  memset(ship, 0, N * N);
  for (int id = 1; id <= 5; id++) {
    for (int tries = 0; tries < 400; tries++) {
      bool horiz = rnd(2) == 0;
      int len = LEN[id];
      int r = rnd(horiz ? N : N - len + 1), c = rnd(horiz ? N - len + 1 : N);
      bool ok = true;
      for (int i = 0; i < len; i++) if (ship[r + (horiz ? 0 : i)][c + (horiz ? i : 0)]) ok = false;
      if (!ok) continue;
      for (int i = 0; i < len; i++) ship[r + (horiz ? 0 : i)][c + (horiz ? i : 0)] = (uint8_t)id;
      break;
    }
  }
}

// Fire at (r,c) on a board. Returns 0 miss, 1 hit, 2 hit and sunk; the ship counters are updated by the caller.
static int shoot(uint8_t ship[N][N], uint8_t shot[N][N], uint8_t* hits, int r, int c) {
  if (!ship[r][c]) { shot[r][c] = 1; return 0; }
  int id = ship[r][c];
  shot[r][c] = 2;
  hits[id]++;
  if (hits[id] < LEN[id]) return 1;
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      if (ship[i][j] == id) shot[i][j] = 3;
  return 2;
}

static void cpuPick(int* pr, int* pc) {
  int w[N * N];
  memset(w, 0, sizeof w);
  static const int DR[4] = {-1, 0, 1, 0}, DC[4] = {0, 1, 0, -1};
  bool cand = false;
  if (gDiff > 0) {
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++) {
        if (pShot[r][c] != 2) continue;                          // an un-sunk hit: look next to it
        for (int d = 0; d < 4; d++) {
          int nr = r + DR[d], nc = c + DC[d];
          if (nr < 0 || nc < 0 || nr >= N || nc >= N || pShot[nr][nc] != 0) continue;
          w[nr * N + nc] += 1;
          int br = r - DR[d], bc = c - DC[d];                    // another hit behind it: this is probably a line
          if (gDiff == 2 && br >= 0 && bc >= 0 && br < N && bc < N && pShot[br][bc] == 2) w[nr * N + nc] += 8;
          cand = true;
        }
      }
  }
  int best = 0, n = 0, list[N * N];
  if (cand) {
    for (int i = 0; i < N * N; i++) if (w[i] > best) best = w[i];
    for (int i = 0; i < N * N; i++) if (w[i] > 0 && (gDiff == 2 ? w[i] == best : true)) list[n++] = i;
  } else {
    bool parity = (gDiff == 2);                                  // hard: search the chessboard squares first
    for (int pass = 0; pass < 2 && n == 0; pass++)
      for (int i = 0; i < N * N; i++)
        if (pShot[i / N][i % N] == 0 && (!parity || pass == 1 || (((i / N) + (i % N)) & 1) == 0)) list[n++] = i;
  }
  int k = list[rnd(n)];
  *pr = k / N;
  *pc = k % N;
}

static void reset() {
  placeFleet(eShip);
  placeFleet(pShip);
  memset(eShot, 0, sizeof eShot);
  memset(pShot, 0, sizeof pShot);
  memset(eHits, 0, sizeof eHits);
  memset(pHits, 0, sizeof pHits);
  shots = 0;
  eLeft = pLeft = 5;
  state = 0;
  setMsg(msgYou, &colYou, "Fire!", C_WHITE);
  setMsg(msgFoe, &colFoe, "", C_WHITE);
  hudBegin("Fleet");
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) { drawEnemy(r, c); drawMini(r, c); }
  text("Your fleet", 262, PF_Y + 8, 2, C_LGREY, MC_DATUM);
  drawStats();
  drawMsgs();
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && millis() - tTurn >= 500) reset();
    return;
  }
  if (state == 1) {
    if (millis() - tTurn < 650) return;
    int r, c;
    cpuPick(&r, &c);
    int res = shoot(pShip, pShot, pHits, r, c);
    for (int i = 0; i < N; i++)
      for (int j = 0; j < N; j++) if (pShot[i][j] == 3) drawMini(i, j);
    drawMini(r, c);
    if (res == 0) { setMsg(msgFoe, &colFoe, "Foe: miss", C_LGREY); beep(400, 40); }
    else if (res == 1) { setMsg(msgFoe, &colFoe, "Foe: HIT!", C_ORANGE); beep(250, 90); }
    else { pLeft--; setMsg(msgFoe, &colFoe, "Foe SUNK one!", C_RED); beep(180, 250); }
    drawStats();
    drawMsgs();
    if (pLeft == 0) {
      state = 2;
      tTurn = millis();
      char l2[28];
      snprintf(l2, sizeof l2, "Enemy had %d left", eLeft);
      overlayMsg("DEFEAT", C_RED, "Your fleet is sunk", l2, "Tap to play again");
      return;
    }
    state = 0;
    return;
  }
  // your turn
  if (!touch.tap || !inRect(touch.x, touch.y, GX, GY, N * CELL, N * CELL)) return;
  int c = clampi((touch.x - GX) / CELL, 0, N - 1), r = clampi((touch.y - GY) / CELL, 0, N - 1);
  if (eShot[r][c] != 0) return;                                   // already fired there
  shots++;
  int res = shoot(eShip, eShot, eHits, r, c);
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) if (eShot[i][j] == 3) drawEnemy(i, j);
  drawEnemy(r, c);
  if (res == 0) { setMsg(msgYou, &colYou, "You: miss", C_LGREY); beep(450, 40); }
  else if (res == 1) { setMsg(msgYou, &colYou, "You: HIT!", C_GREEN); beep(900, 90); }
  else { eLeft--; setMsg(msgYou, &colYou, "You SUNK one!", C_GOLD); beep(1300, 200); }
  setMsg(msgFoe, &colFoe, "", C_WHITE);
  drawStats();
  drawMsgs();
  if (eLeft == 0) {
    state = 2;
    tTurn = millis();
    bool nb = submitScore(shots);                                 // lower is better (flagged in GameDef)
    char l1[28], l2[28];
    snprintf(l1, sizeof l1, "Fleet sunk in %d shots", shots);
    if (nb) snprintf(l2, sizeof l2, "NEW BEST!"); else snprintf(l2, sizeof l2, "Best: %d shots", bestScore());
    beep(1250, 250);
    overlayMsg("VICTORY!", C_GREEN, l1, l2, "Tap to play again");
    return;
  }
  state = 1;
  tTurn = millis();
}

static void icon(int cx, int cy) {
  tft.fillRect(cx - 17, cy - 3, 34, 11, SHIP_COL);                // hull
  tft.fillRect(cx - 7, cy - 10, 14, 7, SHIP_COL);                 // bridge
  tft.fillRect(cx - 1, cy - 17, 2, 7, C_LGREY);                   // mast
  tft.fillRect(cx - 17, cy + 10, 34, 8, WATER);                   // water
  tft.fillCircle(cx + 9, cy + 1, 4, RGB565(255, 190, 40));        // a hit
  tft.fillCircle(cx + 9, cy + 1, 2, C_WHITE);
}

}  // namespace Fleet
