#!/usr/bin/env bash
# run.sh -- build, install into the pico bring-up tree, and run in MAME
# through that tree's own run.sh (which needs a MAME with emu/apply.sh run
# against it for -cartslot fujinet).
#
#   ./run.sh                              interactive
#   ./run.sh --headless "FUJINET LOBBY"   headless, PASS/FAIL on a string
#   ./run.sh --drive                      headless, drive the controller
#
# Env: PICO_COLECO (default ~/Workspace/fujinet-firmware/pico/coleco), plus
# everything the bring-up's run.sh honors: MAME_DIR, SCREEN_EXPECT, SCREEN_AT,
# SCREEN_RESET_AT, DRIVE_SCRIPT, FUJINET_DEBUG... and this Makefile's
# LOBBY_URL for pointing the build at a local lobby server.
set -euo pipefail
cd "$(dirname "$0")"

PICO_COLECO=${PICO_COLECO:-$HOME/Workspace/fujinet-firmware/pico/coleco}

make
cp build/lobby.rom "$PICO_COLECO/build/lobby.bin"
exec "$PICO_COLECO/run.sh" lobby "$@"
