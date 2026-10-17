# CYD Game Hub - handoff note

Paste this into a new chat together with the latest project zip (`cyd-game-hub-v11.zip` or newer).

## What it is
Firmware + browser web flasher for an ESP32-2432S028R "Cheap Yellow Display" (2.8", 320x240, resistive touch).
Built with arduino-cli (esp32 core 2.0.17, TFT_eSPI 2.5.43, XPT2046_Touchscreen 1.4). Published by the user through
GitHub Desktop -> GitHub Pages via the Actions workflow in `.github/workflows/build.yml`. The user has the
**2-USB board (USB-C + micro-USB, ST7789)** and flashes the `st7789` variant.

## Current features (v11, version 2.0.x)
- 24 games (Snake, Flap, Breakout, Pong, Invaders, Asteroids, Tetris, 2048, Simon, Whack, Runner, Tic-Tac-Toe, Connect 4,
  Reversi, Memory, Minesweeper, Lights Out, Sudoku, Slide, Jumper, Frogger, Gems, Mastermind, Fleet; each is a game_*.h header with its own namespace, registered in games.h; 8 tiles per menu page = 3 pages; tools/sim/games_test.cpp holds their behaviour tests). Easy/Normal/Hard with separate high scores per difficulty.
- Settings is a 4x3 tile grid: Sound, Brightness, Difficulty, Rotation, Invert, Calibrate, Scores (asks first),
  Wi-Fi, Clock, Clock style, Red/Blue (ST7789 only), Theme (light/dark).
- Start-up update check (netAutoCheck in net.h, toggle "autoupd" pref, default on): silent, pop-up only if newer.
- Wi-Fi (on-screen keyboard, network picker, password stored unencrypted in NVS) and wireless OTA updates from the
  GitHub Pages site (`ota-<variant>.json` + `firmware/app-<variant>.bin`, size + MD5 checked, TLS via the core's cert bundle).
  Needs the `min_spiffs` partition scheme, so older boards needed one USB reflash.
- Clock screensaver: digital (seven-segment) or analog, 12/24 h, seconds, date formats, colour, own brightness,
  17 time zones with DST, auto-start after idle on the menu. Time via NTP over the saved Wi-Fi (no battery-backed RTC).
- Clock weather (Open-Meteo, no key): icon + temperature + condition + today's high/low. Place = `Place` row: ZONE CITY (the
  selected time zone's city, `CLK_TZ_LAT/LON`) or one of `CLK_PLACES` (UK/Ireland cities incl. Manchester - the user is in
  Manchester); UTC + ZONE CITY = no weather. `Weather` row = off / Celsius / Fahrenheit. Clock style screen has 10 rows now
  (row pitch 21 px). Location id (`wxLoc()`): 1..16 zone city, 100+n place; cached data is only shown for its own id. Fetched when the clock starts, then every 30 min (retry 10 min), silently
  over the saved Wi-Fi, also on auto-start; hidden after 3 h without a refresh. `ClockCfg` grew `wx` (9th byte) and `place` (10th); the old
  8- and 9-byte NVS records still load (weather defaults to Celsius, place to ZONE CITY). Digital: one row at y=212; analog: icon/temp left of the dial,
  condition + H/L right (the dial shrinks to r=84 whenever weather is active).
- Light/dark theme applies to menu, settings, Wi-Fi screens and clock only; games and tiles stay dark.
- ST7789 defaults on a fresh install: Invert on, BGR order (this is what the user's panel needed).

## Files that matter
`firmware/CydGameHub/`: `CydGameHub.ino` (menu, settings, loop), `hub.h` (touch, HUD, sound, scores, theme, settings),
`net.h` (Wi-Fi, keyboard, OTA), `clock.h` (screensaver), `games.h` + `game_*.h` (one namespace per game), `version.h`.
`tools/build.py` (builds both variants, writes `web/`), `tools/gen_undef.py` (regenerates the `#undef` safety net in hub.h
after adding names - run it after every code change that adds upper-case names), `tools/sim/` (host tests: `sh tools/sim/run.sh`).

## Gotchas learned
- **Scores used to vanish on every web flash**: the merged image (offset 0) padded NVS (0x9000-0xDFFF) with 0xFF. Now `build.py`
  emits `bootloader-/partitions-<variant>.bin`, `boot_app0.bin` and `app-<variant>.bin` and the manifest flashes them at
  0x1000 / 0x8000 / 0xE000 / 0x10000, skipping NVS. OTA never touched NVS. No merged image is produced any more.
- `Preferences::getBytes` returns 0 if the buffer is smaller than the stored blob, 8-byte records are read with a 9-byte buffer on purpose.
- System headers define short upper-case macros (BR, LBEG, SAR...). Keep all system includes before the undef block in hub.h.
- TFT_eSPI must NOT own the backlight pin (no `TFT_BL` flag); `backlightBegin()` runs after `tft.init()`, otherwise brightness does nothing.
- The ST7789 build writes MADCTL itself (0x60 landscape / 0xA0 flipped, +0x08 for BGR) for the Red/Blue setting.
- Version string comes from the workflow: `--version "2.0.<run_number>" --build <run_number>`.

## Status / unverified
- Host tests pass (fuzz of every game at every difficulty, Wi-Fi/update UI, clock incl. weather parsing/refresh/rendering;
  the sim HTTP stub serves a realistic Open-Meteo body, `g_wxBody` overrides it).
- Unverified on real hardware / network (the build sandbox cannot reach api.open-meteo.com): the TLS handshake to Open-Meteo
  against the core's root bundle, the real response (parser follows the documented layout: `current{temperature_2m,weather_code,is_day}`,
  `daily{temperature_2m_max[],temperature_2m_min[]}`), the split-image web flash (first flash after this change: check scores
  survive a second flash), and the look of the weather row/side panels (icons were checked as rendered pixels, text was not).
- Compile status: Nothing has been checked on the
  ESP32 compile for the Wi-Fi/OTA/clock code except through the user's successful CI builds - if a CI build fails, ask for the
  first `error:` lines.
- Colours and brightness on the 2-USB board were fixed on the real board by the user's testing (Invert + BGR); brightness fix is untested.
- Ideas the user was offered: auto-check for updates at boot, phone control panel,
  ESP-NOW two-player games, online leaderboard.
