#!/bin/sh
#
# Headless screen capture of build/lobby.sms in MAME with the FujiNet Master
# System cartridge (fn-sms pico/sms/emu, grafted into a MAME tree by its
# apply.sh), against a live fujinet-pc. The nes branch's tools/nes-shot.sh,
# on the SMS: the pad is pressed from a Lua script on a schedule, and the
# screen comes back as text (from the name table, each row tagged with the
# ink it was drawn in) as well as a PNG.
#
#   tools/sms-shot.sh                      the screen at 12 s
#   tools/sms-shot.sh "13:Down 14:Down"    press Down at 13 s and 14 s
#   AT=25 tools/sms-shot.sh "13:B1"        join, capture at 25 s
#
# Buttons: B1 B2 Up Down Left Right Pause. The splash runs ~5 s and the
# first page needs a couple more, so the list is up by about 10 s.
#
# fujinet-pc must be an RS232 build listening for BoIP on FUJINET_TCP
# (default 127.0.0.1:9995), and only one MAME may hold that link at a time.
# Build with LOBBY_URL= to point at a local lobby server.
#
# The PNG lands in build/snap/ (MAME numbers them).
set -e

MAME="${MAME:-$HOME/Workspace/mame}"
AT="${AT:-12}"
HERE=$(cd "$(dirname "$0")/.." && pwd)

[ -f "$HERE/build/lobby.sms" ] || make -C "$HERE" >/dev/null
mkdir -p "$HERE/build/snap"

# MAME must run from its own tree or -autoboot_script is silently ignored.
# A claimed image of 32K or less given as -cart is served as the resident
# client, in place of the baked CONFIG, with the mailbox live.
cd "$MAME"
SHOT_PRESS="$1" SHOT_AT="$AT" FUJINET_TCP="${FUJINET_TCP:-127.0.0.1:9995}" \
    ./mame "${SYSTEM:-sms1}" -slot fujinet -cart "$HERE/build/lobby.sms" \
    -snapshot_directory "$HERE/build/snap" \
    -nvram_directory "$HERE/build/nvram" -cfg_directory "$HERE/build/cfg" \
    -autoboot_script "$HERE/tools/sms-shot.lua" \
    -video none -sound none -nothrottle -seconds_to_run $(( ${AT%.*} + 30 )) 2>&1 \
    | grep -v '^fujinet:'
ls -t "$HERE"/build/snap/*/*.png 2>/dev/null | head -1
