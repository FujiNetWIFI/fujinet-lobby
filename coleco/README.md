# FujiNet Game Lobby for the ColecoVision

A standalone C port of the Lobby client, in the shape of `../intv/`: it
mirrors `../clients/src/main.c`'s flow without sharing its code, because the
shared client is conio-based and buffers a whole page of server records
(~4.9KB of statics) on a machine with **1K of RAM**. The platform playbook —
build flags, video, input, on-screen keyboard, and the RAM discipline — comes
from `fujinet-config/coleco/`, whose `st_lobby.c` boots this very image from
`ec.tnfs.io:/coleco/lobby.rom`.

What it does, in intv parity (`?bin=1`): resolve the shared username (appkey
creator=1/app=1/key=0, editable on the on-screen keyboard), show the lobby's
server list grouped by game with keypad/stick paging, and on fire mount the
chosen room's game client over TNFS, write the room's server URL to
appkey[game_type] for the booted game to read, and take the cartridge ROM
swap.

## Layout

| Layer | Files |
|---|---|
| App / state machine | `lobby.c`, `state.h`, `constants.h`, `st_name.c`, `st_list.c`, `st_boot.c` |
| Platform layer (synced copies; canonical home is fujinet-config/coleco) | `fujidisp.c/h`, `fujiin.c/h`, `fujiedit.c/h`, `fujisnd.c/h`, `fujisplash.c/h` + `fujisplash_data.c` |
| Transport shims | `fujiraw.c/h` (streamed Fuji-device transactions, raw appkeys), `netraw.c/h` (network device, reply left in the window) |
| Build guard | `bsscheck.py` |

`netraw.c` is the one genuinely new piece: fujinet-lib's own network path
copies every read into a caller buffer, so the lobby issues the network
transactions raw (the same byte shapes as `../intv/fujinet.bas`) and reads
each 189-byte record **in place** in the cartridge's reply window at `$F800`,
painting it straight to VRAM before the next read repaints the window.
Nothing is cached: paging advances by rows actually drawn, the selection bar
is a VRAM read-back (`disp_row_invert`), and booting re-fetches the chosen
record by server-side offset (`pagesize=1&offset=N`) before making its three
bounded copies (game_type, server URL, client URL).

## RAM

`CRT_ORG_BSS=0x702C`, `REGISTER_SP=0x73B8`: 908 bytes for statics plus stack.
This build's statics end at `$72BD` (651 bytes), leaving ~250 bytes of stack;
`bsscheck.py` fails the build if statics ever pass `$72E0`. The rules that
keep it that way are at the top of `constants.h`.

## Building

Needs z88dk, [os7lib](https://github.com/tschak909/os7lib), and
fujinet-lib-experimental's `coleco` target (built automatically). No
container; the ADAM subtype's defoogi rig is not needed for plain `+coleco`.

    make            # build/lobby.rom -- stamped, exactly 32768 bytes
    make install    # copy into $(PICO_COLECO)/build/lobby.bin
    make clean

Env overrides: `Z88DK`, `OS7LIB`, `FNLIB`, `PICO_COLECO` (defaults under
`~/Workspace`), and `LOBBY_URL` to point the build at another lobby server —
it rides a generated `build/lobby_url.h`, because zcc strips the quotes off a
`-D` string. A plain `make` reverts to production.

## Testing

`./run.sh` builds, installs into the pico bring-up tree, and defers to that
tree's MAME harness (patched MAME + a fujinet-pc BoIP listener on
`127.0.0.1:9995`):

    ./run.sh                                    # windowed
    SCREEN_AT=12 ./run.sh --headless "FUJINET LOBBY"
    DRIVE_SCRIPT="wait,wait,wait,wait,k3,wait" DRIVE_EXPECT="2/2" ./run.sh --drive

For a seeded lobby, run `../server/` locally (`make` in that directory,
listens on :8080), POST rooms whose `clients` carry
`{"platform":"coleco","url":"tnfs://host/path.rom"}` — the client url must
parse as a URI, so it needs its `tnfs://` scheme — and build with
`LOBBY_URL='N:http://127.0.0.1:8080/view'`. A room whose client url points at
the fujinet-pc's own SD (`tnfs://SD/game.rom`) boots without any TNFS server.

Verified end to end in that rig: name resolve/edit round trip (appkey
checked on disk), page forward/back including the up/down page-crossings,
group re-heading across pages, the page counter's full-list walk, and a
complete boot — record re-fetch, host-slot claim, appkey[game_type] handoff,
MOUNT_IMAGE progress, ROM swap into a real game image.

## Keys

| Key | Action |
|---|---|
| stick up/down | move the bar (crosses pages at the ends) |
| stick left/right, keypad 2/3 | page back / forward |
| fire | boot the selected room's game client |
| keypad 1 | edit the username |
| keypad 9 | refresh (re-walks the page count at page 1) |

## Known limitations

- bin=1 only: the `bin=2` chat-room URL is not fetched, and there is no QR
  screen yet. The fetch design takes the 215-byte stride unchanged when that
  lands.
- The list is live and the boot path re-fetches by offset, so a room that
  moves between the page fetch and the boot fetch can boot a neighbor; the
  `game_type != 0` check is the only guard. Window is seconds.
- Booting reclaims host slot 8 every time, overwriting whatever was there
  (intv's documented policy — leftover slots can hold garbage that fools a
  match check).
- Paging past ~10 pages deep stops pushing (the page stack is 10 entries).
- After booting a game, only a power cycle returns to the Lobby.
