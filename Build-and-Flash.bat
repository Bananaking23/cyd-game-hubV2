@echo off
setlocal
cd /d "%~dp0"
title CYD Game Hub - build and flash

where python >nul 2>nul
if errorlevel 1 (
  echo Python 3 is needed. Install it from https://www.python.org/downloads/ ^(tick "Add python.exe to PATH"^), then run this again.
  pause & exit /b 1
)
where arduino-cli >nul 2>nul
if errorlevel 1 (
  echo Installing arduino-cli...
  winget install -e --id ArduinoSA.CLI --accept-source-agreements --accept-package-agreements
  echo.
  echo Done. Please CLOSE this window and double-click the file again so Windows picks up the new tool.
  pause & exit /b 0
)

echo.
echo Which board do you have?
echo   1 = standard CYD (one micro-USB port)
echo   2 = newer CYD with USB-C + micro-USB
set /p CHOICE=Type 1 or 2 and press Enter: 
set VARIANT=ili9341
if "%CHOICE%"=="2" set VARIANT=st7789

echo.
echo Plug the board in with a data USB cable now. Building takes a few minutes the first time.
python -m pip install --quiet esptool
python tools\build.py --only %VARIANT% --flash
if errorlevel 1 (
  echo.
  echo Something failed - see the messages above. If it could not connect, hold the BOOT button on the board while it says Connecting.
  pause & exit /b 1
)
echo.
echo All done. Touch the three crosshairs on the board to calibrate.
pause
