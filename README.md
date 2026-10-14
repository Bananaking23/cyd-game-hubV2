# CYD Game Hub

19 touch games for the **2.8" resistive-touch Cheap Yellow Display** (ESP32-2432S028R), with a one-click
**browser flasher** (Chrome / Edge, Web Serial, no drivers or IDE needed beyond the USB-serial driver).

Snake, Flap, Breakout, Pong, Invaders, Asteroids, Tetris, 2048, Simon, Whack-a-mole, Runner, Tic-Tac-Toe, Connect 4,
Reversi, Memory, Minesweeper, Lights Out, Sudoku, Slide puzzle. Every game has **Easy / Normal / Hard** (tap the
difficulty tag in the menu header, or Settings), with separate high scores per difficulty. High scores and settings
are saved on the board.

## Easiest on Windows

Double-click **`Build-and-Flash.bat`**: it installs arduino-cli if needed, asks which board you have, builds, and flashes
the board over USB. (If it just installed arduino-cli, close the window and run it again.)

## Get a flashable site (pick one)

The firmware has to be compiled once; this repo does that for you two ways.

### A. Locally (Windows / macOS / Linux)

1. Install [arduino-cli](https://arduino.github.io/arduino-cli/) (Windows: `winget install ArduinoSA.CLI`) and Python 3.
2. `pip install esptool` (only used as a fallback when arduino-cli doesn't emit a merged image)
3. `python tools/build.py`  - installs the ESP32 core 2.0.17 + libraries, builds both display variants,
   writes `web/firmware/*.bin` and `web/manifest-*.json`.
4. `python tools/serve.py`  - opens http://localhost:8000. Pick your board, click flash.

### B. On GitHub (gives you a public flasher URL)

Push this folder to a GitHub repo, then **Settings -> Pages -> Source: GitHub Actions**. The included workflow
(`.github/workflows/build.yml`) compiles both variants and publishes `web/`. Share the Pages URL; anyone can flash.

## Wireless updates

On the board: **Settings -> Wi-Fi & updates**. Pick a network (on-screen keyboard), then **Check for updates**. The board
fetches `ota-<variant>.json` and `firmware/app-<variant>.bin` from the published Pages site over HTTPS (certificates
checked against the ESP32 root bundle, firmware size + MD5 verified), installs it and restarts. Each GitHub Actions run
publishes a new build number, so every push becomes an available update. A local build (`tools/build.py`) has no update
address unless you pass `--base-url`.

* Updates need the two-slot partition layout (`min_spiffs`). A board flashed with an older version of this project must be
  flashed **once more over USB / the web flasher**; settings survive, high scores are reset once (scores are now per difficulty).
* The Wi-Fi password is stored unencrypted in the board's NVS flash. Don't reuse an important password.

## Clock screensaver

**Settings -> Clock** shows a full-screen clock; touch anywhere to leave. **Settings -> Clock style** customises it:
digital (seven-segment) or analog face, 12/24 hour, seconds on/off, date (off, `Wed 14 Oct`, `14/10/2026`, `10/14/2026`,
`2026-10-14`, or the full `Wednesday 14 October`), colour, its own brightness, time zone (daylight saving is automatic) and
**Auto-start** (starts by itself after 1 / 5 / 10 / 30 minutes without touching the menu). PREVIEW in the set-up screen
shows the result. In the set-up list, tapping a row's name steps backwards, tapping its button steps forwards.

The time comes from the internet (NTP) over the saved Wi-Fi, so set up Wi-Fi first (Settings -> Wi-Fi & updates). The board
keeps time by itself afterwards; it needs Wi-Fi once after each power-up (done automatically when you press START) and
re-syncs every few days. There is no battery-backed clock, so an unplugged board forgets the time. Auto-start only runs
once the time is known.

## Which board variant?

| Variant | Boards | Display driver |
|---|---|---|
| `ili9341` | the common single micro-USB CYD | `ILI9341_2_DRIVER` |
| `st7789`  | newer revision with USB-C + micro-USB | `ST7789_DRIVER` |

If you see a white / garbled screen, flash the other variant. Wrong colours: **Settings -> Invert** (photo-negative look) and, on the 2-USB build, **Settings -> Red/Blue** (RGB/BGR order; try the four combinations).
TFT_eSPI is configured entirely through build flags in `tools/build.py` (no `User_Setup.h` editing).

## Using it

* Tap a tile to play; swipe or `<` `>` for more pages; **MENU** (top right) leaves a game.
* **Settings** is a grid of tiles like the game menu (tap a tile to change it, MENU to go back): Clock (START), Clock style (SET UP), Wi-Fi & updates, difficulty, sound, brightness, screen rotation (separate touch calibration per orientation), invert colours,
  touch calibration, clear high scores (asks first). Hold **BOOT** while powering up to force touch calibration.
* First boot asks you to touch three crosshairs (a stylus is more accurate on a resistive panel).

## Layout

```
firmware/CydGameHub/   CydGameHub.ino (menu, settings, main loop), hub.h (touch, HUD, sound, scores),
                       net.h (Wi-Fi, keyboard, updates), clock.h (screensaver clock),
                       games.h (registry), game_*.h (one namespace per game)
web/                   index.html + manifests; firmware/*.bin appear after a build
tools/build.py         build both variants -> web/
tools/serve.py         local https-less server for Web Serial (localhost)
tools/check_syntax.sh  quick host syntax check against stub headers
tools/sim/             host fuzz test: runs every game under ASan/UBSan with scripted touches (tools/sim/run.sh)
```

## Adding a game

Write `game_x.h` with `namespace X { static void init(); static void tick(); static void icon(int cx, int cy); }`
(see the rules at the top of `hub.h` and `game_snake.h` as a template), `#include` it in `games.h`, add one row to
`GAMES[]`. Menu pages and the score table grow automatically.

## Status

Developed without access to the ESP32 toolchain or a physical board: the code is syntax-checked and fuzz-tested on a
PC (every game at every difficulty, scripted touch scenarios, AddressSanitizer + UBSan, no findings; the Wi-Fi/update UI is
exercised against simulated networks and server failures), but the Wi-Fi/OTA code has **not been compiled for the ESP32 or run on real hardware**.
Likely first-run tweaks: display variant / inversion, and touch calibration.
