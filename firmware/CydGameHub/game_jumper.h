// Jumper - bounce up from platform to platform. Hold a finger anywhere: the jumper steers towards it. Don't fall!
// Blue platforms slide sideways, brown ones crumble when you land (a normal platform is always nearby). Higher = better.
#pragma once
#include "hub.h"

namespace Jumper {

static const int JS = 18, PLH = 7, MAXP = 14, UNUSED = 255;
static const int SCROLL_Y = PF_Y + 90;                         // the jumper never climbs above this line: the world scrolls instead
static const float GRAV = 0.5f, JUMP_V = 10.5f;                // jump height ~110 px, platform gaps stay below ~92
static const uint16_t BGC = RGB565(14, 18, 44);

struct Plat { int16_t x, y; int8_t dir; uint8_t kind; };       // kind: 0 normal, 1 sliding, 2 crumbling, UNUSED = free slot
static Plat pl[MAXP];
static int pw, gapMax, movePct, brkPct, climb, score, state;   // state: 0 ready, 1 playing, 2 dead
static float px, py, vy;
static int oldPx, oldPy;
static uint32_t deadAt;

// Every draw is clipped to the playfield, so platforms scrolling under the HUD or off the bottom never touch it.
static void fillR(int x, int y, int w, int h, uint16_t c) {
  int x0 = imax(x, 0), y0 = imax(y, PF_Y), x1 = imin(x + w, SCR_W), y1 = imin(y + h, SCR_H);
  if (x1 > x0 && y1 > y0) tft.fillRect(x0, y0, x1 - x0, y1 - y0, c);
}
static uint16_t platCol(int kind) {
  return kind == 1 ? RGB565(70, 150, 255) : (kind == 2 ? RGB565(176, 112, 60) : RGB565(80, 205, 80));
}
static void drawPlat(const Plat& p) {
  uint16_t c = platCol(p.kind);
  fillR(p.x, p.y, pw, PLH, c);
  fillR(p.x, p.y, pw, 2, RGB565(220, 245, 220));                         // light top edge
  if (p.kind == 2) fillR(p.x + pw / 2 - 1, p.y, 2, PLH, BGC);            // crack
}
static void drawPlayer() {
  int x = (int)px, y = (int)py;
  uint16_t body = RGB565(120, 230, 90);
  fillR(x, y + 2, JS, JS - 6, body);
  fillR(x + 2, y, JS - 4, JS - 4, body);
  fillR(x + 3, y + 4, 4, 5, C_WHITE);                            // eyes
  fillR(x + JS - 7, y + 4, 4, 5, C_WHITE);
  fillR(x + 5, y + 6, 2, 3, C_BLACK);
  fillR(x + JS - 6, y + 6, 2, 3, C_BLACK);
  fillR(x + 2, y + JS - 4, 5, 4, RGB565(70, 160, 50));           // feet
  fillR(x + JS - 7, y + JS - 4, 5, 4, RGB565(70, 160, 50));
}

static int activeCount() {
  int n = 0;
  for (int i = 0; i < MAXP; i++) n += (pl[i].kind != UNUSED);
  return n;
}
static int topY() {
  int t = SCR_H;
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind != UNUSED && pl[i].y < t) t = pl[i].y;
  return t;
}
static int freeSlot() {
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind == UNUSED) return i;
  return -1;
}
static void addPlat(int x, int y, int kind) {
  int s = freeSlot();
  if (s < 0) return;
  pl[s].x = (int16_t)clampi(x, 0, SCR_W - pw);
  pl[s].y = (int16_t)y;
  pl[s].kind = (uint8_t)kind;
  pl[s].dir = (rnd(2) == 0) ? 1 : -1;
}

// Add the next platform of the chain above the current top one (+ sometimes a crumbling decoy beside it).
static void spawnOne() {
  int gmax = imin(gapMax + score / 40, 92);
  int y = topY() - rndRange(28, gmax);
  int x = rndRange(0, SCR_W - pw);
  int kind = (rnd(100) < movePct) ? 1 : 0;
  addPlat(x, y, kind);
  if (rnd(100) < brkPct) {
    int dx = (x > SCR_W / 2) ? rndRange(0, imax(0, x - pw - 8)) : rndRange(imin(x + pw + 8, SCR_W - pw), SCR_W - pw);
    addPlat(dx, y + rndRange(-14, 14), 2);
  }
}
static void refill() {
  int guard = 0;
  while (activeCount() < MAXP - 1 && topY() > PF_Y - 30 && guard++ < 8) spawnOne();
}

static void reset() {
  pw = byDiff(56, 46, 38);
  gapMax = byDiff(58, 68, 80);
  movePct = byDiff(0, 14, 26);
  brkPct = byDiff(0, 12, 22);
  hudBegin("Jumper", BGC);
  for (int i = 0; i < MAXP; i++) pl[i].kind = UNUSED;
  climb = score = 0;
  state = 0;
  addPlat(SCR_W / 2 - pw / 2, SCR_H - 24, 0);
  addPlat(SCR_W / 2 - pw / 2 + rndRange(-40, 40), SCR_H - 24 - 46, 0);
  refill();
  px = (float)(SCR_W / 2 - JS / 2);
  py = (float)(SCR_H - 24 - JS);
  vy = -JUMP_V;
  oldPx = (int)px;
  oldPy = (int)py;
  hudScore(0, "Height");
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind != UNUSED) drawPlat(pl[i]);
  drawPlayer();
  text("Hold a finger to steer", SCR_W / 2, PF_Y + 60, 2, C_WHITE, MC_DATUM);
}

static void die() {
  state = 2;
  deadAt = millis();
  beep(160, 300);
  overlayGameOver(score, " m");
}

static void init() { reset(); }

static void tick() {
  if (state == 2) {
    if (touch.pressed && millis() - deadAt >= 500) reset();
    return;
  }
  if (state == 0) {
    if (!touch.pressed) return;
    state = 1;
    fillR(40, PF_Y + 48, 240, 24, BGC);
  }
  // 1. erase everything at its old position
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind != UNUSED) fillR(pl[i].x, pl[i].y, pw, PLH, BGC);
  fillR(oldPx, oldPy, JS, JS, BGC);

  // 2. steer, slide, fall
  if (touch.down) {
    float d = (float)touch.x - (px + (float)JS / 2.0f);
    float s = d * 0.35f;
    if (s > 7.0f) s = 7.0f;
    if (s < -7.0f) s = -7.0f;
    px += s;
  }
  if (px < 0.0f) px = 0.0f;
  if (px > (float)(SCR_W - JS)) px = (float)(SCR_W - JS);
  for (int i = 0; i < MAXP; i++) {
    Plat& p = pl[i];
    if (p.kind != 1) continue;
    p.x = (int16_t)(p.x + p.dir * 2);
    if (p.x <= 0) { p.x = 0; p.dir = 1; }
    if (p.x >= SCR_W - pw) { p.x = (int16_t)(SCR_W - pw); p.dir = -1; }
  }
  float prevFeet = py + (float)JS;
  vy += GRAV;
  py += vy;
  float feet = py + (float)JS;
  if (vy > 0.0f) {
    for (int i = 0; i < MAXP; i++) {
      Plat& p = pl[i];
      if (p.kind == UNUSED) continue;
      if (prevFeet <= (float)(p.y + 2) && feet >= (float)p.y && px + (float)JS > (float)(p.x + 3) && px < (float)(p.x + pw - 3)) {
        if (p.kind == 2) {                                     // crumbles: no bounce
          p.kind = UNUSED;
          beep(220, 40);
        } else {
          py = (float)(p.y - JS);
          vy = -JUMP_V;
          beep(p.kind == 1 ? 900 : 700, 15);
        }
        break;
      }
    }
  }

  // 3. scroll the world down when the jumper climbs past the line
  if (py < (float)SCROLL_Y) {
    int d = (int)((float)SCROLL_Y - py + 0.5f);
    py = (float)SCROLL_Y;
    for (int i = 0; i < MAXP; i++)
      if (pl[i].kind != UNUSED) pl[i].y = (int16_t)(pl[i].y + d);
    climb += d;
    int ns = climb / 6;
    if (ns != score) { score = ns; hudScore(score, "Height"); }
  }
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind != UNUSED && pl[i].y > SCR_H) pl[i].kind = UNUSED;
  refill();

  // 4. draw
  for (int i = 0; i < MAXP; i++)
    if (pl[i].kind != UNUSED) drawPlat(pl[i]);
  drawPlayer();
  oldPx = (int)px;
  oldPy = (int)py;
  if (py > (float)SCR_H) die();
}

static void icon(int cx, int cy) {
  tft.fillRect(cx - 18, cy + 12, 22, 5, RGB565(80, 205, 80));
  tft.fillRect(cx + 4, cy - 4, 18, 5, RGB565(70, 150, 255));
  tft.fillRect(cx - 14, cy - 18, 20, 5, RGB565(176, 112, 60));
  tft.fillRect(cx - 4, cy - 2, 14, 12, RGB565(120, 230, 90));
  tft.fillRect(cx - 1, cy + 1, 3, 4, C_WHITE);
  tft.fillRect(cx + 5, cy + 1, 3, 4, C_WHITE);
}

}  // namespace Jumper
