// 2048 - swipe to slide the tiles; equal tiles merge. Reach 2048, then keep going.
#pragma once
#include "hub.h"

namespace G2048 {

static const int N = 4, CELL = 48, GAP = 4;
static const int BW = N * CELL + (N + 1) * GAP;   // 212
static const int OX = (SCR_W - BW) / 2;           // 54
static const int OY = PF_Y + 3;                   // 25 -> board ends at y = 236

static const uint16_t BOARD_COL = RGB565(187, 173, 160);
static const uint16_t DARK_TXT  = RGB565(119, 110, 101);
static const uint16_t TILE_COL[13] = {
  RGB565(205, 193, 180),   // 0: empty
  RGB565(238, 228, 218),   // 2
  RGB565(237, 224, 200),   // 4
  RGB565(242, 177, 121),   // 8
  RGB565(245, 149, 99),    // 16
  RGB565(246, 124, 95),    // 32
  RGB565(246, 94, 59),     // 64
  RGB565(237, 207, 114),   // 128
  RGB565(237, 204, 97),    // 256
  RGB565(237, 200, 80),    // 512
  RGB565(237, 197, 63),    // 1024
  RGB565(237, 194, 46),    // 2048
  RGB565(60, 58, 50)       // 4096+
};

static uint8_t bd[N][N];      // exponent: 0 = empty, k = tile 2^k
static uint8_t shown[N][N];   // what is currently drawn (255 = unknown)
static int score, state;      // state: 0 playing, 1 "2048!" message, 2 game-over pending, 3 over
static int merged;            // merges in the last slide
static bool won, hit2048;
static int fourPercent;       // chance (percent) that a spawned tile is a 4 instead of a 2
static uint32_t tState;

static void drawCell(int r, int c) {
  int v = bd[r][c];
  int x = OX + GAP + c * (CELL + GAP);
  int y = OY + GAP + r * (CELL + GAP);
  tft.fillRoundRect(x, y, CELL, CELL, 5, TILE_COL[imin(v, 12)]);
  if (v > 0) {
    char s[12];
    snprintf(s, sizeof s, "%ld", 1L << v);
    uint8_t font = 4;
    if (tft.textWidth(s, 4) > CELL - 6) font = 2;
    if (font == 2 && tft.textWidth(s, 2) > CELL - 6) font = 1;
    text(s, x + CELL / 2, y + CELL / 2 + (font == 4 ? 1 : 0), font,
         v <= 2 ? DARK_TXT : C_WHITE, MC_DATUM);
  }
}

static void refresh() {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (shown[r][c] != bd[r][c]) {
        shown[r][c] = bd[r][c];
        drawCell(r, c);
      }
}

static void drawBoard() {
  tft.fillRoundRect(OX, OY, BW, BW, 8, BOARD_COL);
  memset(shown, 255, sizeof shown);
  refresh();
}

static void drawSide() {
  int rx = (OX + BW + SCR_W) / 2;
  text("SWIPE", OX / 2, OY + 8, 1, C_GREY, MC_DATUM);
  text("BEST", rx, OY + 8, 1, C_GREY, MC_DATUM);
  if (bestScore() > 0) numBox(bestScore(), rx, OY + 24, 46, 14, 1, C_GOLD, C_BLACK);
  else textBox("-", rx, OY + 24, 46, 14, 1, C_GOLD, C_BLACK);
}

// Cell of line i at distance k from the edge the tiles slide towards.
// d: 0 = up, 1 = right, 2 = down, 3 = left (same numbering as touch.swipe).
static void lineCell(int d, int i, int k, int& r, int& c) {
  switch (d) {
    case 0:  r = k;         c = i;         break;
    case 1:  r = i;         c = N - 1 - k; break;
    case 2:  r = N - 1 - k; c = i;         break;
    default: r = i;         c = k;         break;
  }
}

// Slide and merge the whole board. Returns true if anything moved.
static bool slide(int d) {
  bool changed = false;
  merged = 0;
  hit2048 = false;
  for (int i = 0; i < N; i++) {
    uint8_t line[N] = {0, 0, 0, 0};
    uint8_t out[N]  = {0, 0, 0, 0};
    int n = 0, m = 0;
    for (int k = 0; k < N; k++) {
      int r, c;
      lineCell(d, i, k, r, c);
      if (bd[r][c]) line[n++] = bd[r][c];
    }
    for (int k = 0; k < n; k++) {
      if (k + 1 < n && line[k] == line[k + 1]) {   // each tile merges at most once
        int nv = line[k] + 1;
        out[m++] = (uint8_t)nv;
        score += 1 << nv;
        merged++;
        if (nv == 11) hit2048 = true;
        k++;
      } else {
        out[m++] = line[k];
      }
    }
    for (int k = 0; k < N; k++) {
      int r, c;
      lineCell(d, i, k, r, c);
      if (bd[r][c] != out[k]) {
        bd[r][c] = out[k];
        changed = true;
      }
    }
  }
  return changed;
}

static bool canMove() {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (!bd[r][c]) return true;
      if (c + 1 < N && bd[r][c] == bd[r][c + 1]) return true;
      if (r + 1 < N && bd[r][c] == bd[r + 1][c]) return true;
    }
  return false;
}

static void spawn() {
  int empty = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (!bd[r][c]) empty++;
  if (empty == 0) return;
  int pick = rnd(empty);
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (!bd[r][c]) {
        if (pick == 0) {
          bd[r][c] = (uint8_t)(rnd(100) < fourPercent ? 2 : 1);   // normal: 90% "2", 10% "4"
          return;
        }
        pick--;
      }
}

static void reset() {
  fourPercent = byDiff(3, 10, 25);
  hudBegin("2048");
  memset(bd, 0, sizeof bd);
  score = 0;
  merged = 0;
  won = false;
  hit2048 = false;
  state = 0;
  spawn();
  spawn();
  hudScore(score);
  drawSide();
  drawBoard();
}

static void init() { reset(); }

static void tick() {
  uint32_t now = millis();
  switch (state) {
    case 1:   // "2048!" message: tap to keep playing
      if (touch.pressed && now - tState > 400) {
        drawBoard();
        if (canMove()) {
          state = 0;
        } else {
          state = 2;
          tState = now;
        }
      }
      return;
    case 2:   // let the player look at the final board first
      if (now - tState >= 800) {
        overlayGameOver(score);
        drawSide();
        state = 3;
        tState = now;
      }
      return;
    case 3:
      if (touch.pressed && now - tState > 500) reset();
      return;
    default:
      break;
  }

  if (touch.swipe < 0) return;
  if (!slide(touch.swipe)) return;   // nothing moved: no spawn, no sound
  spawn();
  refresh();
  hudScore(score);
  beep(merged > 0 ? 760 : 520, 25);
  if (hit2048 && !won) {
    won = true;
    state = 1;
    tState = now;
    beep(1320, 220);
    overlayMsg("2048!", C_GOLD, "You made it!", "Keep going for more", "Tap to continue");
    return;
  }
  if (!canMove()) {
    state = 2;
    tState = now;
    beep(180, 300);
  }
}

static void icon(int cx, int cy) {
  static const uint16_t col[4] = {RGB565(238, 228, 218), RGB565(242, 177, 121),
                                  RGB565(246, 94, 59), RGB565(237, 194, 46)};
  static const char* lab[4] = {"2", "4", "8", "16"};
  for (int i = 0; i < 4; i++) {
    int x = cx - 17 + (i & 1) * 18;
    int y = cy - 17 + (i >> 1) * 18;
    tft.fillRoundRect(x, y, 16, 16, 3, col[i]);
    text(lab[i], x + 8, y + 8, 1, i == 0 ? DARK_TXT : C_WHITE, MC_DATUM);
  }
}

}  // namespace G2048
