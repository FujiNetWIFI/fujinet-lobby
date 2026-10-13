#!/usr/bin/env bash
# run.sh -- build, install into the pico bring-up tree, and run in MAME
# through that tree's own run.sh (which needs a MAME with emu/apply.sh run
# against it for -slot fujinet, and a fujinet-pc serving BoIP).
#
#   ./run.sh                interactive
#   ./run.sh shot           headless, that tree's emu/shot.lua (SHOT_AT=...)
#   ./run.sh waitshot       headless, PASS/FAIL on SHOT_TEXT
#
# Env: PICO_SMS (default ~/Workspace/fn-sms/pico/sms), plus everything the
# bring-up's run.sh honors: MAME, SYSTEM, SECS, FUJINET_TCP, FUJINET_DEBUG...
# and this Makefile's LOBBY_URL for pointing the build at a local lobby
# server.
set -euo pipefail
cd "$(dirname "$0")"

PICO_SMS=${PICO_SMS:-$HOME/Workspace/fn-sms/pico/sms}

make PICO_SMS="$PICO_SMS" install
exec "$PICO_SMS/run.sh" lobby "$@"
