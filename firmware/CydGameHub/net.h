// =============================================================================
//  net.h - Wi-Fi setup, on-screen keyboard and over-the-air (OTA) firmware updates.
//
//  Everything here runs as a *blocking sub-screen* (own small loop, like the touch calibration), entered from
//  Settings -> "Wi-Fi & updates". Nothing in the games depends on it, and Wi-Fi is switched off again afterwards.
//
//  Update source: UPDATE_BASE_URL (set at build time to the project's GitHub Pages address) serves
//     ota-<variant>.json   {"version","build","size","md5","path"}
//     firmware/app-<variant>.bin   the plain application image (not the merged flasher image)
//  TLS is validated against the ESP32 core's built-in root-certificate bundle (never "insecure"), which needs the
//  clock, so the time is fetched via NTP first. The download is checked against the size + MD5 in the json before the
//  new image is activated.
// =============================================================================
#pragma once
#include "hub.h"
#include "version.h"

// ------------------------------------------------------------------ small shared UI helpers
static void netFrame() {            // pace + poll, for the blocking sub-screen loops
  delay(30);
  soundTick();
  pollTouch();
}
static void netWaitRelease() {      // wait until the finger is up, so the next screen doesn't see the old touch
  uint32_t t0 = millis();
  while (touch.down && millis() - t0 < 3000) netFrame();
  touchClear();
}
static void netWaitTap() {          // wait for a complete tap anywhere
  for (;;) {
    netFrame();
    if (touch.released) break;
  }
  touchClear();
}
static bool menuHit() { return touch.pressed && inRect(touch.x, touch.y, SCR_W - 56, 0, 56, HUD_H + 2); }

static void netMessage(const char* title, uint16_t col, const char* l1, const char* l2 = 0) {
  overlayMsg(title, col, l1, l2, "Tap to continue");
  netWaitTap();
}
// Two-button question. Returns true for the left ("yes") button.
static bool netAsk(const char* title, uint16_t col, const char* l1, const char* l2, const char* yes, const char* no) {
  int w = 220, h = 112, x = (SCR_W - w) / 2, y = PF_Y + (PF_H - h) / 2;
  tft.fillRoundRect(x, y, w, h, 10, RGB565(14, 16, 28));
  tft.drawRoundRect(x, y, w, h, 10, col);
  text(title, SCR_W / 2, y + 20, 4, col, MC_DATUM);
  if (l1) text(l1, SCR_W / 2, y + 46, 2, C_WHITE, MC_DATUM);
  if (l2) text(l2, SCR_W / 2, y + 64, 2, C_GOLD, MC_DATUM);
  button(x + 10, y + 78, 96, 26, yes, RGB565(30, 130, 70));
  button(x + w - 106, y + 78, 96, 26, no, RGB565(90, 90, 110));
  for (;;) {
    netFrame();
    if (!touch.tap) continue;
    if (inRect(touch.x, touch.y, x, y + 70, w / 2, 42)) { touchClear(); return true; }
    if (inRect(touch.x, touch.y, x + w / 2, y + 70, w / 2, 42)) { touchClear(); return false; }
  }
}
static void netBusy(const char* title, const char* line) {
  hudBegin(title);
  text(line, SCR_W / 2, SCR_H / 2, 2, C_WHITE, MC_DATUM);
}

// ------------------------------------------------------------------ on-screen keyboard
struct KbKey { int16_t x, y, w; char c; uint8_t kind; };
enum { KB_CHAR = 0, KB_SHIFT, KB_BKSP, KB_MODE, KB_SPACE, KB_CANCEL, KB_OK };
static const int KB_Y0 = 60, KB_PITCH = 34, KB_MAXKEYS = 48;

static void kbAdd(KbKey* k, int& n, int x, int y, int w, char c, uint8_t kind) {
  k[n].x = (int16_t)x; k[n].y = (int16_t)y; k[n].w = (int16_t)w; k[n].c = c; k[n].kind = kind;
  n++;
}
static int kbBuild(KbKey* k, bool shift, bool sym) {
  static const char* const DIG = "1234567890";
  static const char* const LO[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  static const char* const UP[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  static const char* const SY[3] = {"@#$%&*-_+=", "!?.,:;/\\()", "~^|\"'<>`"};
  int n = 0;
  for (int i = 0; i < 10; i++) kbAdd(k, n, i * 32, KB_Y0, 32, DIG[i], KB_CHAR);
  const char* const* set = sym ? SY : (shift ? UP : LO);
  for (int i = 0; set[0][i]; i++) kbAdd(k, n, i * 32, KB_Y0 + KB_PITCH, 32, set[0][i], KB_CHAR);
  int off2 = sym ? 0 : 16;
  for (int i = 0; set[1][i]; i++) kbAdd(k, n, off2 + i * 32, KB_Y0 + 2 * KB_PITCH, 32, set[1][i], KB_CHAR);
  int y3 = KB_Y0 + 3 * KB_PITCH;
  if (sym) {
    for (int i = 0; set[2][i]; i++) kbAdd(k, n, i * 32, y3, 32, set[2][i], KB_CHAR);
  } else {
    kbAdd(k, n, 0, y3, 44, 0, KB_SHIFT);
    for (int i = 0; set[2][i]; i++) kbAdd(k, n, 44 + i * 32, y3, 32, set[2][i], KB_CHAR);
  }
  kbAdd(k, n, 268, y3, 52, 0, KB_BKSP);
  int y4 = KB_Y0 + 4 * KB_PITCH;
  kbAdd(k, n, 0, y4, 64, 0, KB_MODE);
  kbAdd(k, n, 64, y4, 128, ' ', KB_SPACE);
  kbAdd(k, n, 192, y4, 64, 0, KB_CANCEL);
  kbAdd(k, n, 256, y4, 64, 0, KB_OK);
  return n;
}
static void kbDrawKey(const KbKey& k, bool shift, bool sym, bool hot) {
  uint16_t bg = RGB565(44, 52, 76);
  const char* lab = 0;
  char one[2] = {k.c, 0};
  switch (k.kind) {
    case KB_CHAR:   lab = one; break;
    case KB_SPACE:  lab = "space"; break;
    case KB_SHIFT:  lab = "Aa"; bg = shift ? RGB565(40, 110, 190) : RGB565(70, 78, 104); break;
    case KB_BKSP:   lab = "Del"; bg = RGB565(110, 60, 60); break;
    case KB_MODE:   lab = sym ? "abc" : "123"; bg = RGB565(70, 78, 104); break;
    case KB_CANCEL: lab = "Cancel"; bg = RGB565(110, 50, 60); break;
    default:        lab = "OK"; bg = RGB565(30, 130, 70); break;
  }
  if (hot) bg = C_GOLD;
  tft.fillRoundRect(k.x + 1, k.y + 1, k.w - 2, KB_PITCH - 3, 4, bg);
  text(lab, k.x + k.w / 2, k.y + KB_PITCH / 2 - 1, 2, hot ? C_BLACK : C_WHITE, MC_DATUM);
}
static void kbDrawAll(const KbKey* k, int n, bool shift, bool sym) {
  tft.fillRect(0, KB_Y0, SCR_W, SCR_H - KB_Y0, C_BLACK);
  for (int i = 0; i < n; i++) kbDrawKey(k[i], shift, sym, false);
}
static void kbDrawField(const char* buf, int len, bool show, bool canShow) {
  tft.fillRect(0, PF_Y + 2, SCR_W, 34, C_BLACK);
  int fw = canShow ? 250 : 308;
  tft.fillRect(6, 28, fw, 26, HUD_BG);
  tft.drawRect(6, 28, fw, 26, C_GREY);
  char vis[40];
  int maxv = canShow ? 28 : 36;
  int start = len > maxv ? len - maxv : 0;
  int m = len - start;
  for (int i = 0; i < m; i++) vis[i] = show ? buf[start + i] : '*';
  vis[m] = 0;
  text(vis, 10, 41, 2, C_WHITE, ML_DATUM);
  if (canShow) button(262, 28, 52, 26, show ? "Hide" : "Show", RGB565(70, 78, 104));
}

// Edit `buf` (capacity maxLen+1, holds the initial text). Returns true on OK, false on Cancel / MENU.
static bool keyboardInput(const char* title, char* buf, int maxLen, bool secret) {
  KbKey keys[KB_MAXKEYS];
  bool shift = false, sym = false, show = !secret;
  int len = (int)strlen(buf);
  if (len > maxLen) { len = maxLen; buf[len] = 0; }
  hudBegin(title);
  int n = kbBuild(keys, shift, sym);
  kbDrawField(buf, len, show, secret);
  kbDrawAll(keys, n, shift, sym);
  netWaitRelease();
  for (;;) {
    netFrame();
    if (menuHit()) { netWaitRelease(); return false; }
    if (!touch.pressed) continue;
    int px = touch.x, py = touch.y;
    if (secret && inRect(px, py, 258, 24, 60, 34)) {
      show = !show;
      kbDrawField(buf, len, show, secret);
      continue;
    }
    int hit = -1;
    for (int i = 0; i < n; i++)
      if (inRect(px, py, keys[i].x, keys[i].y, keys[i].w, KB_PITCH)) { hit = i; break; }
    if (hit < 0) continue;
    const KbKey& k = keys[hit];
    kbDrawKey(k, shift, sym, true);
    beep(1300, 12);
    switch (k.kind) {
      case KB_CHAR:
      case KB_SPACE:
        if (len < maxLen) { buf[len++] = k.c; buf[len] = 0; }
        if (shift && !sym) { shift = false; n = kbBuild(keys, shift, sym); kbDrawAll(keys, n, shift, sym); }
        break;
      case KB_BKSP:
        if (len > 0) buf[--len] = 0;
        break;
      case KB_SHIFT:
        shift = !shift;
        n = kbBuild(keys, shift, sym);
        kbDrawAll(keys, n, shift, sym);
        break;
      case KB_MODE:
        sym = !sym;
        shift = false;
        n = kbBuild(keys, shift, sym);
        kbDrawAll(keys, n, shift, sym);
        break;
      case KB_CANCEL:
        netWaitRelease();
        return false;
      default:  // OK
        netWaitRelease();
        return true;
    }
    kbDrawField(buf, len, show, secret);
    // un-highlight the key that was just pressed (the layer redraw above already did it for layer changes)
    if (k.kind == KB_CHAR || k.kind == KB_SPACE || k.kind == KB_BKSP) kbDrawKey(k, shift, sym, false);
  }
}

// ------------------------------------------------------------------ Wi-Fi credentials + connection
struct WifiCreds { char ssid[33]; char pass[65]; };

static bool wifiLoadCreds(WifiCreds& c) {
  memset(&c, 0, sizeof c);
  size_t got = prefs.getBytes("wifi", &c, sizeof c);
  c.ssid[32] = 0;
  c.pass[64] = 0;
  return got == sizeof c && c.ssid[0] != 0;
}
static void wifiSaveCreds(const char* ssid, const char* pass) {
  WifiCreds c;
  memset(&c, 0, sizeof c);
  strncpy(c.ssid, ssid, 32);
  strncpy(c.pass, pass, 64);
  prefs.putBytes("wifi", &c, sizeof c);
}
static void wifiForget() {
  WifiCreds c;
  memset(&c, 0, sizeof c);
  prefs.putBytes("wifi", &c, sizeof c);
}
static void wifiOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}
// Connect and wait (shows progress). Returns true when an IP address was obtained.
static bool wifiConnect(const char* ssid, const char* pass) {
  netBusy("Wi-Fi", "Connecting...");
  char nm[40];
  snprintf(nm, sizeof nm, "%.30s", ssid);
  text(nm, SCR_W / 2, SCR_H / 2 + 24, 2, C_CYAN, MC_DATUM);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    soundTick();
    if (millis() - t0 > 16000) return false;
    char d[8];
    dots = (dots + 1) % 4;
    snprintf(d, sizeof d, "%.*s", dots, "...");
    textBox(d, SCR_W / 2, SCR_H / 2 + 52, 60, 20, 2, C_GREY, C_BLACK);
  }
  return true;
}

// Scan, list, let the user pick, ask for the password, connect and remember it. True if connected.
static bool wifiPickAndConnect() {
  static const int MAXN = 14, PERPAGE = 5;
  static char ssids[MAXN][33];
  static int8_t rssis[MAXN];
  static bool lock[MAXN];
  int count = 0, page = 0;
  bool rescan = true;
  for (;;) {
    if (rescan) {
      rescan = false;
      netBusy("Wi-Fi", "Scanning for networks...");
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      int found = WiFi.scanNetworks();
      count = 0;
      for (int i = 0; i < found && i < 40; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;
        bool dup = false;
        for (int j = 0; j < count; j++) if (strcmp(ssids[j], s.c_str()) == 0) dup = true;
        if (dup) continue;
        int slot = count;
        if (count < MAXN) count++;
        else {                                  // keep the strongest MAXN
          int weakest = 0;
          for (int j = 1; j < MAXN; j++) if (rssis[j] < rssis[weakest]) weakest = j;
          if (WiFi.RSSI(i) <= rssis[weakest]) continue;
          slot = weakest;
        }
        strncpy(ssids[slot], s.c_str(), 32);
        ssids[slot][32] = 0;
        rssis[slot] = (int8_t)clampi(WiFi.RSSI(i), -127, 0);
        lock[slot] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
      WiFi.scanDelete();
      for (int a = 0; a < count; a++)           // strongest first
        for (int b = a + 1; b < count; b++)
          if (rssis[b] > rssis[a]) {
            char tmp[33]; memcpy(tmp, ssids[a], 33); memcpy(ssids[a], ssids[b], 33); memcpy(ssids[b], tmp, 33);
            int8_t r = rssis[a]; rssis[a] = rssis[b]; rssis[b] = r;
            bool l = lock[a]; lock[a] = lock[b]; lock[b] = l;
          }
      page = 0;
    }
    // draw the list
    hudBegin("Choose network");
    if (count == 0) text("No networks found", SCR_W / 2, 90, 2, C_ORANGE, MC_DATUM);
    for (int i = 0; i < PERPAGE; i++) {
      int idx = page * PERPAGE + i;
      if (idx >= count) break;
      int y = PF_Y + 4 + i * 31;
      tft.fillRoundRect(6, y, 308, 28, 6, RGB565(22, 26, 38));
      tft.drawRoundRect(6, y, 308, 28, 6, RGB565(60, 70, 100));
      char nm[26];
      snprintf(nm, sizeof nm, "%.24s", ssids[idx]);
      text(nm, 14, y + 14, 2, C_WHITE, ML_DATUM);
      int bars = rssis[idx] > -55 ? 4 : (rssis[idx] > -67 ? 3 : (rssis[idx] > -78 ? 2 : 1));
      for (int b = 0; b < 4; b++)
        tft.fillRect(280 + b * 6, y + 22 - (b + 1) * 5, 4, (b + 1) * 5, b < bars ? C_GREEN : C_DGREY);
      if (lock[idx]) text("*", 266, y + 14, 2, C_GOLD, MC_DATUM);
    }
    int pages = (count + PERPAGE - 1) / PERPAGE;
    button(6, 188, 60, 30, "<", RGB565(40, 50, 80));
    button(78, 188, 164, 30, "RESCAN", RGB565(40, 50, 80));
    button(254, 188, 60, 30, ">", RGB565(40, 50, 80));
    char pg[16];
    snprintf(pg, sizeof pg, "%d / %d", pages == 0 ? 0 : page + 1, pages);
    text(pg, SCR_W / 2, 228, 1, C_GREY, MC_DATUM);
    netWaitRelease();
    // wait for an action
    int chosen = -1;
    for (;;) {
      netFrame();
      if (menuHit()) { netWaitRelease(); return false; }
      if (!touch.tap) continue;
      int px = touch.x, py = touch.y;
      if (inRect(px, py, 0, 184, 72, 40)) { if (pages > 1) { page = (page + pages - 1) % pages; } break; }
      if (inRect(px, py, 254, 184, 66, 40)) { if (pages > 1) { page = (page + 1) % pages; } break; }
      if (inRect(px, py, 72, 184, 176, 40)) { rescan = true; break; }
      for (int i = 0; i < PERPAGE; i++) {
        int idx = page * PERPAGE + i;
        if (idx < count && inRect(px, py, 6, PF_Y + 4 + i * 31, 308, 29)) { chosen = idx; break; }
      }
      if (chosen >= 0) break;
    }
    touchClear();
    if (chosen < 0) continue;            // page flip or rescan: redraw
    char pass[65] = "";
    if (lock[chosen]) {
      if (!keyboardInput("Wi-Fi password", pass, 63, true)) continue;
    }
    if (wifiConnect(ssids[chosen], pass)) {
      wifiSaveCreds(ssids[chosen], pass);
      memset(pass, 0, sizeof pass);
      return true;
    }
    memset(pass, 0, sizeof pass);
    WiFi.disconnect();
    netMessage("FAILED", C_RED, "Could not connect.", "Wrong password?");
  }
}

// ------------------------------------------------------------------ update check + install
struct OtaInfo { int build, size; char version[16]; char md5[40]; char path[72]; };

static bool jsonFind(const char* js, const char* key, const char** val) {
  char pat[24];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char* p = strstr(js, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  *val = p;
  return true;
}
static bool jsonInt(const char* js, const char* key, int* out) {
  const char* p;
  if (!jsonFind(js, key, &p) || *p < '0' || *p > '9') return false;
  long v = 0;
  while (*p >= '0' && *p <= '9' && v < 100000000L) { v = v * 10 + (*p - '0'); p++; }
  *out = (int)v;
  return true;
}
static bool jsonStr(const char* js, const char* key, char* out, int cap) {
  const char* p;
  if (!jsonFind(js, key, &p) || *p != '"') return false;
  p++;
  int i = 0;
  while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
  out[i] = 0;
  return *p == '"';
}

// Root-certificate bundle that ships inside the ESP32 Arduino core (arduino-esp32 2.x calling convention).
extern const uint8_t hubRootBundle[] asm("_binary_x509_crt_bundle_start");

static bool netSyncClock() {
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  uint32_t t0 = millis();
  while ((long)time(nullptr) < 1700000000L) {
    delay(200);
    if (millis() - t0 > 15000) return false;
  }
  return true;
}
static void netUrl(char* out, size_t cap, const char* tail) {
  snprintf(out, cap, "%s%s", UPDATE_BASE_URL, tail);
}

// Download ota-<variant>.json. On failure returns false and fills err.
static bool otaFetchInfo(OtaInfo& info, char* err, size_t errCap) {
  char url[160];
  char tail[40];
  snprintf(tail, sizeof tail, "ota-%s.json", HUB_VARIANT);
  netUrl(url, sizeof url, tail);
  WiFiClientSecure client;
  client.setCACertBundle(hubRootBundle);
  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) { snprintf(err, errCap, "bad update address"); return false; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(err, errCap, code > 0 ? "server said %d" : "no connection (%d)", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  memset(&info, 0, sizeof info);
  const char* js = body.c_str();
  if (!jsonInt(js, "build", &info.build) || !jsonInt(js, "size", &info.size) ||
      !jsonStr(js, "md5", info.md5, sizeof info.md5) || !jsonStr(js, "path", info.path, sizeof info.path)) {
    snprintf(err, errCap, "unreadable update info");
    return false;
  }
  if (!jsonStr(js, "version", info.version, sizeof info.version)) snprintf(info.version, sizeof info.version, "?");
  if (info.size < 100000 || info.size > 2000000 || strlen(info.md5) != 32) {
    snprintf(err, errCap, "update info invalid");
    return false;
  }
  return true;
}

static void otaProgress(int pct) {
  static int lastPct = -1;
  if (pct == lastPct) return;
  lastPct = pct;
  tft.fillRect(20, 120, 280, 20, C_DGREY);
  tft.fillRect(20, 120, 280 * pct / 100, 20, C_GREEN);
  tft.drawRect(20, 120, 280, 20, C_WHITE);
  char b[12];
  snprintf(b, sizeof b, "%d%%", pct);
  textBox(b, SCR_W / 2, 160, 80, 24, 4, C_WHITE, C_BLACK);
  if (pct >= 100) lastPct = -1;
}

// Download the application image and flash it into the other OTA slot. True on success (caller restarts).
static bool otaInstall(const OtaInfo& info, char* err, size_t errCap) {
  char url[200];
  netUrl(url, sizeof url, info.path);
  WiFiClientSecure client;
  client.setCACertBundle(hubRootBundle);
  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) { snprintf(err, errCap, "bad update address"); return false; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) { snprintf(err, errCap, "download failed (%d)", code); http.end(); return false; }
  int len = http.getSize();
  if (len != info.size) { snprintf(err, errCap, "size mismatch"); http.end(); return false; }
  if (!Update.begin((size_t)len)) {
    snprintf(err, errCap, "no room: flash by USB once");
    http.end();
    return false;
  }
  Update.setMD5(info.md5);
  netBusy("Updating", "Downloading - keep powered");
  otaProgress(0);
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  int got = 0;
  uint32_t lastData = millis();
  while (got < len) {
    int avail = stream->available();
    if (avail > 0) {
      int n = stream->read(buf, (size_t)imin(avail, (int)sizeof buf));
      if (n <= 0) continue;
      if (Update.write(buf, (size_t)n) != (size_t)n) {
        snprintf(err, errCap, "flash write failed");
        Update.abort();
        http.end();
        return false;
      }
      got += n;
      lastData = millis();
      otaProgress((int)((long)got * 100 / len));
    } else {
      if (millis() - lastData > 20000 || !http.connected()) {
        if (got < len) {
          snprintf(err, errCap, "download interrupted");
          Update.abort();
          http.end();
          return false;
        }
      }
      delay(2);
    }
    soundTick();
  }
  http.end();
  if (!Update.end()) {
    snprintf(err, errCap, "%s", Update.errorString());
    return false;
  }
  otaProgress(100);
  return true;
}

// The whole "Check for updates" flow.
static void netCheckForUpdates() {
  if (strlen(UPDATE_BASE_URL) == 0) {
    netMessage("NO ADDRESS", C_ORANGE, "This build has no update", "address (see README).");
    return;
  }
  WifiCreds c;
  bool have = wifiLoadCreds(c);
  if (!have) {
    if (!wifiPickAndConnect()) { wifiOff(); return; }
  } else if (!wifiConnect(c.ssid, c.pass)) {
    wifiOff();
    netMessage("NO WI-FI", C_RED, "Could not join:", c.ssid);
    return;
  }
  netBusy("Updates", "Checking the time...");
  if (!netSyncClock()) {
    wifiOff();
    netMessage("NO INTERNET", C_RED, "Could not get the time.", "Try again later.");
    return;
  }
  netBusy("Updates", "Looking for updates...");
  OtaInfo info;
  char err[40];
  if (!otaFetchInfo(info, err, sizeof err)) {
    wifiOff();
    netMessage("CHECK FAILED", C_RED, err);
    return;
  }
  if (info.build <= HUB_BUILD) {
    wifiOff();
    char l[40];
    snprintf(l, sizeof l, "You have v%s", HUB_VERSION);
    netMessage("UP TO DATE", C_GREEN, l, "Nothing to install.");
    return;
  }
  char l1[40], l2[40];
  snprintf(l1, sizeof l1, "New version v%s", info.version);
  snprintf(l2, sizeof l2, "%d KB download", info.size / 1024);
  if (!netAsk("UPDATE", C_CYAN, l1, l2, "INSTALL", "LATER")) {
    wifiOff();
    return;
  }
  if (!otaInstall(info, err, sizeof err)) {
    wifiOff();
    netMessage("UPDATE FAILED", C_RED, err, "Nothing was changed.");
    return;
  }
  netBusy("Updated", "Restarting...");
  beep(1500, 200);
  delay(700);
  ESP.restart();
}

// ------------------------------------------------------------------ Settings -> Wi-Fi & updates screen
static void netDrawScreen() {
  hudBegin("Wi-Fi & updates");
  char b[48];
  snprintf(b, sizeof b, "Installed: v%s (%s)", HUB_VERSION, HUB_VARIANT);
  text(b, 10, PF_Y + 8, 2, C_WHITE);
  WifiCreds c;
  if (wifiLoadCreds(c)) {
    snprintf(b, sizeof b, "Wi-Fi: %.24s", c.ssid);
    text(b, 10, PF_Y + 30, 2, C_CYAN);
  } else {
    text("Wi-Fi: not set up", 10, PF_Y + 30, 2, C_GREY);
  }
  button(20, 90, 280, 38, "CHECK FOR UPDATES", RGB565(30, 120, 80));
  button(20, 136, 280, 38, "CHOOSE WI-FI NETWORK", RGB565(40, 90, 150));
  button(20, 182, 280, 38, "FORGET WI-FI", RGB565(130, 50, 60));
}
static void netScreen() {
  netDrawScreen();
  netWaitRelease();
  for (;;) {
    netFrame();
    if (menuHit()) { netWaitRelease(); break; }
    if (!touch.tap) continue;
    int px = touch.x, py = touch.y;
    if (inRect(px, py, 20, 90, 280, 38)) {
      touchClear();
      netCheckForUpdates();
      netDrawScreen();
    } else if (inRect(px, py, 20, 136, 280, 38)) {
      touchClear();
      if (wifiPickAndConnect()) netMessage("CONNECTED", C_GREEN, "Wi-Fi saved.");
      wifiOff();
      netDrawScreen();
    } else if (inRect(px, py, 20, 182, 280, 38)) {
      touchClear();
      wifiForget();
      beep(500, 60);
      netDrawScreen();
    }
  }
  touchClear();
}
