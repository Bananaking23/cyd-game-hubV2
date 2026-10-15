# CYD Game Hub - handoff note

Paste this into a new chat together with `cyd-game-hub-v8.zip` (the whole project).

## What it is
Firmware + browser web flasher for an ESP32-2432S028R "Cheap Yellow Display" (2.8", 320x240, resistive touch).
Built with arduino-cli (esp32 core 2.0.17, TFT_eSPI 2.5.43, XPT2046_Touchscreen 1.4). Published by the user through
GitHub Desktop -> GitHub Pages via the Actions workflow in `.github/workflows/build.yml`. The user has the
**2-USB board (USB-C + micro-USB, ST7789)** and flashes the `st7789` variant.

## Current features (v8, version 2.0.x)
- 19 games (Snake, Flap, Breakout, Pong, Invaders, Asteroids, Tetris, 2048, Simon, Whack, Runner, Tic-Tac-Toe, Connect 4,
  Reversi, Memory, Minesweeper, Lights Out, Sudoku, Slide). Easy/Normal/Hard with separate high scores per difficulty.
- Settings is a 4x3 tile grid: Sound, Brightness, Difficulty, Rotation, Invert, Calibrate, Scores (asks first),
  Wi-Fi, Clock, Clock style, Red/Blue (ST7789 only), Theme (light/dark).
- Wi-Fi (on-screen keyboard, network picker, password stored unencrypted in NVS) and wireless OTA updates from the
  GitHub Pages site (`ota-<variant>.json` + `firmware/app-<variant>.bin`, size + MD5 checked, TLS via the core's cert bundle).
  Needs the `min_spiffs` partition scheme, so older boards needed one USB reflash.
- Clock screensaver: digital (seven-segment) or analog, 12/24 h, seconds, date formats, colour, own brightness,
  17 time zones with DST, auto-start after idle on the menu. Time via NTP over the saved Wi-Fi (no battery-backed RTC).
- Light/dark theme applies to menu, settings, Wi-Fi screens and clock only; games and tiles stay dark.
- ST7789 defaults on a fresh install: Invert on, BGR order (this is what the user's panel needed).

## Files that matter
`firmware/CydGameHub/`: `CydGameHub.ino` (menu, settings, loop), `hub.h` (touch, HUD, sound, scores, theme, settings),
`net.h` (Wi-Fi, keyboard, OTA), `clock.h` (screensaver), `games.h` + `game_*.h` (one namespace per game), `version.h`.
`tools/build.py` (builds both variants, writes `web/`), `tools/gen_undef.py` (regenerates the `#undef` safety net in hub.h
after adding names - run it after every code change that adds upper-case names), `tools/sim/` (host tests: `sh tools/sim/run.sh`).

## Gotchas learned
- System headers define short upper-case macros (BR, LBEG, SAR...). Keep all system includes before the undef block in hub.h.
- TFT_eSPI must NOT own the backlight pin (no `TFT_BL` flag); `backlightBegin()` runs after `tft.init()`, otherwise brightness does nothing.
- The ST7789 build writes MADCTL itself (0x60 landscape / 0xA0 flipped, +0x08 for BGR) for the Red/Blue setting.
- Version string comes from the workflow: `--version "2.0.<run_number>" --build <run_number>`.

## Status / unverified
- Host tests pass (fuzz of every game at every difficulty, Wi-Fi/update UI, clock). Nothing has been checked on the
  ESP32 compile for the Wi-Fi/OTA/clock code except through the user's successful CI builds - if a CI build fails, ask for the
  first `error:` lines.
- Colours and brightness on the 2-USB board were fixed on the real board by the user's testing (Invert + BGR); brightness fix is untested.
- Ideas the user was offered: weather on the clock (Open-Meteo), auto-check for updates at boot, phone control panel,
  ESP-NOW two-player games, online leaderboard.
