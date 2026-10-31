#!/usr/bin/env bash
# mksd.sh -- the rig's SD card: DIR/SD, for test/rig.sh's private fujinet-pc.
#
#   test/mksd.sh DIR
#
# The games the seeded rooms boot: K.C. Munchkin from ~/tnfs (a classic
# image, no network), 5 Card Stud (an APPBANK image: CARD5, by default the
# Makefile's build pointed at a dead server, never the live one). NAME: key
# 0 (the player's name) as it is, else none (the first run).
set -euo pipefail
D=$1/SD
rm -rf "$D"
mkdir -p "$D/FujiNet" "$D/games" "$D/apps"
put() { [ -f "$1" ] && cp "$1" "$2" || echo "mksd: no $1 (skipped)" >&2; }
put "$HOME/tnfs/K.C. Munchkin (USA).bin" "$D/games/kc.bin"
put "${CARD5:-$(dirname "$0")/../build/5card/5card.bin}" "$D/apps/5card.bin"
head -c 70000 /dev/zero > "$D/games/big.bin"
printf 'not a cartridge\n' > "$D/games/readme.txt"
if [ -n "${NAME:-}" ]; then printf '%s' "$NAME" > "$D/FujiNet/00010100.key"; fi
