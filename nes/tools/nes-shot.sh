#!/bin/sh
#
# Headless screen capture of build/lobby.nes in MAME with the FujiNet NES
# cartridge (fn-nes pico/nes/emu, grafted into a MAME tree by its apply.sh),
# against a live fujinet-pc. Adapted from fujinet-google-calendar's
# tools/nes-shot.sh: the pad is pressed from a Lua script on a schedule, and
# the screen comes back as text (from the name table) as well as a PNG.
#
#   tools/nes-shot.sh                      the screen at 8 s
#   tools/nes-shot.sh "9:Down 10:Down"     press Down at 9 s and 10 s
#   AT=20 tools/nes-shot.sh "12:A"         join, capture at 20 s
#
# fujinet-pc must be an RS232 build listening for BoIP on FUJINET_TCP
# (default 127.0.0.1:9995), and only one MAME may hold that link at a time.
# Build with LOBBY_URL= to point at a local lobby server.
#
# The PNG lands in build/snap/nes/ (MAME numbers them).
set -e

MAME="${MAME:-$HOME/Workspace/mame}"
AT="${AT:-8}"
HERE=$(cd "$(dirname "$0")/.." && pwd)

[ -f "$HERE/build/lobby.nes" ] || make -C "$HERE" >/dev/null
mkdir -p "$HERE/build/snap"

# MAME must run from its own tree or -autoboot_script is silently ignored.
cd "$MAME"
SHOT_PRESS="$1" SHOT_AT="$AT" FUJINET_TCP="${FUJINET_TCP:-127.0.0.1:9995}" \
    ./mame nes -nes_slot fujinet -cart "$HERE/build/lobby.nes" \
    -snapshot_directory "$HERE/build/snap" \
    -autoboot_script "$HERE/tools/nes-shot.lua" \
    -video none -sound none -nothrottle -seconds_to_run $(( ${AT%.*} + 30 )) 2>&1 \
    | grep -v '^fujinet:'
ls -t "$HERE"/build/snap/nes/*.png 2>/dev/null | head -1
