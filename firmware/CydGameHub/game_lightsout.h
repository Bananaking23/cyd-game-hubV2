// Lights Out - tap a lamp to toggle it and its 4 neighbours. Switch every lamp off. Fewer moves = better.
#pragma once
#include "hub.h"

namespace LightsOut {

static const int N = 5, CS = 38, PITCH = 42;
static const int OX = 10, OY = PF_Y + 6;                    // grid spans x 10..219, y 28..237
static const int BX = 232, BY = 172, BWID = 80, BHGT = 44;  // NEW button

static const uint16_t LAMP_ON   = RGB565(255, 214, 30);
static const uint16_t LAMP_GLOW = RGB565(255, 244, 150);
static const uint16_t LAMP_OFF  = RGB565(30, 36, 54);
static const uint16_t LAMP_RIM  = RGB565(62, 70, 96);

static uint8_t lamp[N][N];
static int moves, state;      // state: 0 playing, 1 solved (waiting for tap)
static uint32_t tWin;

static void drawLamp(int r, int c) {
  int x = OX + c * PITCH;
  int y = OY + r * PITCH;
  if (lamp[r][c]) {
    tft.fillRoundRect(x, y, CS, CS, 7, LAMP_ON);
    tft.fillRoundRect(x + 6, y + 6, CS - 12, CS - 12, 5, LAMP_GLOW);
  } else {
    tft.fillRoundRect(x, y, CS, CS, 7, LAMP_OFF);
    tft.drawRoundRect(x, y, CS, CS, 7, LAMP_RIM);
  }
}

static void flip(int r, int c) {
  if (r < 0 || c < 0 || r >= N || c >= N) return;
  lamp[r][c] ^= 1;
}

static void press(int r, int c) {
  flip(r, c);
  flip(r - 1, c);
  flip(r + 1, c);
  flip(r, c - 1);
  flip(r, c + 1);
}

static int countLit() {
  int n = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) n += lamp[r][c];
  return n;
}

static void drawAll() {
  pfClear(C_BLACK);
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) drawLamp(r, c);
  text("Turn all", 272, PF_Y + 36, 2, C_LGREY, MC_DATUM);
  text("lamps off", 272, PF_Y + 56, 2, C_LGREY, MC_DATUM);
  if (bestScore() > 0) {
    char s[20];
    snprintf(s, sizeof s, "Best: %d", bestScore());
    text(s, 272, PF_Y + 100, 2, C_GOLD, MC_DATUM);
  }
  button(BX, BY, BWID, BHGT, "NEW", RGB565(30, 80, 170));
}

static void newPuzzle() {
  int lit;
  do {
    memset(lamp, 0, sizeof lamp);
    uint8_t used[N * N];
    memset(used, 0, sizeof used);
    int presses = rndRange(byDiff(4, 8, 14), byDiff(6, 12, 18));   // easy 4-6, normal 8-12, hard 14-18
    for (int i = 0; i < presses; i++) {      // distinct cells: always solvable, <= 18 moves
      int k;
      do { k = rnd(N * N); } while (used[k]);
      used[k] = 1;
      press(k / N, k % N);
    }
    lit = countLit();
  } while (lit < 4);                          // never trivial / already solved
  moves = 0;
  state = 0;
  drawAll();
  hudScore(moves, "Moves");
}

static void init() {
  hudBegin("Lights Out");
  newPuzzle();
}

static void tick() {
  if (state == 1) {
    if (touch.pressed && millis() - tWin > 500) newPuzzle();
    return;
  }
  if (!touch.tap) return;
  int x = touch.x, y = touch.y;

  if (inRect(x, y, BX - 8, BY - 8, BWID + 16, BHGT + 8)) {
    beep(700, 30);
    newPuzzle();
    return;
  }
  if (!inRect(x, y, OX, OY, N * PITCH, N * PITCH)) return;

  int c = clampi((x - OX) / PITCH, 0, N - 1);   // the 4px gaps belong to the lamp on their left/top
  int r = clampi((y - OY) / PITCH, 0, N - 1);
  press(r, c);
  moves++;
  drawLamp(r, c);
  if (r > 0) drawLamp(r - 1, c);
  if (r < N - 1) drawLamp(r + 1, c);
  if (c > 0) drawLamp(r, c - 1);
  if (c < N - 1) drawLamp(r, c + 1);
  hudScore(moves, "Moves");
  beep(lamp[r][c] ? 900 : 600, 25);

  if (countLit() == 0) {
    state = 1;
    tWin = millis();
    beep(1250, 250);
    bool nb = submitScore(moves);              // lower is better (flagged in GameDef)
    char l1[28], l2[28];
    snprintf(l1, sizeof l1, "Solved in %d %s", moves, moves == 1 ? "move" : "moves");
    if (nb) snprintf(l2, sizeof l2, "NEW BEST!");
    else    snprintf(l2, sizeof l2, "Best: %d moves", bestScore());
    overlayMsg("SOLVED!", C_GREEN, l1, l2, "Tap for a new puzzle");
  }
}

static void icon(int cx, int cy) {
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) {
      bool on = (r == 1 || c == 1);               // plus shape = one press
      int x = cx - 17 + c * 12;
      int y = cy - 17 + r * 12;
      tft.fillRoundRect(x, y, 10, 10, 2, on ? LAMP_ON : LAMP_OFF);
    }
}

}  // namespace LightsOut
