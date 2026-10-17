// Behaviour tests for the five newer games (Jumper, Frogger, Gems, Mastermind, Fleet): rules, scoring, AI strength,
// reachability and the full tap-driven game flows. (sim_main.cpp separately fuzzes every game for crashes / bad draws.)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "sim_env.h"
#include "../../firmware/CydGameHub/CydGameHub.ino"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static int gameIdx(const char* n) {
  for (int i = 0; i < NUM_GAMES; i++) if (!strcmp(GAMES[i].name, n)) return i;
  printf("FAIL: no game named %s\n", n); exit(1);
}
static void clearTouch() { touch = TouchState{false, false, false, false, -1, 160, 120, 160, 120, 0, 0}; }
static void tapAt(void (*tk)(), int x, int y) { touch = TouchState{false, true, true, true, -1, x, y, x, y, 0, 0}; tk(); clearTouch(); }
static void advance(void (*tk)(), int ms) { for (int t = 0; t < ms; t += 33) { g_millis += 33; clearTouch(); tk(); } }
static void start(const char* name, int diff, unsigned seed) {
  int i = gameIdx(name);
  gDiff = (uint8_t)diff; gCur = i; gLowerBetter = GAMES[i].lowerBetter;
  for (int k = 0; k < NUM_GAMES; k++) hiSet(k, 0);
  srand(seed); g_offscreen = 0; clearTouch();
  GAMES[i].init();
}

// ------------------------------------------------------------------------------------------------ Gems
static bool gemsClean() {                       // idle board: nothing empty, nothing matched
  for (int r = 0; r < Gems::ROWS; r++) for (int c = 0; c < Gems::COLS; c++) if (Gems::g[r][c] == Gems::EMPTY || Gems::g[r][c] >= Gems::ntypes) return false;
  return Gems::findMatches() == 0;
}
static bool findValidSwap(int* r1, int* c1, int* r2, int* c2) {
  using namespace Gems;
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
    if (c + 1 < COLS) { swapCells(r, c, r, c + 1); bool m = matchAt(r, c) || matchAt(r, c + 1); swapCells(r, c, r, c + 1); if (m) { *r1 = r; *c1 = c; *r2 = r; *c2 = c + 1; return true; } }
    if (r + 1 < ROWS) { swapCells(r, c, r + 1, c); bool m = matchAt(r, c) || matchAt(r + 1, c); swapCells(r, c, r + 1, c); if (m) { *r1 = r; *c1 = c; *r2 = r + 1; *c2 = c; return true; } }
  }
  return false;
}
static void gemsTests() {
  using namespace Gems;
  // generated boards: no ready-made match, at least one move, only valid types
  bool ok = true;
  for (int d = 0; d < 3; d++) for (int s = 0; s < 150; s++) { start("Gems", d, 100 + s); ok = ok && gemsClean() && hasMove() && moves == (d == 0 ? 30 : d == 1 ? 25 : 20) && ntypes == 5 + d; }
  CHECK(ok, "gems: every new board is match-free, playable, and difficulty sets types/moves (5-7 types, 30/25/20 moves)");
  // match detection on a crafted board: row of 4 and a column of 3 crossing + an unrelated pair
  start("Gems", 1, 1);
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) g[r][c] = (uint8_t)((r * 3 + c * 5 + (r / 2)) % 5 + ((r + c) % 2 ? 0 : 0));
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) g[r][c] = (uint8_t)(((r * 2 + c * 3) % 5));   // pattern without runs
  CHECK(findMatches() == 0, "gems: crafted run-free pattern has no matches");
  g[2][1] = g[2][2] = g[2][3] = g[2][4] = 6; g[0][3] = g[1][3] = 6;     // horizontal 4 crossing a vertical 3 at (2,3)
  int n = findMatches();
  CHECK(n == 6 && mk[2][1] && mk[2][4] && mk[0][3] && mk[1][3] && mk[2][3], "gems: 4-run and crossing 3-run are both marked (6 cells)");
  g[2][1] = g[2][2] = 5; g[2][3] = 4; CHECK(!matchAt(2, 1), "gems: a pair is not a match");
  // invalid swap does nothing; valid swap costs a move and cascades to a clean board
  start("Gems", 1, 7);
  int r1, c1, r2, c2;
  bool foundBad = false;
  for (int r = 0; r < ROWS && !foundBad; r++) for (int c = 0; c + 1 < COLS && !foundBad; c++) {
    swapCells(r, c, r, c + 1); bool m = matchAt(r, c) || matchAt(r, c + 1); swapCells(r, c, r, c + 1);
    if (!m) { foundBad = true; r1 = r; c1 = c; r2 = r; c2 = c + 1; }
  }
  uint8_t before[ROWS][COLS]; memcpy(before, g, sizeof g); int mv = moves;
  tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5); tapAt(Gems::tick, OX + c2 * CSZ + 5, OY + r2 * CSZ + 5);
  CHECK(foundBad && memcmp(before, g, sizeof g) == 0 && moves == mv && state == 0 && selR < 0, "gems: a swap that makes no match is undone for free");
  bool got = findValidSwap(&r1, &c1, &r2, &c2);
  tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5);
  CHECK(selR == r1 && selC == c1, "gems: first tap selects");
  tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5); CHECK(selR < 0, "gems: tapping the selected gem again deselects");
  tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5); tapAt(Gems::tick, OX + c2 * CSZ + 5, OY + r2 * CSZ + 5);
  CHECK(got && state == 1 && moves == mv - 1 && score > 0, "gems: valid swap uses a move, scores and starts clearing");
  advance(Gems::tick, 20000);
  CHECK(state == 0 && gemsClean() && score >= 30 && hasMove(), "gems: cascade settles into a clean board that still has a move");
  // swipe works too
  got = findValidSwap(&r1, &c1, &r2, &c2); mv = moves; int sc = score;
  int dir = (r2 > r1) ? 2 : 1;
  touch = TouchState{false, false, true, false, (int8_t)dir, OX + c1 * CSZ + 10, OY + r1 * CSZ + 10, OX + c1 * CSZ + 10, OY + r1 * CSZ + 10, 0, 0};
  Gems::tick(); clearTouch(); advance(Gems::tick, 20000);
  CHECK(got && moves == mv - 1 && score > sc && gemsClean(), "gems: swiping a gem towards its neighbour swaps them");
  // play whole games with a brute-force bot: invariants hold after every settle, game ends when moves run out
  bool inv = true, over = true; int games = 0;
  for (int d = 0; d < 3; d++) {
    start("Gems", d, 900 + d); int guard = 0;
    while (state != 3 && guard++ < 400) {
      if (state == 0) {
        if (!findValidSwap(&r1, &c1, &r2, &c2)) { inv = false; break; }
        tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5); tapAt(Gems::tick, OX + c2 * CSZ + 5, OY + r2 * CSZ + 5);
      }
      advance(Gems::tick, 12000);
      if (state == 0 && !gemsClean()) inv = false;
    }
    over = over && state == 3 && moves == 0 && score > 100 && hiGet(gCur) == score;
    games++;
  }
  CHECK(inv && over && games == 3, "gems: bot plays full games on all difficulties: board always clean, game over at 0 moves, score saved");
  start("Gems", 1, 5); moves = 1; findValidSwap(&r1, &c1, &r2, &c2);
  tapAt(Gems::tick, OX + c1 * CSZ + 5, OY + r1 * CSZ + 5); tapAt(Gems::tick, OX + c2 * CSZ + 5, OY + r2 * CSZ + 5); advance(Gems::tick, 20000);
  CHECK(state == 3, "gems: last move ends the game after the cascade settles");
  g_millis += 600; tapAt(Gems::tick, 100, 100); CHECK(state == 0 && score == 0 && moves == 25, "gems: tap after game over starts a new game");
  CHECK(g_offscreen < 1000, "gems: no bad draw calls");
}

// ------------------------------------------------------------------------------------------------ Mastermind
static void mindTests() {
  using namespace Mind;
  struct { uint8_t code[4], guess[4]; int ex, co; } T[] = {
    {{0, 1, 2, 3}, {0, 1, 2, 3}, 4, 0}, {{0, 1, 2, 3}, {1, 0, 3, 2}, 0, 4}, {{0, 0, 1, 1}, {0, 1, 0, 1}, 2, 2},
    {{0, 0, 0, 1}, {0, 0, 0, 0}, 3, 0}, {{1, 2, 3, 4}, {1, 1, 1, 1}, 1, 0}, {{0, 1, 2, 3}, {4, 4, 5, 5}, 0, 0},
    {{2, 2, 3, 3}, {3, 3, 2, 2}, 0, 4}, {{0, 1, 1, 2}, {1, 1, 0, 0}, 1, 2}, {{5, 5, 5, 5}, {5, 0, 0, 0}, 1, 0}};
  start("Mastermind", 1, 3);
  bool ok = true;
  for (auto& t : T) { memcpy(code, t.code, 4); int ex, co; feedback(t.guess, &ex, &co); if (ex != t.ex || co != t.co) { ok = false; printf("      code %d%d%d%d guess %d%d%d%d -> %d,%d want %d,%d\n", t.code[0], t.code[1], t.code[2], t.code[3], t.guess[0], t.guess[1], t.guess[2], t.guess[3], ex, co, t.ex, t.co); } }
  CHECK(ok, "mastermind: gold/grey feedback is right, incl. repeated colours");
  bool distinct = true, inRange = true, dup = false;
  for (int s = 0; s < 400; s++) {
    start("Mastermind", 0, s); for (int a = 0; a < 4; a++) { if (code[a] >= 5) inRange = false; for (int b = a + 1; b < 4; b++) if (code[a] == code[b]) distinct = false; }
    start("Mastermind", 1, s); for (int a = 0; a < 4; a++) { if (code[a] >= 6) inRange = false; for (int b = a + 1; b < 4; b++) if (code[a] == code[b]) dup = true; }
  }
  CHECK(distinct && inRange && dup, "mastermind: easy codes never repeat a colour (5 colours); normal codes use 6 colours and may repeat");
  start("Mastermind", 2, 1); CHECK(maxG == 8 && ncol == 6, "mastermind: hard = 8 guesses");
  start("Mastermind", 0, 1); CHECK(maxG == 10 && ncol == 5, "mastermind: easy = 10 guesses, 5 colours");
  auto pal = [&](int i) { tapAt(Mind::tick, PAL_X + (i % 3) * PAL_DX, PAL_Y + (i / 3) * PAL_DY); };
  auto okBtn = [&]() { tapAt(Mind::tick, OK_X + 10, BTN_Y + 10); };
  // OK does nothing until all four slots are filled; DEL and slot taps clear; a 5th colour is ignored
  start("Mastermind", 1, 11); pal(0); pal(1); okBtn(); CHECK(nGuess == 0, "mastermind: OK is ignored with an unfinished row");
  tapAt(Mind::tick, DEL_X + 10, BTN_Y + 10); CHECK(cur[1] == NOPEG && cur[0] == 0, "mastermind: DEL removes the last peg");
  pal(2); pal(3); pal(4); pal(5); CHECK(cur[0] == 0 && cur[1] == 2 && cur[2] == 3 && cur[3] == 4, "mastermind: pegs fill left to right; extra taps ignored when full");
  tapAt(Mind::tick, SLOT_X + 1 * SLOT_DX, SLOT_Y); CHECK(cur[1] == NOPEG, "mastermind: tapping a slot empties it");
  // win: copy the code in
  start("Mastermind", 1, 21); int idx = gCur;
  for (int k = 0; k < 4; k++) pal(code[k]); okBtn();
  CHECK(state == 1 && nGuess == 1 && hiGet(idx) == 1 && fbx[0] == 4, "mastermind: entering the code wins, the guess count is the score");
  g_millis += 600; tapAt(Mind::tick, 100, 100); CHECK(state == 0 && nGuess == 0, "mastermind: tap starts a new code");
  // win on the 3rd guess keeps the best (lowest) score; a worse later win does not replace it
  start("Mastermind", 1, 22);
  for (int g2 = 0; g2 < 2; g2++) { uint8_t wrong = (uint8_t)((code[0] + 1) % 6); for (int k = 0; k < 4; k++) pal(wrong); okBtn(); }
  for (int k = 0; k < 4; k++) pal(code[k]); okBtn();
  CHECK(state == 1 && hiGet(gCur) == 3, "mastermind: win in 3 guesses records 3");
  // lose: every guess wrong
  start("Mastermind", 2, 23);
  for (int g2 = 0; g2 < 8; g2++) { uint8_t wrong = 0; for (int c = 0; c < 6; c++) { bool used = false; for (int k = 0; k < 4; k++) used = used || code[k] == c; if (!used) wrong = (uint8_t)c; }
    if (wrong == code[0]) wrong = (uint8_t)((wrong + 1) % 6);
    for (int k = 0; k < 4; k++) pal(wrong); okBtn(); }
  CHECK(state == 1 && hiGet(gCur) == 0 && nGuess == 8, "mastermind: running out of guesses ends the game with no score");
  // all-colour bot: a consistent-guess strategy must solve every code within 10 guesses on normal (sanity of the feedback)
  bool solved = true; int worst = 0;
  for (int s = 0; s < 60; s++) {
    start("Mastermind", 1, 500 + s);
    uint8_t secret[4]; memcpy(secret, code, 4);
    int tries = 0; bool win = false;
    while (state == 0 && tries < 10) {
      // find any guess consistent with all earlier feedback
      uint8_t cand[4]; bool found = false;
      for (int v = 0; v < 1296 && !found; v++) {
        int x = v; for (int k = 0; k < 4; k++) { cand[k] = (uint8_t)(x % 6); x /= 6; }
        bool consistent = true;
        for (int h = 0; h < nGuess && consistent; h++) { int ex, co; uint8_t save[4]; memcpy(save, code, 4); memcpy(code, gs[h], 4); feedback(cand, &ex, &co); memcpy(code, save, 4); if (ex != fbx[h] || co != fbc[h]) consistent = false; }
        found = consistent;
      }
      for (int k = 0; k < 4; k++) pal(cand[k]); okBtn(); tries++;
      if (nGuess > 0 && fbx[nGuess - 1] == 4) win = true;
    }
    if (!win) solved = false;
    worst = imax(worst, tries);
  }
  CHECK(solved && worst <= 8, "mastermind: consistent-guess bot cracks 60 random codes (feedback is self-consistent)");
  CHECK(g_offscreen < 1000, "mastermind: no bad draw calls");
}

// ------------------------------------------------------------------------------------------------ Fleet
static bool fleetLayoutOk(uint8_t s[Fleet::N][Fleet::N]) {
  using namespace Fleet;
  for (int id = 1; id <= 5; id++) {
    int cnt = 0, minr = 99, maxr = -1, minc = 99, maxc = -1;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (s[r][c] == id) { cnt++; minr = imin(minr, r); maxr = imax(maxr, r); minc = imin(minc, c); maxc = imax(maxc, c); }
    if (cnt != LEN[id]) return false;
    if (!((minr == maxr && maxc - minc + 1 == cnt) || (minc == maxc && maxr - minr + 1 == cnt))) return false;
  }
  int total = 0;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) total += s[r][c] != 0;
  return total == 17;
}
static double cpuAverage(int diff, int games, int* longest) {
  using namespace Fleet;
  gDiff = (uint8_t)diff; long sum = 0; *longest = 0;
  for (int gm = 0; gm < games; gm++) {
    placeFleet(pShip); memset(pShot, 0, sizeof pShot); memset(pHits, 0, sizeof pHits);
    int left = 5, n = 0;
    while (left > 0 && n < 70) {
      int r, c; cpuPick(&r, &c);
      if (pShot[r][c] != 0) return -1;                    // fired at the same square twice
      if (shoot(pShip, pShot, pHits, r, c) == 2) left--;
      n++;
    }
    if (left > 0) return -2;
    sum += n; *longest = imax(*longest, n);
  }
  return (double)sum / games;
}
static void fleetTests() {
  using namespace Fleet;
  start("Fleet", 1, 1);
  bool ok = true;
  for (int i = 0; i < 1500; i++) { placeFleet(eShip); ok = ok && fleetLayoutOk(eShip); }
  CHECK(ok, "fleet: 1500 random fleets: five straight ships of 5,4,3,3,2 that never overlap");
  // shooting rules on a hand-made board
  uint8_t sh[N][N], st[N][N], hits[6]; memset(sh, 0, sizeof sh); memset(st, 0, sizeof st); memset(hits, 0, sizeof hits);
  sh[0][0] = sh[0][1] = 5; sh[3][3] = sh[4][3] = sh[5][3] = 4 /* pretend a 3-long ship uses id 4's slot: only id 5 is checked below */;
  CHECK(shoot(sh, st, hits, 7, 7) == 0 && st[7][7] == 1, "fleet: miss");
  CHECK(shoot(sh, st, hits, 0, 0) == 1 && st[0][0] == 2, "fleet: hit");
  CHECK(shoot(sh, st, hits, 0, 1) == 2 && st[0][0] == 3 && st[0][1] == 3, "fleet: last square of a ship sinks it and marks the whole ship");
  // CPU strength
  int l0, l1, l2;
  double a0 = cpuAverage(0, 600, &l0), a1 = cpuAverage(1, 600, &l1), a2 = cpuAverage(2, 600, &l2);
  printf("      CPU shots to sink the fleet: easy %.1f, normal %.1f, hard %.1f (worst %d/%d/%d)\n", a0, a1, a2, l0, l1, l2);
  CHECK(a0 > 0 && a1 > 0 && a2 > 0, "fleet: the CPU never fires at the same square twice and always finishes within 64 shots");
  CHECK(a0 > 52 && a1 < a0 - 8 && a2 < a1 - 3 && a2 > 28, "fleet: easy ~random (>52), normal clearly better, hard better again but not perfect");
  // UI flow: tap squares, CPU answers, games end; player's win records its shot count
  int wins = 0, losses = 0, ended = 0;
  for (int d = 0; d < 3; d++) for (int gm = 0; gm < 12; gm++) {
    start("Fleet", d, 4000 + d * 100 + gm);
    int idx = gCur, guard = 0;
    while (state != 2 && guard++ < 400) {
      if (state == 0) {
        int r = rnd(N), c = rnd(N);
        if (eShot[r][c] != 0) continue;
        int before = shots;
        tapAt(Fleet::tick, GX + c * CELL + 4, GY + r * CELL + 4);
        if (shots != before + 1) { printf("      shot not counted\n"); fails++; }
        tapAt(Fleet::tick, GX + c * CELL + 4, GY + r * CELL + 4);                // same square again: ignored
        if (shots != before + 1) { printf("      repeat shot counted\n"); fails++; }
      }
      advance(Fleet::tick, 700);
    }
    if (state == 2) { ended++; if (eLeft == 0) { wins++; if (hiGet(idx) != shots) fails++; } else { losses++; if (hiGet(idx) != 0 && eLeft != 0) fails++; } }
  }
  CHECK(ended == 36, "fleet: 36 tap-driven games all finish");
  printf("      (%d wins, %d losses against random shooting)\n", wins, losses);
  CHECK(wins > 0, "fleet: random shooting can win; the shot count is saved as the score");
  start("Fleet", 1, 77); for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (eShip[r][c] == 0) { tapAt(Fleet::tick, GX + c * CELL + 4, GY + r * CELL + 4); advance(Fleet::tick, 700); if (state == 2) break; }
  CHECK(g_offscreen < 1000, "fleet: no bad draw calls");
}

// ------------------------------------------------------------------------------------------------ Frogger
static bool frogLanesOk() {
  using namespace Frogger;
  for (int r = 1; r <= 7; r++) {
    const Lane& l = ln[r];
    if (!l.kind) continue;
    if (l.period < SCR_W + l.w) return false;
    for (int k = 0; k < l.n; k++) {
      if (l.x[k] < -l.w || l.x[k] >= l.period - l.w) return false;
      for (int j = k + 1; j < l.n; j++) {                                  // objects of one lane never overlap (circular)
        int d = iabs(l.x[k] - l.x[j]); d = imin(d, l.period - d);
        if (d < l.w + 8) return false;
      }
    }
  }
  return true;
}
static void frogTests() {
  using namespace Frogger;
  bool ok = true;
  for (int d = 0; d < 3; d++) {
    start("Frogger", d, 50 + d);
    for (int t = 0; t < 3000; t++) { g_millis += 33; clearTouch(); Frogger::tick(); if (!frogLanesOk()) ok = false; if (state != 0) { state = 0; lives = 3; } }
  }
  CHECK(ok, "frogger: lane objects stay inside their wrap range and never overlap, on every difficulty");
  start("Frogger", 1, 8);
  CHECK(frow == 8 && lives == 3 && score == 0, "frogger: starts on the bottom row with three lives");
  // find a river position without a log, and one with a log
  auto onLogAt = [&](int row, int x) { const Lane& l = ln[row]; int cx = x + FW / 2; for (int k = 0; k < l.n; k++) if (cx >= l.x[k] + 2 && cx < l.x[k] + l.w - 2) return true; return false; };
  int water = -1, logx = -1;
  for (int x = 0; x <= SCR_W - FW; x++) { if (!onLogAt(1, x) && water < 0) water = x; if (onLogAt(1, x) && logx < 0 && x > 30) logx = x + 4; }
  frow = 1; fx = logx; checkFrog(); CHECK(state == 0 && lives == 3, "frogger: standing on a log is safe");
  frow = 1; fx = water; checkFrog(); CHECK(state == 1 && lives == 2, "frogger: water costs a life");
  // road: put the frog on a car
  start("Frogger", 1, 9);
  { const Lane& l = ln[6]; int cx = l.x[0]; if (cx < 0 || cx > SCR_W - FW) { for (int k = 0; k < l.n; k++) if (l.x[k] >= 0 && l.x[k] < SCR_W - FW) cx = l.x[k]; } frow = 6; fx = clampi(cx + 4, 0, SCR_W - FW); }
  checkFrog(); CHECK(state == 1 && lives == 2, "frogger: a car costs a life");
  { int gx = -1; for (int x = 0; x <= SCR_W - FW && gx < 0; x++) { bool hit = false; const Lane& l = ln[6]; for (int k = 0; k < l.n; k++) if (x + 3 < l.x[k] + l.w - 2 && x + FW - 3 > l.x[k] + 2) hit = true; if (!hit) gx = x; }
    start("Frogger", 1, 9); frow = 6; fx = gx; checkFrog(); CHECK(state == 0 && lives == 3, "frogger: a gap in the traffic is safe"); }
  // carried by a log: the frog moves exactly with the log
  start("Frogger", 1, 10);
  frow = 2; { const Lane& l = ln[2]; for (int k = 0; k < l.n; k++) if (l.x[k] > 20 && l.x[k] + l.w < SCR_W - 20) fx = l.x[k] + l.w / 2 - FW / 2; }
  int dirL = ln[2].dir, moved = 0, bad = 0;
  for (int t = 0; t < 12; t++) { int x0 = fx; g_millis += 33; clearTouch(); Frogger::tick(); if (state != 0) break; moved += iabs(fx - x0); if (fx - x0 != dirL * ln[2].dxNow) bad++; }
  CHECK(bad == 0 && moved > 0, "frogger: a log carries the frog sideways at the log's speed");
  // carried off the edge = death
  start("Frogger", 1, 10); frow = 2; fx = 0;
  { Lane& l = ln[2]; l.dir = -1; l.x[0] = -10; l.w = 60; l.n = 1; l.s = 400; l.period = 400; l.sp = 8; fx = 4; }
  for (int t = 0; t < 40 && state == 0; t++) { g_millis += 33; clearTouch(); Frogger::tick(); }
  CHECK(state == 1, "frogger: riding a log off the screen costs a life");
  // crossing: tap up from the log row onto the top row scores and resets the frog
  start("Frogger", 1, 12);
  frow = 1; { const Lane& l = ln[1]; for (int k = 0; k < l.n; k++) if (l.x[k] > 30 && l.x[k] + l.w < SCR_W - 30) fx = l.x[k] + l.w / 2 - FW / 2; }
  tapAt(Frogger::tick, fx + FW / 2, rowY(0) + 6);
  CHECK(score == 1 && frow == 8 && level == 1 && state == 0, "frogger: reaching the top scores 1 and sends the frog back to the start");
  // swipes and taps in every direction
  start("Frogger", 1, 13); fx = 150;
  ln[7].kind = 0; ln[7].n = 0; ln[6].kind = 0; ln[6].n = 0;
  tapAt(Frogger::tick, fx + FW / 2 + 40, rowY(frow) + RH / 2); CHECK(fx == 174 && frow == 8, "frogger: tap to the right hops right one cell");
  tapAt(Frogger::tick, fx + FW / 2 - 40, rowY(frow) + RH / 2); CHECK(fx == 150 && frow == 8, "frogger: tap to the left hops left");
  touch = TouchState{false, false, true, false, 0, 100, 100, 100, 150, 0, 0}; Frogger::tick(); clearTouch(); CHECK(frow == 7, "frogger: swipe up hops up");
  touch = TouchState{false, false, true, false, 2, 100, 150, 100, 100, 0, 0}; Frogger::tick(); clearTouch(); CHECK(frow == 8, "frogger: swipe down hops down");
  for (int i = 0; i < 20; i++) { touch = TouchState{false, false, true, false, 3, 100, 100, 150, 100, 0, 0}; Frogger::tick(); clearTouch(); }
  CHECK(fx == 0, "frogger: the frog cannot leave the left edge");
  // three lives -> game over, score saved, tap restarts
  start("Frogger", 1, 14); score = 4; int fi = gCur;
  for (int life = 0; life < 3; life++) { killFrog(); advance(Frogger::tick, 800); }
  CHECK(state == 2 && hiGet(fi) == 4, "frogger: after the third life the game ends and keeps the score");
  g_millis += 600; tapAt(Frogger::tick, 100, 100); CHECK(state == 0 && lives == 3 && score == 0, "frogger: tap restarts");
  CHECK(g_offscreen < 1000, "frogger: no bad draw calls");
}

// ------------------------------------------------------------------------------------------------ Jumper
static int framesToLand(float feet, float v, int y) {            // frames until the feet come down onto height y (-1: never)
  for (int n = 1; n < 120; n++) { v += Jumper::GRAV; float prev = feet; feet += v; if (v > 0 && prev <= y + 2 && feet >= y) return n; }
  return -1;
}
static void jumperBot(int* steerX) {
  using namespace Jumper;
  float feet = py + JS;
  int best = -1, bestY = 9999, near = -1, nearD = 9999;
  for (int i = 0; i < MAXP; i++) {
    if (pl[i].kind == UNUSED || pl[i].kind == 2) continue;
    int n = framesToLand(feet, vy, pl[i].y);
    if (n < 0) continue;
    int cx = pl[i].x + pw / 2, d = iabs(cx - ((int)px + JS / 2));
    int slack = d - (pw / 2 + JS / 2 - 6);
    if (slack < 0) slack = 0;
    if (slack <= 6 * n && pl[i].y < bestY) { bestY = pl[i].y; best = i; }
    if (d < nearD) { nearD = d; near = i; }
  }
  if (best < 0) best = near;
  if (best < 0) { *steerX = (int)px + JS / 2; return; }
  *steerX = pl[best].x + pw / 2;
}
static void jumperTests() {
  using namespace Jumper;
  for (int d = 0; d < 3; d++) {
    int scores[9]; bool inv = true; int maxGap = 0;
    for (int sd = 0; sd < 9; sd++) {
      start("Jumper", d, 300 + d * 20 + sd);
      touch.pressed = true; Jumper::tick(); clearTouch();                                  // leave the ready state
      for (int t = 0; t < 9000 && state == 1; t++) {
        int sx; jumperBot(&sx);
        g_millis += 33; touch = TouchState{true, false, false, false, -1, sx, 150, sx, 150, 0, 0}; Jumper::tick();
        if (activeCount() > MAXP) inv = false;
        int ys[MAXP], n = 0; for (int i = 0; i < MAXP; i++) if (pl[i].kind != UNUSED && pl[i].kind != 2) ys[n++] = pl[i].y;
        for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) if (ys[b] < ys[a]) { int gap = ys[a] - ys[b]; bool mid = false; for (int c = 0; c < n; c++) if (ys[c] < ys[a] && ys[c] > ys[b]) mid = true; if (!mid) maxGap = imax(maxGap, gap); }
      }
      scores[sd] = score;
    }
    clearTouch();
    for (int a = 0; a < 9; a++) for (int b = a + 1; b < 9; b++) if (scores[b] < scores[a]) { int t = scores[a]; scores[a] = scores[b]; scores[b] = t; }
    printf("      %s: simple steering bot, median %d m (best %d, worst %d), largest gap in the platform chain %d px\n", DIFF_NAMES[d], scores[4], scores[8], scores[0], maxGap);
    CHECK(inv && scores[4] >= 250, "jumper: a simple steering bot climbs high (platforms are reachable)");
    CHECK(maxGap <= 100, "jumper: platform gaps stay within the jump height (~110 px)");
  }
  // no input: the jumper eventually falls, the game ends, the height is saved
  start("Jumper", 1, 9); int idx = gCur;
  touch.pressed = true; Jumper::tick(); clearTouch();
  for (int t = 0; t < 1500 && state == 1; t++) { g_millis += 33; touch = TouchState{true, false, false, false, -1, 0, 150, 0, 150, 0, 0}; Jumper::tick(); }
  clearTouch();
  CHECK(state == 2, "jumper: steering away from every platform ends the game");
  int sc = score; CHECK(hiGet(idx) == sc || sc == 0, "jumper: height is saved as the score");
  g_millis += 600; tapAt(Jumper::tick, 100, 100); CHECK(state == 0 && score == 0, "jumper: tap restarts");
  // crumbling platforms: landing on one removes it and gives no bounce; sliding ones stay on screen
  start("Jumper", 2, 33);
  for (int i = 0; i < MAXP; i++) pl[i].kind = UNUSED;
  pw = 46; pl[0] = Plat{100, 200, 1, 2}; px = 105; py = 150; vy = 4; state = 1;
  for (int t = 0; t < 30; t++) { g_millis += 33; clearTouch(); Jumper::tick(); if (pl[0].kind == UNUSED) break; }
  CHECK(pl[0].kind == UNUSED, "jumper: a crumbling platform breaks when landed on");
  start("Jumper", 2, 34); state = 1; bool slide = true;
  for (int t = 0; t < 500 && state == 1; t++) { g_millis += 33; touch = TouchState{true, false, false, false, -1, (int)px + 9, 150, 0, 0, 0, 0}; Jumper::tick(); for (int i = 0; i < MAXP; i++) if (pl[i].kind == 1 && (pl[i].x < 0 || pl[i].x > SCR_W - pw)) slide = false; }
  CHECK(slide, "jumper: sliding platforms bounce off the screen edges");
  CHECK(g_offscreen < 1000, "jumper: no bad draw calls");
}

int main() {
  prefs.begin("sim"); settingsLoad(); cal = TouchCal{0, 400, 3700, 400, 3700};
  g_millis = 100000;
  gemsTests();
  mindTests();
  fleetTests();
  frogTests();
  jumperTests();
  printf("\n%s (%d failures)\n", fails ? "GAMES TEST FAILED" : "games test passed", fails);
  return fails ? 1 : 0;
}
