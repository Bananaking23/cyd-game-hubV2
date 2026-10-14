// Pong - drag your finger vertically to move the left paddle. First to 7 wins a match.
// Score = matches won in a row. Lose a match -> game over.
#pragma once
#include "hub.h"

namespace Pong {

static const int PW = 6, PH = 44, BS = 7;
static const int LX = 8, RX = SCR_W - 8 - PW;
static const int TOP = PF_Y + 2, BOT = SCR_H - 2;  // play area for ball/paddles
static const uint16_t BG = C_BLACK;

static float bx, by, vx, vy;           // ball
static float py, ay;                   // paddle tops (player, ai)
static int oldBx, oldBy, oldPy, oldAy;
static int sp, sa, streak, state;      // state: 0 ready, 1 playing, 2 point pause, 3 dead, 4 match won
static float aiSpeed, aiErr, ballSpeed;
static float aiSpeedCap, aiErrFloor, ballStartSpeed, ballSpeedCap;   // difficulty limits
static uint32_t pauseUntil;
static int serveDir;

static void drawNet() {
  for (int y = TOP; y < BOT; y += 12) tft.fillRect(SCR_W / 2 - 1, y, 2, 6, C_DGREY);
}
static void drawScores() {
  tft.fillRect(SCR_W / 2 - 60, PF_Y + 4, 120, 30, BG);
  char b[8];
  snprintf(b, sizeof b, "%d", sp);
  text(b, SCR_W / 2 - 30, PF_Y + 18, 4, C_CYAN, MC_DATUM);
  snprintf(b, sizeof b, "%d", sa);
  text(b, SCR_W / 2 + 30, PF_Y + 18, 4, C_ORANGE, MC_DATUM);
}
static void drawPaddles() {
  tft.fillRect(LX, oldPy, PW, PH, BG);
  tft.fillRect(RX, oldAy, PW, PH, BG);
  tft.fillRoundRect(LX, (int)py, PW, PH, 2, C_CYAN);
  tft.fillRoundRect(RX, (int)ay, PW, PH, 2, C_ORANGE);
  oldPy = (int)py;
  oldAy = (int)ay;
}
static void serve(int dir) {
  bx = SCR_W / 2 - BS / 2;
  by = (TOP + BOT) / 2 - BS / 2;
  ballSpeed = ballStartSpeed;
  float ang = (float)rndRange(-30, 30) * 0.0174533f;
  vx = ballSpeed * cosf(ang) * (float)dir;
  vy = ballSpeed * sinf(ang);
  oldBx = (int)bx;
  oldBy = (int)by;
}
static void newMatch() {
  sp = sa = 0;
  py = ay = (TOP + BOT) / 2 - PH / 2;
  oldPy = (int)py;
  oldAy = (int)ay;
  pfClear(BG);
  drawNet();
  drawScores();
  drawPaddles();
  serveDir = (rnd(2) ? 1 : -1);
  serve(serveDir);
  state = 0;
  text("Touch to serve", SCR_W / 2, PF_Y + 70, 2, C_WHITE, MC_DATUM);
  char b[24];
  snprintf(b, sizeof b, "First to 7");
  text(b, SCR_W / 2, PF_Y + 90, 2, C_GREY, MC_DATUM);
}
static void startAll() {
  hudBegin("Pong");
  streak = 0;
  aiSpeed = (float)byDiff(18, 24, 30) / 10.0f;
  aiErr = (float)byDiff(34, 22, 10);
  aiSpeedCap = (float)byDiff(38, 46, 50) / 10.0f;
  aiErrFloor = (float)byDiff(14, 6, 3);
  ballStartSpeed = (float)byDiff(28, 32, 38) / 10.0f;
  ballSpeedCap = (float)byDiff(64, 70, 76) / 10.0f;
  newMatch();
  hudScore(streak, "Wins");
}

static void init() { startAll(); }

static void pointTo(bool playerScored) {
  if (playerScored) { sp++; beep(1400, 80); }
  else              { sa++; beep(300, 150); }
  tft.fillRect((int)bx, (int)by, BS, BS, BG);
  drawNet();
  drawScores();
  if (sp >= 7) {
    streak++;
    hudScore(streak, "Wins");
    aiSpeed = fminf(aiSpeed + 0.35f, aiSpeedCap);
    aiErr = fmaxf(aiErr - 3.0f, aiErrFloor);
    state = 4;
    overlayMsg("YOU WIN", C_GREEN, "Match won!", "AI gets faster", "Tap for next match");
    return;
  }
  if (sa >= 7) {
    state = 3;
    overlayGameOver(streak, " wins");
    return;
  }
  serveDir = playerScored ? 1 : -1;
  serve(serveDir);
  state = 2;
  pauseUntil = millis() + 600;
}

static void tick() {
  if (state == 3) {
    if (touch.pressed) startAll();
    return;
  }
  if (state == 4) {
    if (touch.pressed) newMatch();
    return;
  }
  // player paddle follows the finger (smoothly, clamped)
  if (touch.down) {
    float target = (float)touch.y - PH / 2;
    float d = target - py;
    if (d > 9) d = 9;
    if (d < -9) d = -9;
    py += d;
  }
  py = (float)clampi((int)py, TOP, BOT - PH);
  if (state == 0) {
    if (touch.pressed) {
      state = 1;
      pfClear(BG);
      drawNet();
      drawScores();
      oldPy = (int)py;
      oldAy = (int)ay;
    }
    drawPaddles();
    return;
  }
  if (state == 2) {
    drawPaddles();
    if ((int32_t)(millis() - pauseUntil) >= 0) state = 1;
    return;
  }
  // AI: track the ball with limited speed and a fixed aiming error
  float aimY = (vx > 0 ? by : (TOP + BOT) / 2.0f) + BS / 2 - PH / 2;
  if (vx > 0) aimY += (float)((int)(bx / 8.0f) % 3 - 1) * aiErr * 0.5f;
  float da = aimY - ay;
  if (da > aiSpeed) da = aiSpeed;
  if (da < -aiSpeed) da = -aiSpeed;
  if (iabs((int)(aimY - ay)) > 4) ay += da;
  ay = (float)clampi((int)ay, TOP, BOT - PH);

  // ball
  tft.fillRect(oldBx, oldBy, BS, BS, BG);
  bx += vx;
  by += vy;
  if (by < TOP) { by = (float)TOP; vy = -vy; beep(700, 15); }
  if (by > BOT - BS) { by = (float)(BOT - BS); vy = -vy; beep(700, 15); }

  // paddle hits
  if (vx < 0 && bx <= LX + PW && bx + BS >= LX && by + BS >= py && by <= py + PH) {
    bx = (float)(LX + PW);
    float rel = ((by + BS / 2) - (py + PH / 2)) / (PH / 2.0f);   // -1..1
    ballSpeed = fminf(ballSpeed + 0.25f, ballSpeedCap);
    vx = ballSpeed * 0.9f;
    vy = ballSpeed * 0.8f * rel;
    beep(900, 25);
  } else if (vx > 0 && bx + BS >= RX && bx <= RX + PW && by + BS >= ay && by <= ay + PH) {
    bx = (float)(RX - BS);
    float rel = ((by + BS / 2) - (ay + PH / 2)) / (PH / 2.0f);
    ballSpeed = fminf(ballSpeed + 0.25f, ballSpeedCap);
    vx = -ballSpeed * 0.9f;
    vy = ballSpeed * 0.8f * rel;
    beep(800, 25);
  }
  if (bx < -BS) { pointTo(false); return; }
  if (bx > SCR_W) { pointTo(true); return; }

  oldBx = (int)bx;
  oldBy = (int)by;
  drawPaddles();
  tft.fillRect(oldBx, oldBy, BS, BS, C_WHITE);
  // the net may be overdrawn by the ball's erase; redraw cheaply when the ball is near the middle
  if (oldBx > SCR_W / 2 - 12 && oldBx < SCR_W / 2 + 12) drawNet();
}

static void icon(int cx, int cy) {
  tft.fillRoundRect(cx - 18, cy - 12, 5, 18, 2, C_CYAN);
  tft.fillRoundRect(cx + 13, cy - 6, 5, 18, 2, C_ORANGE);
  tft.fillRect(cx - 3, cy - 3, 6, 6, C_WHITE);
  for (int y = cy - 16; y < cy + 16; y += 8) tft.fillRect(cx - 1, y, 2, 4, C_GREY);
}

}  // namespace Pong
