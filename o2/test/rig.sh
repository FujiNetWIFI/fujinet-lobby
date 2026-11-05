#!/usr/bin/env bash
# rig.sh -- a private FujiNet and lobby for a live run, then a scenario.
#
#   test/rig.sh SCENARIO OUTDIR [SECS [IMAGE]]
#
# A private copy of fujinet-pc-rs232 (FNPC_DIST; FNPC_BIN another build's
# binary) on BoIP port BOIP (19979), its SD from test/mksd.sh (NAME: key 0)
# and its hosts from HOSTS (comma-separated: "SD", or a TNFS host; default
# SD alone). A private lobby server (../server, a fresh database) behind
# test/lobbyproxy.py on LOBBY_PORT (8089), its rooms from LOBBY_SEED (a JSON
# list of rooms; "none": no lobby at all); LOBBY_CHANGE="N FILE [METHOD]"
# changes them before the Nth GET. Then tools/o2sim.py on SCENARIO against
# it, with IMAGE (build/lobby.bin) as the cart's image. Kills only what it
# started; never the everyday fujinet-pc on 9995. HOLD=1 leaves it up after
# the scenario, until ^C.
set -euo pipefail
cd "$(dirname "$0")/.."
HERE=$(pwd)
SCEN=$1
OUT=$2
SECS=${3:-120}
IMAGE=${4:-build/lobby.bin}
BOIP=${BOIP:-19979}
LOBBY_PORT=${LOBBY_PORT:-8089}
LOBBY_SEED=${LOBBY_SEED:-none}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
FNPC_BIN=${FNPC_BIN:-$FNPC_DIST/fujinet}
RIG="${RIG:-$HERE/build/rig}"
HOSTS=${HOSTS:-SD}

pids=()
cleanup() {
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
}
trap cleanup EXIT

for port in "$BOIP" "$((BOIP - 10000))" "$LOBBY_PORT" "$((LOBBY_PORT + 1))"; do
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        ss -ltn | grep -q ":$port " || break
        sleep 0.5
    done
    if ss -ltn | grep -q ":$port "; then echo "rig: port $port is taken" >&2; exit 1; fi
done

rm -rf "$RIG/fn"; mkdir -p "$RIG/fn"
# the binary by link, the rest copied: its config is ours to change
ln -s "$FNPC_BIN" "$RIG/fn/fujinet"
cp -a "$FNPC_DIST/data" "$RIG/fn"/
test/mksd.sh "$RIG/fn"
python3 - "$FNPC_DIST/fnconfig.ini" "$RIG/fn/fnconfig.ini" "$BOIP" "$HOSTS" <<'PY'
import re, sys
src, dst, boip, hosts = sys.argv[1:5]
s = open(src).read()
# no tokens, no mounts, no other machine's hosts in a test's config
s = re.sub(r"\[(GoogleDrive|OneDrive|S3|Mount\d+|Host\d+)\][^\[]*", "", s)
s = re.sub(r"(\[BOIP\][^\[]*?\bport=)\d*", r"\g<1>" + boip, s, flags=re.S)
s = re.sub(r"(\[BOIP\][^\[]*?\benabled=)\d*", r"\g<1>1", s, flags=re.S)
names = hosts.split(",")
for i in range(8):
    n = names[i] if i < len(names) else ""
    if n:
        s += "[Host%d]\ntype=%s\nname=%s\n\n" % (i + 1, "SD" if n == "SD" else "TNFS", n)
open(dst, "w").write(s)
PY
( cd "$RIG/fn" && exec ./fujinet -u http://127.0.0.1:$((BOIP - 10000)) -c fnconfig.ini -s SD ) \
    > "$RIG/fn.log" 2>&1 &
pids+=($!)
for _ in 1 2 3 4 5 6 7 8 9 10; do
    ss -ltn | grep -q ":$BOIP " && break
    sleep 0.5
done
ss -ltn | grep -q ":$BOIP " || { echo "rig: no BoIP listener on $BOIP" >&2; exit 1; }

if [ "$LOBBY_SEED" != none ]; then
    SRV="$HERE/build/lobbysrv/lobbyPersist"
    if [ ! -x "$SRV" ] || [ -n "$(find "$HERE/../server" -name '*.go' -newer "$SRV")" ]; then
        mkdir -p "$HERE/build/lobbysrv"
        ( cd "$HERE/../server" && go build -o "$SRV" . )
    fi
    rm -rf "$RIG/lobby"; mkdir -p "$RIG/lobby/db"
    sqlite3 -bail "$RIG/lobby/db/lobby.sqlite3" < "$HERE/../server/lobby_schema.sql"
    ( cd "$RIG/lobby" && exec "$SRV" -srvaddr "127.0.0.1:$((LOBBY_PORT + 1))" ) \
        > "$RIG/lobby.log" 2>&1 &
    pids+=($!)
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        ss -ltn | grep -q ":$((LOBBY_PORT + 1)) " && break
        sleep 0.5
    done
    python3 "$HERE/test/seed.py" "$LOBBY_SEED" "http://127.0.0.1:$((LOBBY_PORT + 1))/server"
    # shellcheck disable=SC2086
    python3 "$HERE/test/lobbyproxy.py" "$LOBBY_PORT" "http://127.0.0.1:$((LOBBY_PORT + 1))" \
        ${LOBBY_CHANGE:-} > "$RIG/proxy.log" 2>&1 &
    pids+=($!)
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        ss -ltn | grep -q ":$LOBBY_PORT " && break
        sleep 0.5
    done
fi

O2SIM_FUJINET="127.0.0.1:$BOIP" O2SIM_SECONDS=$SECS \
    python3 tools/o2sim.py "$SCEN" "$OUT" "$IMAGE"
if [ -n "${HOLD:-}" ]; then
    echo "rig: up (BoIP $BOIP); ^C ends it"
    wait
fi
