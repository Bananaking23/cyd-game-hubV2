// Mastermind - crack the secret code of 4 coloured pegs. Tap colours to fill the row, OK to guess.
// Gold pin = right colour in the right place, grey pin = right colour in the wrong place. Fewer guesses = better.
// Easy: 5 colours, no repeats. Normal: 6 colours, repeats allowed. Hard: the same with fewer guesses.
#pragma once
#include "hub.h"

namespace Mind {

static const int NP = 4, MAXROWS = 10, ROWH = 21, LX = 16, LP = 24, NOPEG = 255;
static const int PAL_X = 190, PAL_Y = PF_Y + 30, PAL_DX = 48, PAL_DY = 44, PAL_R = 15;   // 3 x 2 colour buttons
static const int SLOT_X = 182, SLOT_DX = 38, SLOT_Y = PF_Y + 130, SLOT_R = 13;           // the row being built
static const int DEL_X = 172, OK_X = 244, BTN_Y = 176, BTN_W = 64, BTN_H = 32;
static const uint16_t PCOL[6] = {C_RED, C_GREEN, RGB565(70, 140, 255), C_YELLOW, RGB565(176, 110, 255), C_ORANGE};
static const uint16_t RIM = RGB565(70, 78, 100), PIN_OFF = RGB565(40, 46, 62);

static uint8_t code[NP], cur[NP], gs[MAXROWS][NP], fbx[MAXROWS], fbc[MAXROWS];
static int ncol, maxG, nGuess, state;          // state: 0 playing, 1 finished (waiting for a tap)
static uint32_t tEnd;

static int rowY(int i) { return PF_Y + 4 + i * ROWH; }

static void peg(int cx, int cy, int r, uint16_t c) {
  tft.fillCircle(cx, cy, r, c);
  tft.fillCircle(cx - r / 3, cy - r / 3, imax(1, r / 5), C_WHITE);
}
static void hollow(int cx, int cy, int r) {
  tft.fillCircle(cx, cy, r, C_BLACK);
  tft.drawCircle(cx, cy, r, RIM);
}

static void drawRow(int i) {
  int y = rowY(i);
  tft.fillRect(0, y, 156, ROWH, C_BLACK);
  for (int k = 0; k < NP; k++) {
    int cx = LX + k * LP, cy = y + 9;
    if (i < nGuess) peg(cx, cy, 7, PCOL[gs[i][k] % 6]); else hollow(cx, cy, 7);
  }
  for (int j = 0; j < 4; j++) {
    int px = 124 + (j % 2) * 10, py = y + 5 + (j / 2) * 9;
    if (i < nGuess) tft.fillCircle(px, py, 3, j < fbx[i] ? C_GOLD : (j < fbx[i] + fbc[i] ? C_LGREY : PIN_OFF));
    else tft.drawCircle(px, py, 3, PIN_OFF);
  }
  if (i == nGuess && state == 0) tft.fillTriangle(0, y + 5, 0, y + 13, 5, y + 9, C_YELLOW);   // the active row
}
static void drawSlot(int k) {
  int cx = SLOT_X + k * SLOT_DX;
  tft.fillRect(cx - SLOT_R - 1, SLOT_Y - SLOT_R - 1, 2 * SLOT_R + 3, 2 * SLOT_R + 3, C_BLACK);
  if (cur[k] == NOPEG) hollow(cx, SLOT_Y, SLOT_R); else peg(cx, SLOT_Y, SLOT_R, PCOL[cur[k] % 6]);
}
static bool curFull() {
  for (int k = 0; k < NP; k++) if (cur[k] == NOPEG) return false;
  return true;
}
static void drawOk() { button(OK_X, BTN_Y, BTN_W, BTN_H, "OK", curFull() ? RGB565(30, 130, 70) : RGB565(46, 56, 50)); }
static void hud() {
  char b[28];
  snprintf(b, sizeof b, "Guess %d of %d", imin(nGuess + 1, maxG), maxG);
  hudText(b);
}
static void drawPalette() {
  tft.fillRect(166, PF_Y + 2, 154, 106, C_BLACK);
  for (int i = 0; i < ncol; i++) peg(PAL_X + (i % 3) * PAL_DX, PAL_Y + (i / 3) * PAL_DY, PAL_R, PCOL[i]);
}

static void newCode() {
  if (gDiff == 0) {                                  // easy: all different
    uint8_t pool[6] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < ncol; i++) { int j = rnd(ncol); uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    for (int k = 0; k < NP; k++) code[k] = pool[k];
  } else {
    for (int k = 0; k < NP; k++) code[k] = (uint8_t)rnd(ncol);
  }
}

static void reset() {
  ncol = byDiff(5, 6, 6);
  maxG = byDiff(10, 10, 8);
  nGuess = 0;
  state = 0;
  newCode();
  for (int k = 0; k < NP; k++) cur[k] = NOPEG;
  hudBegin("Mastermind");
  for (int i = 0; i < maxG; i++) drawRow(i);
  tft.drawFastVLine(160, PF_Y + 2, PF_H - 4, RIM);
  drawPalette();
  for (int k = 0; k < NP; k++) drawSlot(k);
  button(DEL_X, BTN_Y, BTN_W, BTN_H, "DEL", RGB565(150, 50, 50));
  drawOk();
  text("gold = right spot", 168, 214, 1, C_GOLD);
  text("grey = right colour", 168, 224, 1, C_LGREY);
  hud();
}

static void feedback(const uint8_t* guess, int* exact, int* colour) {
  int cc[6] = {0, 0, 0, 0, 0, 0}, gc[6] = {0, 0, 0, 0, 0, 0}, ex = 0;
  for (int k = 0; k < NP; k++) {
    if (guess[k] == code[k]) ex++;
    else { cc[code[k] % 6]++; gc[guess[k] % 6]++; }
  }
  int co = 0;
  for (int i = 0; i < 6; i++) co += imin(cc[i], gc[i]);
  *exact = ex;
  *colour = co;
}

static void finish(bool won) {
  state = 1;
  tEnd = millis();
  if (nGuess < maxG) drawRow(nGuess);                // remove the active-row marker
  char l1[28], l2[28];
  if (won) {
    bool nb = submitScore(nGuess);                   // lower is better (flagged in GameDef)
    snprintf(l1, sizeof l1, "Cracked in %d %s", nGuess, nGuess == 1 ? "guess" : "guesses");
    if (nb) snprintf(l2, sizeof l2, "NEW BEST!"); else snprintf(l2, sizeof l2, "Best: %d guesses", bestScore());
    beep(1250, 250);
    overlayMsg("CRACKED!", C_GREEN, l1, l2, "Tap for a new code");
  } else {
    tft.fillRect(166, PF_Y + 2, 154, 50, C_BLACK);   // show the code above the message box
    text("The code was", 241, PF_Y + 9, 2, C_WHITE, MC_DATUM);
    for (int k = 0; k < NP; k++) peg(SLOT_X + k * SLOT_DX, PF_Y + 36, 11, PCOL[code[k] % 6]);
    beep(200, 300);
    overlayMsg("OUT OF GUESSES", C_RED, "So close!", nullptr, "Tap for a new code");
  }
}

static void submit() {
  int ex, co;
  for (int k = 0; k < NP; k++) gs[nGuess][k] = cur[k];
  feedback(cur, &ex, &co);
  fbx[nGuess] = (uint8_t)ex;
  fbc[nGuess] = (uint8_t)co;
  nGuess++;
  drawRow(nGuess - 1);
  for (int k = 0; k < NP; k++) { cur[k] = NOPEG; drawSlot(k); }
  drawOk();
  if (ex == NP) { finish(true); return; }
  if (nGuess >= maxG) { finish(false); return; }
  drawRow(nGuess);
  hud();
  beep(ex > 0 ? 1000 : 600, 30);
}

static void init() { reset(); }

static void tick() {
  if (state == 1) {
    if (touch.pressed && millis() - tEnd >= 500) reset();
    return;
  }
  if (!touch.tap) return;
  int x = touch.x, y = touch.y;
  for (int i = 0; i < ncol; i++) {                   // colour buttons: fill the first empty slot
    int cx = PAL_X + (i % 3) * PAL_DX, cy = PAL_Y + (i / 3) * PAL_DY;
    if (!inRect(x, y, cx - 23, cy - 21, 46, 42)) continue;
    for (int k = 0; k < NP; k++)
      if (cur[k] == NOPEG) {
        cur[k] = (uint8_t)i;
        drawSlot(k);
        if (curFull()) drawOk();
        beep(800 + i * 60, 15);
        break;
      }
    return;
  }
  for (int k = 0; k < NP; k++) {                     // tap a filled slot to empty it
    if (inRect(x, y, SLOT_X + k * SLOT_DX - 18, SLOT_Y - 18, 36, 36)) {
      if (cur[k] != NOPEG) { bool was = curFull(); cur[k] = NOPEG; drawSlot(k); if (was) drawOk(); beep(500, 15); }
      return;
    }
  }
  if (inRect(x, y, DEL_X - 4, BTN_Y - 4, BTN_W + 8, BTN_H + 8)) {
    for (int k = NP - 1; k >= 0; k--)
      if (cur[k] != NOPEG) { bool was = curFull(); cur[k] = NOPEG; drawSlot(k); if (was) drawOk(); beep(500, 15); break; }
    return;
  }
  if (inRect(x, y, OK_X - 4, BTN_Y - 4, BTN_W + 8, BTN_H + 8) && curFull()) submit();
}

static void icon(int cx, int cy) {
  for (int k = 0; k < 4; k++) tft.fillCircle(cx - 15 + k * 10, cy - 8, 4, PCOL[k]);
  for (int k = 0; k < 4; k++) tft.fillCircle(cx - 15 + k * 10, cy + 6, 4, PCOL[(k * 2 + 1) % 6]);
  tft.fillCircle(cx - 11, cy + 16, 2, C_GOLD);
  tft.fillCircle(cx - 4, cy + 16, 2, C_GOLD);
  tft.fillCircle(cx + 3, cy + 16, 2, C_LGREY);
}

}  // namespace Mind
