#!/usr/bin/env python3
"""
Build the CYD Game Hub firmware (two display variants) and put the flashable images where the
web flasher expects them:

    web/firmware/cyd-gamehub-ili9341.bin   (merged image, flash at offset 0 - used by the web flasher)
    web/firmware/cyd-gamehub-st7789.bin
    web/manifest-ili9341.json / web/manifest-st7789.json
    web/firmware/app-ili9341.bin           (plain application image - used by over-the-air updates)
    web/firmware/app-st7789.bin
    web/ota-ili9341.json / web/ota-st7789.json   (version, build number, size, md5 of the app image)

Needs only `arduino-cli` on PATH (https://arduino.github.io/arduino-cli/ - on Windows: `winget install ArduinoSA.CLI`).
It installs the ESP32 core + libraries itself. Usage:   python tools/build.py [--version 1.0.7 --build 7] [--only ili9341]
Over-the-air updates need the address the files will be published at: it is derived automatically on GitHub Actions
(https://<owner>.github.io/<repo>/) or can be given with --base-url.
"""
import argparse, glob, hashlib, json, os, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH = os.path.join(ROOT, "firmware", "CydGameHub")
WEB = os.path.join(ROOT, "web")
OUT = os.path.join(ROOT, "build")

ESP32_INDEX = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
CORE = "esp32:esp32"
CORE_VERSION = "2.0.17"
LIBS = ["TFT_eSPI@2.5.43", "XPT2046_Touchscreen@1.4"]
# min_spiffs = two 1.9 MB application slots (needed for over-the-air updates) + 190 KB storage. NVS (scores, settings) keeps its place.
APP_SLOT_MAX = 0x1E0000
FQBN = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40,FlashSize=4M,UploadSpeed=921600"

# TFT_eSPI is configured purely through compiler flags (no User_Setup.h edit needed).
COMMON = {
    "USER_SETUP_LOADED": "1",
    "TFT_WIDTH": "240", "TFT_HEIGHT": "320",
    "TFT_MISO": "12", "TFT_MOSI": "13", "TFT_SCLK": "14", "TFT_CS": "15", "TFT_DC": "2", "TFT_RST": "-1",
    "TFT_BL": "21", "TFT_BACKLIGHT_ON": "HIGH",
    "USE_HSPI_PORT": "1",
    "LOAD_GLCD": "1", "LOAD_FONT2": "1", "LOAD_FONT4": "1",
    "SPI_FREQUENCY": "40000000", "SPI_READ_FREQUENCY": "20000000", "SPI_TOUCH_FREQUENCY": "2500000",
}
VARIANTS = {
    # standard single-USB CYD (ESP32-2432S028R, ILI9341)
    "ili9341": {"label": "Standard CYD (ILI9341)", "defs": {"ILI9341_2_DRIVER": "1"}},
    # CYD with USB-C + micro-USB ("2-USB" revision, ST7789)
    "st7789": {"label": "2-USB CYD (ST7789)", "defs": {"ST7789_DRIVER": "1"}},
}


def run(cmd, **kw):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def find_cli():
    cli = shutil.which("arduino-cli")
    if not cli:
        sys.exit("arduino-cli not found on PATH. Install it from https://arduino.github.io/arduino-cli/ "
                 "(Windows: winget install ArduinoSA.CLI) and run this again.")
    return cli


def default_base_url():
    """On GitHub Actions: the GitHub Pages address of this repository."""
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    if "/" not in repo:
        return ""
    owner, name = repo.split("/", 1)
    if name.lower() == owner.lower() + ".github.io":
        return "https://%s.github.io/" % owner.lower()
    return "https://%s.github.io/%s/" % (owner.lower(), name)


def write_version_h(version, build, variant, base_url):
    with open(os.path.join(SKETCH, "version.h"), "w") as f:
        f.write('#pragma once\n'
                '// Rewritten by tools/build.py for every build (the #ifndef guards only let the host test harness override values).\n'
                '#ifndef HUB_VERSION\n#define HUB_VERSION "%s"\n#endif\n'
                '#ifndef HUB_BUILD\n#define HUB_BUILD %d\n#endif\n'
                '#ifndef HUB_VARIANT\n#define HUB_VARIANT "%s"\n#endif\n'
                '#ifndef UPDATE_BASE_URL\n#define UPDATE_BASE_URL "%s"\n#endif\n' % (version, build, variant, base_url))


def flags(defs):
    d = dict(COMMON)
    d.update(defs)
    return " ".join("-D%s=%s" % (k, v) for k, v in d.items())


def merge_fallback(build_dir, out_bin):
    """Used only if arduino-cli did not produce *.merged.bin: merge the pieces with esptool."""
    def one(pattern):
        m = glob.glob(os.path.join(build_dir, pattern))
        if not m:
            sys.exit("missing build output: " + pattern)
        return m[0]
    boot = one("*.ino.bootloader.bin")
    part = one("*.ino.partitions.bin")
    app = one("*.ino.bin")
    home = os.path.expanduser("~")
    cands = glob.glob(os.path.join(home, ".arduino15", "packages", "esp32", "hardware", "esp32", "*", "tools", "partitions", "boot_app0.bin"))
    cands += glob.glob(os.path.join(os.environ.get("LOCALAPPDATA", "/nonexistent"), "Arduino15", "packages", "esp32", "hardware", "esp32", "*", "tools", "partitions", "boot_app0.bin"))
    if not cands:
        sys.exit("boot_app0.bin not found")
    run([sys.executable, "-m", "esptool", "--chip", "esp32", "merge_bin", "-o", out_bin,
         "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
         "0x1000", boot, "0x8000", part, "0xe000", cands[0], "0x10000", app])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="1.0")
    ap.add_argument("--build", type=int, default=0, help="build number; an update is offered when the published one is higher")
    ap.add_argument("--base-url", default=None, help="where the web/ folder is published, e.g. https://you.github.io/cyd-game-hub/")
    ap.add_argument("--only", choices=list(VARIANTS), help="build just one variant")
    ap.add_argument("--skip-install", action="store_true", help="do not (re)install core/libraries")
    ap.add_argument("--flash", action="store_true", help="after building, flash the board over USB with esptool (needs --only)")
    ap.add_argument("--port", help="serial port for --flash, e.g. COM5 (default: auto-detect)")
    a = ap.parse_args()

    if a.flash and not a.only:
        sys.exit("--flash needs --only ili9341|st7789")
    cli = find_cli()
    if not a.skip_install:
        # the ESP32 core is not in Arduino's default index, so its index URL must be passed explicitly
        run([cli, "core", "update-index", "--additional-urls", ESP32_INDEX])
        run([cli, "core", "install", "%s@%s" % (CORE, CORE_VERSION), "--additional-urls", ESP32_INDEX])
        run([cli, "lib", "install"] + LIBS)

    os.makedirs(os.path.join(WEB, "firmware"), exist_ok=True)
    base_url = a.base_url if a.base_url is not None else default_base_url()
    if base_url and not base_url.endswith("/"):
        base_url += "/"
    print("version %s, build %d, update address: %s" % (a.version, a.build, base_url or "(none - wireless updates disabled in this build)"))
    try:
        build_all(a, cli, base_url)
    finally:
        write_version_h("1.0", 0, "ili9341", "")      # leave the working tree with neutral defaults


def build_all(a, cli, base_url):
    for name, v in VARIANTS.items():
        if a.only and a.only != name:
            continue
        write_version_h(a.version, a.build, name, base_url)
        bdir = os.path.join(OUT, name)
        shutil.rmtree(bdir, ignore_errors=True)
        extra = flags(v["defs"])
        # Windows/Linux alike: the whole thing is ONE argv element, so no shell quoting is involved.
        run([cli, "compile", "--fqbn", FQBN, "--additional-urls", ESP32_INDEX, "--output-dir", bdir, "--warnings", "none",
             "--build-property", "compiler.cpp.extra_flags=" + extra, SKETCH])
        merged = glob.glob(os.path.join(bdir, "*.merged.bin"))
        dest = os.path.join(WEB, "firmware", "cyd-gamehub-%s.bin" % name)
        if merged:
            shutil.copyfile(merged[0], dest)
        else:
            merge_fallback(bdir, dest)
        size = os.path.getsize(dest)
        if size < 200_000:
            sys.exit("suspiciously small image (%d bytes): %s" % (size, dest))
        # plain application image for over-the-air updates
        apps = [f for f in glob.glob(os.path.join(bdir, "*.ino.bin"))]
        if not apps:
            sys.exit("application image (*.ino.bin) not found in " + bdir)
        app_dest = os.path.join(WEB, "firmware", "app-%s.bin" % name)
        shutil.copyfile(apps[0], app_dest)
        app_size = os.path.getsize(app_dest)
        if app_size > APP_SLOT_MAX:
            sys.exit("application image is %d bytes, larger than the %d byte update slot" % (app_size, APP_SLOT_MAX))
        with open(app_dest, "rb") as fh:
            md5 = hashlib.md5(fh.read()).hexdigest()
        with open(os.path.join(WEB, "ota-%s.json" % name), "w") as f:
            json.dump({"version": a.version, "build": a.build, "size": app_size, "md5": md5,
                       "path": "firmware/app-%s.bin" % name}, f, indent=2)
        print("    app image %d bytes (%.0f%% of the %d byte slot)" % (app_size, 100.0 * app_size / APP_SLOT_MAX, APP_SLOT_MAX))
        manifest = {
            "name": "CYD Game Hub - " + v["label"],
            "version": a.version,
            "new_install_prompt_erase": False,
            "builds": [{"chipFamily": "ESP32",
                        "parts": [{"path": "firmware/cyd-gamehub-%s.bin" % name, "offset": 0}]}],
        }
        with open(os.path.join(WEB, "manifest-%s.json" % name), "w") as f:
            json.dump(manifest, f, indent=2)
        print("OK  %-8s %7d bytes -> %s" % (name, size, dest))
    if a.flash:
        img = os.path.join(WEB, "firmware", "cyd-gamehub-%s.bin" % a.only)
        cmd = [sys.executable, "-m", "esptool", "--chip", "esp32", "--baud", "460800"]
        if a.port:
            cmd += ["--port", a.port]
        run(cmd + ["write_flash", "0x0", img])
        print("\nFlashed. The board restarts by itself; touch the three crosshairs to calibrate.")
        return
    print("\nDone. Serve the flasher locally with:  python tools/serve.py")


if __name__ == "__main__":
    main()
