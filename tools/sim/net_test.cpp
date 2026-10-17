// Scripted UI test for the keyboard, Wi-Fi picker, update flow and Wi-Fi screen (net.h) with fake hardware.
// The "finger" is driven from delay(30) - the pace of every blocking UI loop - so taps only land while a screen waits.
#include <stdio.h>
#include <string>
#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "sim_env.h"
#include "../../firmware/CydGameHub/CydGameHub.ino"

struct Ev { int x, y; };
static std::vector<Ev> q;
static size_t qi = 0;
static int hold = 0, gap = 0, idle = 0;
static void tapHook(uint32_t ms) {
  if (ms != 30) return;                       // only the UI frame pacing advances the script
  if (hold > 0) { hold--; g_simTouched = true; return; }
  g_simTouched = false;
  if (gap > 0) { gap--; return; }
  if (qi < q.size()) {
    int x = q[qi].x, y = q[qi].y; qi++;
    g_simRawX = cal.ax0 + (int)((long)(x - 20) * (cal.ax1 - cal.ax0) / 280);
    g_simRawY = cal.ay0 + (int)((long)(y - 20) * (cal.ay1 - cal.ay0) / 200);
    hold = 3; gap = 9; g_simTouched = true; hold--;
  } else if (++idle > 4000) { fprintf(stderr, "script exhausted but UI still waiting - stuck?\n"); exit(2); }
}
static void run(const std::vector<Ev>& v, int lead = 0) { q = v; qi = 0; hold = idle = 0; gap = lead; g_simTouched = false; touch = TouchState{false, false, false, false, -1, 0, 0, 0, 0, 0, 0}; gMiss = 0; }

static Ev keyPos(char c, bool shift, bool sym, int kind = KB_CHAR) {
  KbKey k[KB_MAXKEYS]; int n = kbBuild(k, shift, sym);
  for (int i = 0; i < n; i++) if (k[i].kind == kind && (kind != KB_CHAR || k[i].c == c)) return Ev{k[i].x + k[i].w / 2, k[i].y + 12};
  printf("FAIL: key '%c' kind %d not found\n", c, kind); exit(1);
}
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

int main() {
  g_delayHook = tapHook;
  prefs.begin("sim"); settingsLoad(); cal = TouchCal{0, 400, 3700, 400, 3700};
  Ev menu = {300, 10};

  // 1. keyboard: type, shift, symbols, delete, OK
  { char buf[65] = ""; 
    run({keyPos('h', 0, 0), keyPos('i', 0, 0), keyPos(0, 0, 0, KB_SHIFT), keyPos('T', 1, 0), keyPos(0, 0, 0, KB_MODE), keyPos('@', 0, 1),
         keyPos(0, 0, 1, KB_MODE), keyPos(0, 0, 0, KB_BKSP), keyPos(0, 0, 0, KB_OK)});
    bool ok = keyboardInput("Wi-Fi password", buf, 63, true);
    CHECK(ok && strcmp(buf, "hiT") == 0, "keyboard: h i Shift T @ (symbols) Del OK -> \"hiT\"");
    run({keyPos(0, 0, 0, KB_CANCEL)}); char b2[65] = "keep"; ok = keyboardInput("x", b2, 63, false);
    CHECK(!ok, "keyboard: Cancel returns false");
    run({menu}); ok = keyboardInput("x", b2, 63, false);
    CHECK(!ok, "keyboard: MENU returns false");
    // length limit + show/hide toggle
    std::vector<Ev> many; for (int i = 0; i < 80; i++) many.push_back(keyPos('a', 0, 0)); many.insert(many.begin() + 3, Ev{288, 40}); many.push_back(keyPos(0, 0, 0, KB_OK));
    char b3[65] = ""; run(many); ok = keyboardInput("x", b3, 63, true);
    CHECK(ok && strlen(b3) == 63, "keyboard: input is capped at 63 characters, Show/Hide toggle ok");
  }
  // 2. wifi picker: pick network 0 (strongest, locked) -> password -> connected + saved
  { g_wifiPassOK = true; wifiForget(); WifiCreds c;
    run({Ev{100, 44}, keyPos('p', 0, 0), keyPos('w', 0, 0), keyPos(0, 0, 0, KB_OK)});
    bool ok = wifiPickAndConnect();
    CHECK(ok && wifiLoadCreds(c) && c.ssid[0], "wifi picker: choose network, type password, connect, credentials saved");
    // wrong password path: fail message, back to the list, leave with MENU
    g_wifiPassOK = false;
    run({Ev{100, 44}, keyPos('p', 0, 0), keyPos(0, 0, 0, KB_OK), Ev{160, 120}, menu});
    ok = wifiPickAndConnect();
    CHECK(!ok, "wifi picker: wrong password shows failure, MENU leaves");
    // paging + rescan + empty scan
    g_wifiPassOK = true; g_scanCount = 12;
    run({Ev{290, 200}, Ev{30, 200}, Ev{160, 200}, menu}); ok = wifiPickAndConnect();
    CHECK(!ok, "wifi picker: next / prev page, rescan, leave");
    g_scanCount = 0; run({menu}); ok = wifiPickAndConnect(); CHECK(!ok, "wifi picker: no networks found screen");
    g_scanCount = 60; run({menu}); ok = wifiPickAndConnect(); CHECK(!ok, "wifi picker: 60 networks (more than the list holds)");
    g_scanCount = 12;
  }
  // 3. update flow variants
  int w = 220, h = 112, ax = (SCR_W - w) / 2, ay = PF_Y + (PF_H - h) / 2;
  Ev install = {ax + 10 + 48, ay + 78 + 13}, later = {ax + w - 10 - 48, ay + 78 + 13};
  { g_wifiPassOK = true; wifiSaveCreds("HomeNet", "secret123");
    g_httpMode = 0; g_restarts = 0; g_updWritten = 0;
    run({install}); netCheckForUpdates();
    CHECK(g_restarts == 1 && g_updWritten == g_binSize, "update: newer build found, installed, board restarts");
    g_restarts = 0; run({later}); netCheckForUpdates(); CHECK(g_restarts == 0, "update: LATER does nothing");
    g_httpMode = 1; run({Ev{160, 120}}); netCheckForUpdates(); CHECK(g_restarts == 0, "update: garbage update info is rejected");
    g_httpMode = 3; run({Ev{160, 120}}); netCheckForUpdates(); CHECK(g_restarts == 0, "update: server error handled");
    g_httpMode = 2; run({install, Ev{160, 120}}); netCheckForUpdates(); CHECK(g_restarts == 0, "update: size mismatch aborts before flashing");
    g_httpMode = 4; run({install, Ev{160, 120}}); netCheckForUpdates(); CHECK(g_restarts == 0 && !g_updBegun, "update: stalled download aborts cleanly");
    g_httpMode = 0; g_wifiPassOK = false; run({Ev{160, 120}}); netCheckForUpdates(); CHECK(g_restarts == 0, "update: saved Wi-Fi unreachable handled");
    g_wifiPassOK = true; wifiForget(); run({Ev{100, 44}, keyPos('p', 0, 0), keyPos(0, 0, 0, KB_OK), later}); netCheckForUpdates();
    CHECK(g_restarts == 0, "update: no saved Wi-Fi -> picker -> connect -> LATER");
  }
  // 4. Settings -> Wi-Fi & updates screen
  { wifiSaveCreds("HomeNet", "x"); WifiCreds c;
    run({Ev{160, 201}, menu}); netScreen();
    CHECK(!wifiLoadCreds(c), "wifi screen: FORGET WI-FI clears the saved network");
    run({Ev{160, 155}, menu, menu}); g_scanCount = 0; netScreen(); g_scanCount = 12;
    CHECK(true, "wifi screen: CHOOSE WI-FI opens and returns");
  }
  // 5. silent start-up check + its Settings toggle
  { g_wifiPassOK = true; wifiSaveCreds("HomeNet", "secret123"); g_httpMode = 0; gAutoUpd = 1;
    g_restarts = 0; g_updWritten = 0;
    run({install}, 40); bool shown = netAutoCheck();
    CHECK(shown && g_restarts == 1 && g_updWritten == g_binSize, "auto-check: newer build -> pop-up -> INSTALL updates and restarts");
    g_restarts = 0; run({later}, 40); shown = netAutoCheck();
    CHECK(shown && g_restarts == 0, "auto-check: pop-up -> LATER does nothing");
    g_httpMode = 3; run({}); shown = netAutoCheck(); CHECK(!shown && g_restarts == 0, "auto-check: server error is silent");
    g_httpMode = 1; run({}); shown = netAutoCheck(); CHECK(!shown, "auto-check: garbage info is silent");
    g_httpMode = 0; g_wifiPassOK = false; run({}); shown = netAutoCheck(); CHECK(!shown, "auto-check: unreachable Wi-Fi is silent");
    g_wifiPassOK = true; gAutoUpd = 0; run({install}); shown = netAutoCheck(); CHECK(!shown && g_restarts == 0, "auto-check: switched off -> never connects");
    gAutoUpd = 1; wifiForget(); run({install}); shown = netAutoCheck(); CHECK(!shown, "auto-check: no saved Wi-Fi -> nothing happens");
    wifiSaveCreds("HomeNet", "secret123");
    g_restarts = 0; run({install}, 0); shown = netAutoCheck(); CHECK(!shown && g_restarts == 0, "auto-check: a touch while connecting skips the check");
    run({Ev{160, 73}, menu}); netScreen(); CHECK(gAutoUpd == 0 && prefs.getUChar("autoupd", 1) == 0, "wifi screen: toggle turns the start-up check off and saves it");
    run({Ev{160, 73}, menu}); netScreen(); CHECK(gAutoUpd == 1 && prefs.getUChar("autoupd", 0) == 1, "wifi screen: toggle turns it back on");
  }
  printf("\n%s (%d failures)\n", fails ? "NET TEST FAILED" : "net test passed", fails);
  return fails ? 1 : 0;
}
