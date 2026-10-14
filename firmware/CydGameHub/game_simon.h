// Simon - watch the growing pad sequence, then repeat it by tapping the pads.
#pragma once
#include "hub.h"

namespace Simon {

static const int AX = 4, AY = PF_Y + 4, GAPP = 6;
static const int AW = SCR_W - 2 * AX;            // 312
static const int AH = SCR_H - AY - 4;            // 210
static const int PW = (AW - GAPP) / 2;           // 153
static const int PH = (AH - GAPP) / 2;           // 102
static const int CX = SCR_W / 2;                 // 160
static const int CY = AY + AH / 2;               // 131
static const int MAXSEQ = 40;

// pad index: 0 = top-left green, 1 = top-right red, 2 = bottom-left yellow, 3 = bottom-right blue
static const uint16_t DIM[4]    = {RGB565(0, 95, 25),   RGB565(110, 8, 8),   RGB565(115, 95, 0),  RGB565(10, 30, 120)};
static const uint16_t BRIGHT[4] = {RGB565(40, 255, 90), RGB565(255, 60, 60), RGB565(255, 235, 30), RGB565(70, 130, 255)};
static const int TONE[4]        = {523, 659, 784, 1047};

static const int ST_IDLE = 0, ST_SHOW = 1, ST_WAIT = 2, ST_FB = 3, ST_OVER = 4;

static uint8_t seq[MAXSEQ];
static int len, showIdx, inIdx, score, state, litPad;
static bool fbFail;
static uint32_t litOff, nextT, tOver;

static int onBaseMs, onStepMs, onMinMs, offBaseMs, offStepMs, offMinMs;   // difficulty: pad lit / gap durations and how fast they shrink

static int onMs()  { return imax(onMinMs, onBaseMs - (len - 1) * onStepMs); }
static int offMs() { return imax(offMinMs, offBaseMs - (len - 1) * offStepMs); }

static void drawCentre() {
  tft.fillCircle(CX, CY, 36, C_BLACK);
  tft.fillCircle(CX, CY, 30, RGB565(40, 44, 58));
  tft.drawCircle(CX, CY, 30, RGB565(110, 118, 140));
  if (state == ST_IDLE) {
    text("GO", CX, CY, 4, C_WHITE, MC_DATUM);
  } else {
    char s[8];
    snprintf(s, sizeof s, "%d", len);
    text(s, CX, CY, 4, C_WHITE, MC_DATUM);
  }
}

static void drawPad(int p, bool lit) {
  int x = AX + (p & 1) * (PW + GAPP);
  int y = AY + (p >> 1) * (PH + GAPP);
  tft.fillRoundRect(x, y, PW, PH, 16, lit ? BRIGHT[p] : DIM[p]);
  if (lit) tft.drawRoundRect(x, y, PW, PH, 16, C_WHITE);
  drawCentre();   // the centre disc overlaps all four pads
}

static void lightPad(int p, int ms, bool withBeep) {
  if (litPad >= 0 && litPad != p) drawPad(litPad, false);
  litPad = p;
  litOff = millis() + (uint32_t)ms;
  drawPad(p, true);
  if (withBeep) beep(TONE[p], ms);
}

static int padAt(int x, int y) {
  return (y >= CY ? 2 : 0) + (x >= CX ? 1 : 0);
}

static void startShow() {
  state = ST_SHOW;
  showIdx = 0;
  inIdx = 0;
  nextT = millis() + 700;
  hudText("Watch...", C_CYAN);
  drawCentre();
}

static void reset() {
  onBaseMs = byDiff(560, 450, 360);
  onStepMs = byDiff(8, 10, 12);
  onMinMs = byDiff(260, 200, 150);
  offBaseMs = byDiff(190, 150, 115);
  offStepMs = byDiff(3, 4, 5);
  offMinMs = byDiff(100, 70, 50);
  hudBegin("Simon");
  pfClear(C_BLACK);
  len = 0;
  score = 0;
  showIdx = 0;
  inIdx = 0;
  litPad = -1;
  fbFail = false;
  state = ST_IDLE;
  for (int p = 0; p < 4; p++) drawPad(p, false);
  hudText("Tap to start", C_WHITE);
}

static void init() { reset(); }

static void tick() {
  uint32_t now = millis();
  if (litPad >= 0 && (int32_t)(now - litOff) >= 0) {
    int p = litPad;
    litPad = -1;
    drawPad(p, false);
  }

  switch (state) {
    case ST_IDLE:
      if (touch.pressed && touch.y >= PF_Y) {
        len = 1;
        score = 0;
        seq[0] = (uint8_t)rnd(4);
        startShow();
      }
      break;

    case ST_SHOW:
      if (litPad < 0) {
        if (showIdx >= len) {
          state = ST_WAIT;
          inIdx = 0;
          hudText("Your turn", C_GREEN);
        } else if ((int32_t)(now - nextT) >= 0) {
          int on = onMs();
          lightPad(seq[showIdx], on, true);
          showIdx++;
          nextT = now + (uint32_t)(on + offMs());
        }
      }
      break;

    case ST_WAIT:
      if (touch.pressed && touch.y >= PF_Y) {
        int p = padAt(touch.x, touch.y);
        if (p == seq[inIdx]) {
          lightPad(p, 220, true);
          inIdx++;
          if (inIdx >= len) {              // round complete
            score = len;
            state = ST_FB;
            fbFail = false;
            nextT = now + 650;
            hudText("Nice!", C_GREEN);
          }
        } else {
          lightPad(p, 500, false);
          beep(140, 450);
          state = ST_FB;
          fbFail = true;
          nextT = now + 900;
          hudText("Wrong!", C_RED);
        }
      }
      break;

    case ST_FB:
      if ((int32_t)(now - nextT) >= 0) {
        if (fbFail || len >= MAXSEQ) {
          state = ST_OVER;
          tOver = now;
          overlayGameOver(score);
        } else {
          seq[len] = (uint8_t)rnd(4);
          len++;
          startShow();
        }
      }
      break;

    default:   // ST_OVER
      if (touch.pressed && now - tOver > 400) reset();
      break;
  }
}

static void icon(int cx, int cy) {
  tft.fillRoundRect(cx - 18, cy - 18, 17, 17, 4, RGB565(40, 255, 90));
  tft.fillRoundRect(cx + 1,  cy - 18, 17, 17, 4, RGB565(255, 60, 60));
  tft.fillRoundRect(cx - 18, cy + 1,  17, 17, 4, RGB565(255, 235, 30));
  tft.fillRoundRect(cx + 1,  cy + 1,  17, 17, 4, RGB565(70, 130, 255));
  tft.fillCircle(cx, cy, 7, C_DGREY);
  tft.fillCircle(cx, cy, 4, C_WHITE);
}

}  // namespace Simon
