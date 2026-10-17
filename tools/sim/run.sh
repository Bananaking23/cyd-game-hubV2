#!/bin/sh
# Compile the whole sketch for the host with AddressSanitizer + UBSan and fuzz every game with scripted touches,
# then run the scripted Wi-Fi / keyboard / OTA UI test and the clock screensaver test.
cd "$(dirname "$0")/../.." || exit 1
FLAGS="-std=gnu++11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wno-unused-function -Wno-unused-variable -Itools/sim -Ifirmware/CydGameHub -include Arduino.h"
g++ $FLAGS tools/sim/sim_main.cpp tools/sim/sim_bundle.cpp -o /tmp/cydsim || exit 1
timeout 900 /tmp/cydsim "${1:-4000}" || exit 1
g++ $FLAGS -DUPDATE_BASE_URL='"https://example.invalid/cyd-game-hub/"' tools/sim/net_test.cpp tools/sim/sim_bundle.cpp -o /tmp/cydnet || exit 1
timeout 300 /tmp/cydnet
g++ $FLAGS -DHUB_VARIANT='"st7789"' tools/sim/clock_test.cpp tools/sim/sim_bundle.cpp -o /tmp/cydclock || exit 1
timeout 300 /tmp/cydclock
g++ $FLAGS tools/sim/games_test.cpp tools/sim/sim_bundle.cpp -o /tmp/cydgames || exit 1
timeout 300 /tmp/cydgames
