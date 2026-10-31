# FujiNet game lobby: Odyssey 2 / Videopac

The lobby for the Magnavox Odyssey 2 and Philips Videopac with the FujiNet
cart. It lists the rooms the lobby server has for the `o2` platform, then
joins one: the game's host goes in a host slot, the room's server URL goes in
the game's appkey, and the game is mounted and booted. CONFIG's GAME LOBBY
key (L on the hosts screen) boots it from `tnfs://ec.tnfs.io/o2/lobby.bin`.

It is 8048 assembly, a protocol 2 (APPBANK) image: a fixed 1K plus 10 pages
of 2K, 21K in all. The display engine, the editor, the help, the boot's
progress bar and the tools are a snapshot of fujinet-config's `o2/` (main
347436d). Fix platform-layer bugs there first.

## Look

```
 LOBBY PLAYER1  1      T0: the name (key 0), the page
 5 CARD STUD           a game (tan), when it isn't the row above's
 BASEMENT 2/8          its rooms: name, players (grey when offline)
 THE DEN  1/8          the bar: the room picked; a long name scrolls
 BATTLESHIP
 HARBOR   1/2

 FIRE JOIN ? HELP
```

A page shows 4 records and asks for 5: the 5th says a next page exists. A
record takes 2 rows at most, so the 8 rows always hold a page.

## Controls

| Key | Does |
|---|---|
| Stick up/down | room; past a page's end, the next or previous page |
| Stick left/right | page |
| Button, ENT | join |
| R | reload from the first page |
| N | edit the name (2-8 of A-Z, 0-9) |
| ?, H | help |

The first run without a name asks for one. Escaping carries on unnamed.

## The join

1. While the list's reply is still in, the room's type, its place, a checksum of its server URL, and its client URL's host and path are saved. Only `tnfs://` client URLs are taken.
2. The host goes in the slot that already has it, else the first empty slot, else slot 8. Then MOUNT_HOST, SET_HOST_PREFIX "/", and SET_DEVICE_FULLPATH for slot 0.
3. The room is fetched again (`pagesize=1&offset=n`), because the list is sorted by players and can move. If the checksum doesn't match, the 5 rooms around it are searched; if it isn't there, ROOM GONE.
4. The server URL is written to appkey creator 1, app 1, key = the game type. This is best effort.
5. MOUNT_IMAGE with CONFIG's progress bar, then the soft reset into the game.

Each failure is named, and a key goes back to the list on the same room.

## Build

```
make                    # build/lobby.bin
ENDPOINT=http://127.0.0.1:8089/view ./build.sh
PLATFORM=intv OUT=build/real ./build.sh      # another platform's rooms
make run                # MAME window, FUJINET_TCP (127.0.0.1:9995)
```

Needs Macroassembler AS (`~/asl/asl`, `p2bin`) and python3. `build.sh` runs
CONFIG's checks on every page:

- `checkflags`
- `checkobj`
- `checklayout`
- `checkmb`
- `checkpage`
- `checkstack` (main 6 + interrupts 2 of 8)
- the fixed-half compare

## Testing

`make test` runs `test/t/*.scen` on a private rig:

- a private fujinet-pc on BoIP 19979;
- `../server` built and run on a fresh database behind `test/lobbyproxy.py` (port 8089);
- rooms from `test/seed`;
- MAME headless.

`tools/cfgtest.py` checks the screen's text and the SD's appkeys. The proxy
can move or delete a room between two GETs (`LOBBY_CHANGE`). 5 Card Stud is
built pointing at a dead server, so nothing touches a live game server.

Covered:

- the first-run name;
- the list, paging both ways, and the marquee;
- help, renaming and reloading;
- joins that boot K.C. Munchkin (a classic image) and 5 Card Stud (APPBANK);
- a moved room, and a gone room;
- BAD GAME URL, HOST ERROR, NOT A GAME and TOO BIG;
- no rooms, and no server (NETWORK ERROR CF).

`make rows` checks frame stability on NTSC and PAL. This has been verified
in MAME only, never on hardware.

## Deployment

Copy `build/lobby.bin` to `ec.tnfs.io:/o2/lobby.bin`. Games list rooms for
the O2 by registering a client `{"platform": "o2", "url": "tnfs://..."}`.

## Known limits

- **A pushed lobby can't boot a game yet.** The cart refuses any push while
  a pushed APPBANK image runs, because that image is served from the stage
  buffer (fujinet-firmware `pico/o2/firmware/src/fujimail.c`,
  BOOT_ERR_BUSY: "POWER OFF+ON"). Started from CONFIG, the lobby lists rooms
  but can't join one until the firmware takes a push into the stage's free
  space. MAME's `-cart` image is exempt, so the rig joins.
- **PAL:** the last line of a row's characters in columns 10-11 can flicker,
  when the next row's band starts. This is the engine's, as on CONFIG's
  editor.
- Only M.U.L.E. registers an `o2` client today (QA lobby). The 5 Card Stud,
  Battleship and Hold'em O2 builds hard-code their server and don't read
  the room's appkey.
