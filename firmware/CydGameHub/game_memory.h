// Memory - 6x4 cards, 12 pairs. Tap two cards to flip them; matching pairs stay up. Fewer moves = better.
#pragma once
#include "hub.h"

namespace Memory {

static const int COLS = 6, ROWS = 4, N = COLS * ROWS, PAIRS = N / 2;
static const int PX = 52, PY = 53, CW = 48, CH = 49;       // pitch and card size
static const int OX = 6, OY = PF_Y + 3;                    // grid spans x 6..313, y 25..233

static const uint16_t BACK_FILL = RGB565(36, 56, 140);
static const uint16_t BACK_RIM  = RGB565(110, 140, 235);
static const uint16_t FACE_FILL = RGB565(20, 24, 40);
static const uint16_t FACE_RIM  = RGB565(200, 205, 225);
static const uint16_t DONE_FILL = RGB565(10, 46, 28);
static const uint16_t DONE_RIM  = RGB565(40, 190, 90);

// Pair p uses shape p % 6 with its own colour: all 12 shape+colour combos are distinct.
static const uint16_t PAL[PAIRS] = {
  C_RED, C_GREEN, C_CYAN, C_YELLOW, C_ORANGE, C_MAGENTA,
  C_WHITE, RGB565(170, 110, 255), RGB565(150, 255, 80), C_PINK, RGB565(80, 150, 255), RGB565(255, 130, 120)
};

static uint8_t deck[N];     // pair id of each card
static uint8_t st[N];       // 0 face down, 1 face up, 2 matched
static int first, second, moves, matched, phase;   // phase: 0 picking, 1 mismatch shown, 2 won (waiting tap), 3 won (overlay pending)
static uint32_t tFlip, tWin;
static int mismatchMs;      // how long a mismatched pair stays face up

static void drawSym(int cx, int cy, int shape, uint16_t col, uint16_t bg) {
  switch (shape) {
    case 0: tft.fillCircle(cx, cy, 13, col); break;
    case 1: tft.fillRect(cx - 11, cy - 11, 22, 22, col); break;
    case 2: tft.fillTriangle(cx, cy - 14, cx - 14, cy + 11, cx + 14, cy + 11, col); break;
    case 3:
      tft.fillTriangle(cx, cy - 15, cx - 12, cy, cx + 12, cy, col);
      tft.fillTriangle(cx, cy + 15, cx - 12, cy, cx + 12, cy, col);
      break;
    case 4:
      tft.fillCircle(cx, cy, 13, col);
      tft.fillCircle(cx, cy, 6, bg);
      break;
    default:
      tft.fillRect(cx - 4, cy - 13, 8, 26, col);
      tft.fillRect(cx - 13, cy - 4, 26, 8, col);
      break;
  }
}

static void drawCard(int i) {
  int x = OX + (i % COLS) * PX;
  int y = OY + (i / COLS) * PY;
  if (st[i] == 0) {
    tft.fillRoundRect(x, y, CW, CH, 6, BACK_FILL);
    tft.drawRoundRect(x, y, CW, CH, 6, BACK_RIM);
    tft.drawRoundRect(x + 4, y + 4, CW - 8, CH - 8, 4, RGB565(70, 95, 190));
    text("?", x + CW / 2, y + CH / 2, 4, RGB565(150, 175, 255), MC_DATUM);
  } else {
    uint16_t bg = (st[i] == 2) ? DONE_FILL : FACE_FILL;
    uint16_t rim = (st[i] == 2) ? DONE_RIM : FACE_RIM;
    tft.fillRoundRect(x, y, CW, CH, 6, bg);
    tft.drawRoundRect(x, y, CW, CH, 6, rim);
    drawSym(x + CW / 2, y + CH / 2, deck[i] % 6, PAL[deck[i]], bg);
  }
}

static void reset() {
  mismatchMs = byDiff(1100, 700, 450);
  hudBegin("Memory");
  pfClear(C_BLACK);
  for (int i = 0; i < N; i++) {
    deck[i] = (uint8_t)(i / 2);
    st[i] = 0;
  }
  for (int i = N - 1; i > 0; i--) {          // Fisher-Yates
    int j = rnd(i + 1);
    uint8_t t = deck[i];
    deck[i] = deck[j];
    deck[j] = t;
  }
  first = second = -1;
  moves = 0;
  matched = 0;
  phase = 0;
  for (int i = 0; i < N; i++) drawCard(i);
  hudScore(moves, "Moves");
}

static void hideMismatch() {
  if (first >= 0) { st[first] = 0; drawCard(first); }
  if (second >= 0) { st[second] = 0; drawCard(second); }
  first = second = -1;
  phase = 0;
}

static void init() { reset(); }

static void tick() {
  if (phase == 3) {                                  // let the last pair be seen first
    if (millis() - tWin >= 700) {
      overlayGameOver(moves, " moves");
      phase = 2;
      tWin = millis();
    }
    return;
  }
  if (phase == 2) {
    if (touch.pressed && touch.y >= PF_Y && millis() - tWin > 500) reset();
    return;
  }
  if (phase == 1) {
    if (millis() - tFlip >= (uint32_t)mismatchMs) hideMismatch();
    else if (touch.tap) hideMismatch();              // impatient tap: hide now, then handle the tap below
  }
  if (!touch.tap) return;
  if (!inRect(touch.x, touch.y, OX, OY, COLS * PX, ROWS * PY)) return;

  int c = clampi((touch.x - OX) / PX, 0, COLS - 1);  // the gaps belong to the card on their left/top
  int r = clampi((touch.y - OY) / PY, 0, ROWS - 1);
  int i = r * COLS + c;
  if (st[i] != 0) return;

  st[i] = 1;
  drawCard(i);
  beep(900, 25);
  if (first < 0) {
    first = i;
    return;
  }
  second = i;
  moves++;
  hudScore(moves, "Moves");
  if (deck[first] == deck[second]) {
    st[first] = st[second] = 2;
    drawCard(first);
    drawCard(second);
    first = second = -1;
    matched++;
    if (matched == PAIRS) {
      phase = 3;
      tWin = millis();
      beep(1250, 250);
    } else {
      beep(1300, 90);
    }
  } else {
    phase = 1;
    tFlip = millis();
    beep(220, 110);
  }
}

static void icon(int cx, int cy) {
  for (int k = 0; k < 4; k++) {
    int x = cx - 17 + (k % 2) * 18;
    int y = cy - 17 + (k / 2) * 18;
    bool up = (k == 0 || k == 3);
    tft.fillRoundRect(x, y, 16, 16, 3, up ? FACE_FILL : BACK_FILL);
    tft.drawRoundRect(x, y, 16, 16, 3, up ? FACE_RIM : BACK_RIM);
    if (k == 0) tft.fillCircle(x + 8, y + 8, 4, C_RED);
    else if (k == 3) tft.fillTriangle(x + 8, y + 3, x + 3, y + 12, x + 13, y + 12, C_YELLOW);
    else text("?", x + 8, y + 8, 1, RGB565(150, 175, 255), MC_DATUM);
  }
}

}  // namespace Memory
