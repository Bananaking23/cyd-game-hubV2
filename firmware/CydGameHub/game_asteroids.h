// Asteroids - vector space shooter. Touch and hold: the ship turns towards your finger and auto-fires;
// it thrusts only while the finger is far from the ship. Let go to drift. Shoot the rocks, survive the waves.
// Rocks: large -> 2 medium -> 2 small -> gone (20 / 50 / 100 points). Score = points.
#pragma once
#include "hub.h"
#include <math.h>

namespace Asteroids {

static const int   FIELD_W     = SCR_W;
static const int   FIELD_Y     = PF_Y;
static const int   FIELD_H     = SCR_H - PF_Y;
static const int   MAX_ROCKS   = 12;
static const int   MAX_BULLETS = 6;
static const int   MAX_DEBRIS  = 12;
static const int   MAX_SEGS    = 150;
static const int   MAX_VERTS   = 10;
static const float PI_F        = 3.14159265f;
static const float TAU_F       = 6.28318531f;
static const float SHIP_MAX_V  = 3.6f;
static const float BULLET_V    = 6.5f;
static const int   ROCK_R[3]   = {22, 12, 7};        // nominal radius per size
static const int   ROCK_PTS[3] = {20, 50, 100};
static const int   FINGER_NEAR = 50;                 // px: closer than this -> no thrust

static const uint16_t COL_SHIP   = C_GREEN;
static const uint16_t COL_FLAME  = C_ORANGE;
static const uint16_t COL_BULLET = C_YELLOW;

struct Rock {
  float x, y, vx, vy, ang, spin;
  int8_t ox[MAX_VERTS], oy[MAX_VERTS];   // per-rock shape: vertex offsets at angle 0
  uint8_t nv, size, alive;
};
struct Bullet { float x, y, vx, vy; int16_t life; };
struct Debris { float x, y, vx, vy; int16_t life; uint16_t col; };
struct Seg    { int16_t x0, y0, x1, y1; uint16_t col; };
struct SegOld { int16_t x0, y0, x1, y1; };

static Rock    rocks[MAX_ROCKS];
static Bullet  bullets[MAX_BULLETS];
static Debris  debris[MAX_DEBRIS];
static Seg     segs[MAX_SEGS];
static SegOld  prevSegs[MAX_SEGS];
static int     nSegs, nPrev, debrisNext;

static float shipX, shipY, shipVx, shipVy, shipA;
static bool  shipAlive, thrusting;
enum { MODE_PLAY = 0, MODE_RESPAWN = 1, MODE_DYING = 2, MODE_OVER = 3 };
static int   mode, lives, score, wave, nextExtra;
static bool  waveWait, bannerOn, hudDirty;
static uint32_t nextFireAt, invulnUntil, respawnAt, waveAt, bannerUntil, overAt;

// difficulty knobs (set in reset())
static int   startLives, rocksStart, rocksCap, fireMs, bulletLife, maxBullets, extraEvery;
static float rockBase, rockWaveCap, turnRate, thrustAcc, dragK;

// ------------------------------------------------------------------ small math helpers
static float fclampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int roundi(float v) { return (int)floorf(v + 0.5f); }
static float frand(int lo, int hi) { return (float)rndRange(lo, hi) / 100.0f; }   // lo/100 .. hi/100

static void wrapPos(float& x, float& y, float m) {
  float w = (float)FIELD_W + 2.0f * m, h = (float)FIELD_H + 2.0f * m;
  if (x < -m) x += w; else if (x >= (float)FIELD_W + m) x -= w;
  if (y < (float)FIELD_Y - m) y += h; else if (y >= (float)(FIELD_Y + FIELD_H) + m) y -= h;
}

// ------------------------------------------------------------------ line clipping to the playfield
static int outCode(int x, int y) {
  int c = 0;
  if (x < 0) c |= 1; else if (x > SCR_W - 1) c |= 2;
  if (y < FIELD_Y) c |= 4; else if (y > SCR_H - 1) c |= 8;
  return c;
}
static bool clipLine(int& x0, int& y0, int& x1, int& y1) {
  int c0 = outCode(x0, y0), c1 = outCode(x1, y1);
  for (int it = 0; it < 10; it++) {
    if ((c0 | c1) == 0) return true;
    if (c0 & c1) return false;
    int co = c0 ? c0 : c1;
    long x = 0, y = 0;
    if (co & 8)      { y = SCR_H - 1; x = x0 + (long)(x1 - x0) * (y - y0) / (y1 - y0); }
    else if (co & 4) { y = FIELD_Y;   x = x0 + (long)(x1 - x0) * (y - y0) / (y1 - y0); }
    else if (co & 2) { x = SCR_W - 1; y = y0 + (long)(y1 - y0) * (x - x0) / (x1 - x0); }
    else             { x = 0;         y = y0 + (long)(y1 - y0) * (x - x0) / (x1 - x0); }
    if (co == c0) { x0 = (int)x; y0 = (int)y; c0 = outCode(x0, y0); }
    else          { x1 = (int)x; y1 = (int)y; c1 = outCode(x1, y1); }
  }
  return false;
}
static void addSeg(float fx0, float fy0, float fx1, float fy1, uint16_t col) {
  if (nSegs >= MAX_SEGS) return;
  int x0 = roundi(fx0), y0 = roundi(fy0), x1 = roundi(fx1), y1 = roundi(fy1);
  if (iabs(x0) > 2000 || iabs(y0) > 2000 || iabs(x1) > 2000 || iabs(y1) > 2000) return;
  if (!clipLine(x0, y0, x1, y1)) return;
  Seg& s = segs[nSegs++];
  s.x0 = (int16_t)x0; s.y0 = (int16_t)y0; s.x1 = (int16_t)x1; s.y1 = (int16_t)y1; s.col = col;
}

// ------------------------------------------------------------------ HUD
static void refreshHud() {
  hudDirty = false;
  if (bannerOn) {
    char b[16];
    snprintf(b, sizeof b, "WAVE %d", wave);
    hudText(b, C_CYAN);
  } else {
    char b[32];
    snprintf(b, sizeof b, "%d  Lives %d", score, lives);
    hudText(b);
  }
}
static void addScore(int pts) {
  score += pts;
  while (score >= nextExtra) {
    nextExtra += extraEvery;
    if (lives < 9) { lives++; beep(1500, 120); }
  }
  hudDirty = true;
}

// ------------------------------------------------------------------ rocks
static int spawnRock(int size, float x, float y, float vx, float vy) {
  for (int i = 0; i < MAX_ROCKS; i++) {
    if (rocks[i].alive) continue;
    Rock& r = rocks[i];
    r.alive = 1;
    r.size = (uint8_t)size;
    r.x = x; r.y = y; r.vx = vx; r.vy = vy;
    r.ang = frand(0, 628);
    float sp = frand(8, 35);
    r.spin = rnd(2) ? sp * 0.1f : -sp * 0.1f;
    r.nv = (uint8_t)rndRange(7, 10);
    float step = TAU_F / (float)r.nv;
    for (int k = 0; k < r.nv; k++) {
      float a = (float)k * step + frand(-30, 30) * step;
      float rr = (float)ROCK_R[size] * frand(72, 118);
      r.ox[k] = (int8_t)roundi(cosf(a) * rr);
      r.oy[k] = (int8_t)roundi(sinf(a) * rr);
    }
    return i;
  }
  return -1;
}
static int rocksAlive() {
  int n = 0;
  for (int i = 0; i < MAX_ROCKS; i++) if (rocks[i].alive) n++;
  return n;
}
static float waveSpeedMul() { return fminf(1.0f + 0.07f * (float)(wave - 1), rockWaveCap); }

static void spawnWave() {
  int n = imin(rocksStart + wave - 1, rocksCap);
  float sx = shipAlive ? shipX : (float)FIELD_W / 2.0f;
  float sy = shipAlive ? shipY : (float)FIELD_Y + (float)FIELD_H / 2.0f;
  for (int i = 0; i < n; i++) {
    float x = 0, y = 0;
    bool ok = false;
    for (int t = 0; t < 40 && !ok; t++) {
      x = (float)rnd(FIELD_W);
      y = (float)(FIELD_Y + rnd(FIELD_H));
      float dx = x - sx, dy = y - sy;
      ok = (dx * dx + dy * dy) > 95.0f * 95.0f;
    }
    if (!ok) {   // never on top of the ship: fall back to the far side
      x = (sx < (float)FIELD_W / 2.0f) ? (float)(FIELD_W - 10) : 10.0f;
      y = (float)(FIELD_Y + 10);
    }
    float a = frand(0, 628);
    float sp = fminf(rockBase * waveSpeedMul() * frand(75, 125), 3.0f);
    spawnRock(0, x, y, cosf(a) * sp, sinf(a) * sp);
  }
}

// ------------------------------------------------------------------ debris / explosions
static void addDebris(float x, float y, int count, uint16_t col) {
  for (int k = 0; k < count; k++) {
    Debris& d = debris[debrisNext];
    debrisNext = (debrisNext + 1) % MAX_DEBRIS;
    float a = frand(0, 628), sp = frand(60, 260);
    d.x = x; d.y = y; d.vx = cosf(a) * sp; d.vy = sinf(a) * sp;
    d.life = (int16_t)rndRange(12, 22);
    d.col = col;
  }
}

static void hitRock(int i, bool award) {
  Rock r = rocks[i];
  rocks[i].alive = 0;
  if (award) addScore(ROCK_PTS[r.size]);
  addDebris(r.x, r.y, r.size == 0 ? 4 : 2, C_LGREY);
  beep(r.size == 0 ? 200 : (r.size == 1 ? 300 : 450), r.size == 0 ? 70 : 45);
  if (r.size < 2) {
    float base = atan2f(r.vy, r.vx);
    float sp = fminf(sqrtf(r.vx * r.vx + r.vy * r.vy) * 1.3f + 0.25f, 3.2f);
    for (int k = 0; k < 2; k++) {
      float a = base + (k ? 1.0f : -1.0f) * frand(30, 90);
      spawnRock(r.size + 1, r.x, r.y, cosf(a) * sp, sinf(a) * sp);
    }
  }
}

// ------------------------------------------------------------------ ship
static void killShip() {
  shipAlive = false;
  thrusting = false;
  addDebris(shipX, shipY, 8, C_GREEN);
  beep(120, 300);
  lives--;
  hudDirty = true;
  if (lives > 0) {
    mode = MODE_RESPAWN;
    respawnAt = millis() + 1300;
  } else {
    mode = MODE_DYING;
    overAt = millis() + 1300;
  }
}
static bool centerClear() {
  float cx = (float)FIELD_W / 2.0f, cy = (float)FIELD_Y + (float)FIELD_H / 2.0f;
  for (int i = 0; i < MAX_ROCKS; i++) {
    if (!rocks[i].alive) continue;
    float dx = rocks[i].x - cx, dy = rocks[i].y - cy;
    float lim = (float)ROCK_R[rocks[i].size] + 45.0f;
    if (dx * dx + dy * dy < lim * lim) return false;
  }
  return true;
}
static void spawnShip() {
  shipX = (float)FIELD_W / 2.0f;
  shipY = (float)FIELD_Y + (float)FIELD_H / 2.0f;
  shipVx = shipVy = 0;
  shipA = -PI_F / 2.0f;
  shipAlive = true;
  thrusting = false;
  mode = MODE_PLAY;
  invulnUntil = millis() + 2000;
}

// ------------------------------------------------------------------ rendering
static void buildSegs() {
  uint32_t now = millis();
  nSegs = 0;
  for (int i = 0; i < MAX_ROCKS; i++) {
    const Rock& r = rocks[i];
    if (!r.alive) continue;
    float c = cosf(r.ang), s = sinf(r.ang);
    uint16_t col = (r.size == 0) ? C_WHITE : (r.size == 1 ? C_CYAN : C_ORANGE);
    float px = 0, py = 0, fx = 0, fy = 0;
    for (int k = 0; k < r.nv; k++) {
      float vx = r.x + (float)r.ox[k] * c - (float)r.oy[k] * s;
      float vy = r.y + (float)r.ox[k] * s + (float)r.oy[k] * c;
      if (k == 0) { fx = vx; fy = vy; } else addSeg(px, py, vx, vy, col);
      px = vx; py = vy;
    }
    addSeg(px, py, fx, fy, col);
  }
  if (shipAlive) {
    bool hide = ((int32_t)(invulnUntil - now) > 0) && (((int32_t)(invulnUntil - now) / 130) & 1);
    if (!hide) {
      float hx = cosf(shipA), hy = sinf(shipA);
      float nx = -hy, ny = hx;
      float x0 = shipX + hx * 9.0f, y0 = shipY + hy * 9.0f;
      float x1 = shipX - hx * 6.0f + nx * 5.5f, y1 = shipY - hy * 6.0f + ny * 5.5f;
      float x2 = shipX - hx * 6.0f - nx * 5.5f, y2 = shipY - hy * 6.0f - ny * 5.5f;
      addSeg(x0, y0, x1, y1, COL_SHIP);
      addSeg(x1, y1, x2, y2, COL_SHIP);
      addSeg(x2, y2, x0, y0, COL_SHIP);
      if (thrusting) {
        float fl = 5.0f + (float)rnd(5);
        float bx = shipX - hx * 6.0f, by = shipY - hy * 6.0f;
        addSeg(bx + nx * 2.5f, by + ny * 2.5f, bx - hx * fl, by - hy * fl, COL_FLAME);
        addSeg(bx - nx * 2.5f, by - ny * 2.5f, bx - hx * fl, by - hy * fl, COL_FLAME);
      }
    }
  }
  for (int i = 0; i < MAX_BULLETS; i++) {
    const Bullet& b = bullets[i];
    if (b.life <= 0) continue;
    addSeg(b.x, b.y, b.x - b.vx * 0.5f, b.y - b.vy * 0.5f, COL_BULLET);
  }
  for (int i = 0; i < MAX_DEBRIS; i++) {
    const Debris& d = debris[i];
    if (d.life <= 0) continue;
    addSeg(d.x, d.y, d.x + d.vx * 2.5f, d.y + d.vy * 2.5f, d.col);
  }
}
static void render() {
  buildSegs();
  for (int i = 0; i < nPrev; i++) tft.drawLine(prevSegs[i].x0, prevSegs[i].y0, prevSegs[i].x1, prevSegs[i].y1, C_BLACK);
  for (int i = 0; i < nSegs; i++) {
    tft.drawLine(segs[i].x0, segs[i].y0, segs[i].x1, segs[i].y1, segs[i].col);
    prevSegs[i].x0 = segs[i].x0; prevSegs[i].y0 = segs[i].y0;
    prevSegs[i].x1 = segs[i].x1; prevSegs[i].y1 = segs[i].y1;
  }
  nPrev = nSegs;
}

// ------------------------------------------------------------------ game flow
static void startBanner() {
  bannerOn = true;
  bannerUntil = millis() + 1500;
  hudDirty = true;
}

static void reset() {
  startLives  = byDiff(5, 3, 2);
  rocksStart  = byDiff(3, 4, 5);
  rocksCap    = byDiff(5, 6, 7);
  rockBase    = (float)byDiff(55, 80, 110) / 100.0f;
  rockWaveCap = (float)byDiff(150, 170, 200) / 100.0f;
  fireMs      = byDiff(240, 280, 320);
  bulletLife  = byDiff(42, 34, 28);
  maxBullets  = byDiff(6, 5, 4);
  turnRate    = (float)byDiff(17, 14, 11) / 100.0f;
  thrustAcc   = (float)byDiff(13, 12, 12) / 100.0f;
  dragK       = (float)byDiff(970, 983, 990) / 1000.0f;
  extraEvery  = byDiff(2000, 3000, 4000);

  hudBegin("Asteroids");
  memset(rocks, 0, sizeof rocks);
  memset(bullets, 0, sizeof bullets);
  memset(debris, 0, sizeof debris);
  nSegs = nPrev = 0;
  debrisNext = 0;
  lives = startLives;
  score = 0;
  wave = 1;
  nextExtra = extraEvery;
  waveWait = false;
  bannerOn = false;
  nextFireAt = millis();
  spawnShip();
  spawnWave();
  startBanner();
  refreshHud();
}

static void init() { reset(); }

static void tick() {
  uint32_t now = millis();
  if (mode == MODE_OVER) {
    if (touch.pressed) reset();
    return;
  }

  // ---- ship control
  if (shipAlive) {
    thrusting = false;
    if (touch.down) {
      float dx = (float)touch.x - shipX, dy = (float)touch.y - shipY;
      float d = sqrtf(dx * dx + dy * dy);
      if (d > 10.0f) {
        float diff = atan2f(dy, dx) - shipA;
        while (diff > PI_F) diff -= TAU_F;
        while (diff < -PI_F) diff += TAU_F;
        shipA += fclampf(diff, -turnRate, turnRate);
        if (shipA > PI_F) shipA -= TAU_F;
        if (shipA < -PI_F) shipA += TAU_F;
      }
      if (d > (float)FINGER_NEAR) {
        shipVx += cosf(shipA) * thrustAcc;
        shipVy += sinf(shipA) * thrustAcc;
        thrusting = true;
      }
      if ((int32_t)(now - nextFireAt) >= 0) {
        int active = 0, slot = -1;
        for (int i = 0; i < MAX_BULLETS; i++) {
          if (bullets[i].life > 0) active++; else if (slot < 0) slot = i;
        }
        if (active < maxBullets && slot >= 0) {
          Bullet& b = bullets[slot];
          b.x = shipX + cosf(shipA) * 9.0f;
          b.y = shipY + sinf(shipA) * 9.0f;
          b.vx = cosf(shipA) * BULLET_V;
          b.vy = sinf(shipA) * BULLET_V;
          b.life = (int16_t)bulletLife;
          nextFireAt = now + (uint32_t)fireMs;
          beep(1300, 12);
        }
      }
    }
    shipVx *= dragK;
    shipVy *= dragK;
    float v = sqrtf(shipVx * shipVx + shipVy * shipVy);
    if (v > SHIP_MAX_V) { shipVx *= SHIP_MAX_V / v; shipVy *= SHIP_MAX_V / v; }
    shipX += shipVx;
    shipY += shipVy;
    wrapPos(shipX, shipY, 8.0f);
  } else if (mode == MODE_RESPAWN) {
    int32_t waited = (int32_t)(now - respawnAt);
    if (waited >= 0 && (centerClear() || waited > 2500)) spawnShip();
  }

  // ---- bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet& b = bullets[i];
    if (b.life <= 0) continue;
    b.life--;
    b.x += b.vx;
    b.y += b.vy;
    wrapPos(b.x, b.y, 0.0f);
    for (int j = 0; j < MAX_ROCKS && b.life > 0; j++) {
      if (!rocks[j].alive) continue;
      float rad = (float)ROCK_R[rocks[j].size] * 0.95f + 1.0f;
      float dx = b.x - rocks[j].x, dy = b.y - rocks[j].y;
      float mx = dx - b.vx * 0.5f, my = dy - b.vy * 0.5f;   // half-step sample against tunnelling
      if (dx * dx + dy * dy < rad * rad || mx * mx + my * my < rad * rad) {
        b.life = 0;
        hitRock(j, true);
      }
    }
  }

  // ---- rocks
  for (int i = 0; i < MAX_ROCKS; i++) {
    Rock& r = rocks[i];
    if (!r.alive) continue;
    r.x += r.vx;
    r.y += r.vy;
    r.ang += r.spin;
    if (r.ang > TAU_F) r.ang -= TAU_F;
    if (r.ang < 0.0f) r.ang += TAU_F;
    wrapPos(r.x, r.y, (float)ROCK_R[r.size] + 4.0f);
  }

  // ---- ship vs rocks
  if (shipAlive && (int32_t)(invulnUntil - now) <= 0) {
    for (int i = 0; i < MAX_ROCKS; i++) {
      if (!rocks[i].alive) continue;
      float rad = (float)ROCK_R[rocks[i].size] * 0.85f + 3.0f;
      float dx = shipX - rocks[i].x, dy = shipY - rocks[i].y;
      if (dx * dx + dy * dy < rad * rad) {
        hitRock(i, false);
        killShip();
        break;
      }
    }
  }

  // ---- debris
  for (int i = 0; i < MAX_DEBRIS; i++) {
    Debris& d = debris[i];
    if (d.life <= 0) continue;
    d.life--;
    d.x += d.vx;
    d.y += d.vy;
  }

  // ---- waves
  if (!waveWait && rocksAlive() == 0) {
    waveWait = true;
    waveAt = now + 1500;
  } else if (waveWait && (int32_t)(now - waveAt) >= 0) {
    waveWait = false;
    wave++;
    spawnWave();
    startBanner();
    beep(800, 100);
  }
  if (bannerOn && (int32_t)(now - bannerUntil) >= 0) {
    bannerOn = false;
    hudDirty = true;
  }

  render();
  if (hudDirty) refreshHud();

  if (mode == MODE_DYING && (int32_t)(now - overAt) >= 0) {
    mode = MODE_OVER;
    overlayGameOver(score);
  }
}

static void icon(int cx, int cy) {
  // rock
  static const int8_t rx[8] = {-6, -1, 5, 9, 6, 7, -1, -8};
  static const int8_t ry[8] = {-12, -15, -11, -3, 2, 9, 12, 5};
  for (int i = 0; i < 8; i++) {
    int j = (i + 1) & 7;
    tft.drawLine(cx - 6 + rx[i], cy - 4 + ry[i], cx - 6 + rx[j], cy - 4 + ry[j], C_WHITE);
  }
  // ship
  int sx = cx + 8, sy = cy + 10;
  tft.drawLine(sx, sy - 9, sx + 6, sy + 6, C_GREEN);
  tft.drawLine(sx + 6, sy + 6, sx - 6, sy + 6, C_GREEN);
  tft.drawLine(sx - 6, sy + 6, sx, sy - 9, C_GREEN);
  tft.drawLine(sx, sy - 12, sx, sy - 17, C_YELLOW);
}

}  // namespace Asteroids
