// Runner - endless runner. Tap anywhere to jump (hold a little longer = a little higher).
// Jump over cacti and, later, low flying birds. Score = distance / 10.
#pragma once
#include "hub.h"

namespace Runner {

static const int GROUND = 200;                 // y of the ground line (2 px thick)
static const int RX = 36;                      // runner left edge
static const int RW = 22, RH = 24;             // runner sprite box
static const int MAXOBS = 4, NSPECK = 14, NCLOUD = 2;
static const int JV = 216;                     // jump velocity, 1/16 px per frame
static const int G_HOLD = 24, G_CUT = 40;      // gravity (1/16 px per frame^2) held / released while rising
static const uint16_t C_RUN   = RGB565(120, 225, 90);
static const uint16_t C_CAC   = RGB565(50, 180, 80);
static const uint16_t C_BIRD  = RGB565(255, 150, 60);
static const uint16_t C_WING  = RGB565(255, 205, 100);
static const uint16_t C_CLOUD = RGB565(44, 54, 76);

struct Obs {
  int16_t x, y;
  uint8_t w, h, kind, n;   // kind: 0 cactus (n stems), 1 bird
  bool on;
};
static Obs obs[MAXOBS];

static int16_t spX[NSPECK];
static uint8_t spY[NSPECK], spW[NSPECK];
static int16_t clX[NCLOUD], clY[NCLOUD];

static int hgt, vv, jumpBuf, state, score, lastScore, scr16, nextGap, sinceLast, drawnTop;
static bool onGround;
// per-run difficulty knobs (set in reset(); normal = the original numbers)
static int speedBase16, speedRampDiv, speedRampCap, speedMinPx, gapExtraBase, gapExtraRand, firstGap, birdPercent;
static long distPx;
static uint32_t frame, deadAt;   // state: 0 ready, 1 running, 2 dead

// Clipped fillRect (obstacles scroll in from / out to the screen edges).
static void fr(int x, int y, int w, int h, uint16_t c) {
  int x2 = x + w, y2 = y + h;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x2 > SCR_W) x2 = SCR_W;
  if (y2 > SCR_H) y2 = SCR_H;
  if (x2 <= x || y2 <= y) return;
  tft.fillRect(x, y, x2 - x, y2 - y, c);
}

// leg: 0 = left leg down, 1 = right leg down, 2 = both down (in the air)
static void drawRunner(int x, int y0, int leg, bool dead) {
  fr(x + 10, y0, 12, 9, C_RUN);                     // head
  fr(x + 16, y0 + 7, 6, 1, C_BLACK);                // mouth
  fr(x + 16, y0 + 2, 3, 3, dead ? C_RED : C_BLACK); // eye
  fr(x + 4, y0 + 8, 14, 10, C_RUN);                 // body
  fr(x, y0 + 9, 4, 4, C_RUN);                       // tail
  fr(x + 16, y0 + 12, 5, 2, C_RUN);                 // arm
  int lenL = (leg == 1) ? 3 : 6;
  int lenR = (leg == 0) ? 3 : 6;
  fr(x + 6, y0 + 18, 4, lenL, C_RUN);
  if (lenL > 3) fr(x + 6, y0 + 22, 6, 2, C_RUN);
  fr(x + 13, y0 + 18, 4, lenR, C_RUN);
  if (lenR > 3) fr(x + 13, y0 + 22, 6, 2, C_RUN);
}

static void drawObs(const Obs& o) {
  if (o.kind == 0) {
    for (int i = 0; i < o.n; i++) {
      int xo = o.x + i * 14;
      int hh = o.h - ((i & 1) ? 6 : 0);
      int ty = GROUND - hh;
      fr(xo + 4, ty, 6, 1, C_CAC);                 // rounded top
      fr(xo + 3, ty + 1, 8, hh - 1, C_CAC);        // trunk
      int ay = ty + hh / 3;
      if (hh >= 22) { fr(xo, ay, 4, 3, C_CAC); fr(xo, ay - 6, 3, 7, C_CAC); }
      if (hh >= 26) { fr(xo + 10, ay + 4, 4, 3, C_CAC); fr(xo + 11, ay - 2, 3, 7, C_CAC); }
    }
  } else {
    int x = o.x, y = o.y;
    fr(x + 5, y + 5, 13, 6, C_BIRD);               // body
    fr(x + 1, y + 4, 6, 6, C_BIRD);                // head
    fr(x, y + 7, 2, 2, C_YELLOW);                  // beak
    fr(x + 3, y + 5, 2, 2, C_BLACK);               // eye
    fr(x + 18, y + 6, 4, 3, C_BIRD);               // tail
    if (((frame >> 2) & 1) == 0) {                 // wing up
      fr(x + 9, y + 2, 8, 3, C_WING);
      fr(x + 11, y, 5, 2, C_WING);
    } else {                                       // wing down
      fr(x + 9, y + 11, 8, 3, C_WING);
      fr(x + 11, y + 14, 5, 2, C_WING);
    }
  }
}

static void eraseObs(const Obs& o) { fr(o.x, o.y, o.w, o.h, C_BLACK); }

static void drawCloud(int i) {
  fr(clX[i] + 6, clY[i], 12, 3, C_CLOUD);
  fr(clX[i] + 2, clY[i] + 3, 26, 3, C_CLOUD);
  fr(clX[i], clY[i] + 6, 30, 3, C_CLOUD);
}
static void eraseCloud(int i) { fr(clX[i], clY[i], 30, 9, C_BLACK); }

static void newSpeck(int i, int x) {
  spX[i] = (int16_t)x;
  spY[i] = (uint8_t)rndRange(GROUND + 5, GROUND + 30);
  spW[i] = (uint8_t)rndRange(1, 4);
}

static int speed16() { return speedBase16 + imin(score / speedRampDiv, speedRampCap); }

static void reset() {
  speedBase16  = byDiff(64, 80, 96);          // start speed, 1/16 px per frame
  speedRampDiv = byDiff(10, 8, 6);            // score points per +1/16 px of speed
  speedRampCap = byDiff(50, 70, 90);          // max speed gain
  speedMinPx   = byDiff(4, 5, 5);
  gapExtraBase = byDiff(70, 40, 25);          // extra gap beyond the jump distance
  gapExtraRand = byDiff(130, 110, 90);
  firstGap     = byDiff(170, 130, 110);
  birdPercent  = byDiff(15, 28, 38);
  hudBegin("Runner");
  hgt = 0;
  vv = 0;
  onGround = true;
  jumpBuf = 0;
  state = 0;
  distPx = 0;
  score = 0;
  lastScore = 0;
  scr16 = 0;
  sinceLast = 0;
  nextGap = firstGap;
  frame = 0;
  for (int i = 0; i < MAXOBS; i++) obs[i].on = false;
  fr(0, GROUND, SCR_W, 2, C_LGREY);
  for (int i = 0; i < NSPECK; i++) {
    newSpeck(i, rnd(SCR_W));
    fr(spX[i], spY[i], spW[i], 1, C_GREY);
  }
  for (int i = 0; i < NCLOUD; i++) {
    clX[i] = (int16_t)(i * 160 + rnd(120));
    clY[i] = (int16_t)rndRange(34, 76);
    drawCloud(i);
  }
  drawnTop = GROUND - RH;
  drawRunner(RX, drawnTop, 0, false);
  hudScore(0);
  text("Tap to run", SCR_W / 2, 122, 4, C_WHITE, MC_DATUM);
  text("hold = jump higher", SCR_W / 2, 148, 2, C_GREY, MC_DATUM);
}

static void spawn() {
  int slot = -1;
  for (int i = 0; i < MAXOBS; i++) {
    if (!obs[i].on) { slot = i; break; }
  }
  if (slot < 0) return;   // retry next tick
  Obs& o = obs[slot];
  o.on = true;
  o.x = SCR_W;
  if (score >= 100 && rnd(100) < birdPercent) {
    o.kind = 1;
    o.n = 1;
    o.w = 22;
    o.h = 16;
    int lift = rnd(2) ? 11 : 3;
    o.y = (int16_t)(GROUND - lift - 16);
  } else {
    int r = rnd(10);
    int n = 1;
    if (score >= 60 && r >= 5) n = 2;
    if (score >= 250 && r >= 8) n = 3;
    o.kind = 0;
    o.n = (uint8_t)n;
    o.w = (uint8_t)(n * 14);
    o.h = (uint8_t)rndRange(20, 34);
    o.y = (int16_t)(GROUND - o.h);
  }
  int spdPx = imax(speedMinPx, speed16() >> 4);
  nextGap = o.w + spdPx * 22 + gapExtraBase + rnd(gapExtraRand);
  sinceLast = 0;
}

static bool hit(int top) {
  int rx0 = RX + 4, rx1 = RX + RW - 4;
  int ry0 = top + 2, ry1 = top + RH - 2;
  for (int i = 0; i < MAXOBS; i++) {
    const Obs& o = obs[i];
    if (!o.on) continue;
    int ox0 = o.x + 2, ox1 = o.x + o.w - 2;
    int oy0 = (o.kind == 0) ? o.y + 2 : o.y + 3;
    int oy1 = (o.kind == 0) ? o.y + o.h : o.y + 13;
    if (rx0 < ox1 && ox0 < rx1 && ry0 < oy1 && oy0 < ry1) return true;
  }
  return false;
}

static void die(int top) {
  state = 2;
  deadAt = millis();
  drawRunner(RX, top, 2, true);
  beep(150, 300);
  overlayGameOver(score);
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && touch.y >= PF_Y && millis() - deadAt >= 700) reset();
    return;
  }
  if (state == 0) {
    if (!(touch.pressed && touch.y >= PF_Y)) return;
    state = 1;
    fr(50, 106, 220, 56, C_BLACK);   // wipe the "Tap to run" texts
  }
  frame++;

  // ---- input and jump physics
  if (touch.pressed && touch.y >= PF_Y) jumpBuf = 6;
  if (jumpBuf > 0) {
    if (onGround) {
      vv = JV;
      onGround = false;
      jumpBuf = 0;
      beep(660, 35);
    } else {
      jumpBuf--;
    }
  }
  if (!onGround) {
    hgt += vv;
    vv -= (vv > 0 && !touch.down) ? G_CUT : G_HOLD;
    if (hgt <= 0) {
      hgt = 0;
      vv = 0;
      onGround = true;
    }
  }

  // ---- speed / distance
  int spd16 = speed16();   // 5.0 .. 9.4 px per frame
  scr16 += spd16;
  int dx = scr16 >> 4;
  scr16 &= 15;
  distPx += dx;
  score = (int)(distPx / 10);

  // ---- erase moving things
  fr(RX, drawnTop, RW, RH, C_BLACK);
  for (int i = 0; i < MAXOBS; i++) {
    if (obs[i].on) eraseObs(obs[i]);
  }
  for (int i = 0; i < NSPECK; i++) fr(spX[i], spY[i], spW[i], 1, C_BLACK);

  // ---- move
  for (int i = 0; i < MAXOBS; i++) {
    if (!obs[i].on) continue;
    obs[i].x = (int16_t)(obs[i].x - dx);
    if (obs[i].x + obs[i].w <= 0) obs[i].on = false;
  }
  sinceLast += dx;
  if (sinceLast >= nextGap) spawn();
  for (int i = 0; i < NSPECK; i++) {
    spX[i] = (int16_t)(spX[i] - dx);
    if (spX[i] + spW[i] <= 0) newSpeck(i, SCR_W + rnd(40));
  }
  if ((frame & 1) == 0) {
    for (int i = 0; i < NCLOUD; i++) {
      eraseCloud(i);
      clX[i]--;
      if (clX[i] < -30) {
        clX[i] = (int16_t)(SCR_W + rnd(60));
        clY[i] = (int16_t)rndRange(34, 76);
      }
      drawCloud(i);
    }
  }

  // ---- draw
  for (int i = 0; i < NSPECK; i++) fr(spX[i], spY[i], spW[i], 1, C_GREY);
  for (int i = 0; i < MAXOBS; i++) {
    if (obs[i].on) drawObs(obs[i]);
  }
  int top = GROUND - RH - (hgt >> 4);
  drawnTop = top;
  drawRunner(RX, top, onGround ? (int)((frame >> 2) & 1) : 2, false);

  if (hit(top)) {
    die(top);
    return;
  }
  if (score != lastScore) {
    lastScore = score;
    hudScore(score);
  }
}

static void icon(int cx, int cy) {
  drawRunner(cx - 17, cy - 14, 0, false);
  fr(cx - 18, cy + 10, 36, 2, C_LGREY);
  fr(cx + 9, cy - 8, 6, 18, C_CAC);
  fr(cx + 6, cy, 3, 3, C_CAC);
  fr(cx + 6, cy - 5, 2, 6, C_CAC);
  fr(cx + 14, cy + 3, 3, 3, C_CAC);
  fr(cx + 15, cy - 1, 2, 5, C_CAC);
}

}  // namespace Runner
