# FujiNet Game Lobby for the Bally Astrocade

A standalone Z80 assembly client, the same kind as the other Astrocade FujiNet
apps. The shared C client in `clients/` cannot fit this machine, and there is
no fujinet-lib for it. It talks to the lobby through the FujiNet Astrocade
cartridge, the RP2040 mailbox cart from `fujinet-firmware/pico/astrocade`.

Pieces are borrowed from other Astrocade ports:

- **From 5 Card Stud** (`fujinet-5cardstud/astrocade`): the mailbox transport,
  the 4x6 text drawn through the magic expander, input, and sound.
- **From CONFIG** (`fujinet-config/astrocade`): the boot handoff.

CONFIG's keypad **0** ("GAME LOBBY") boots this image from
`ec.tnfs.io:/astrocade/lobby.bin`.

## Screen

    FUJINET GAME LOBBY                  THOMC
    GAME / ROOM                       PLAYERS
    5 CARD STUD                                 <- game header, yellow
      NORMAL                              2/8   <- room, white
      HIGH ROLLERS                        0/8   <- highlight bar: inverse
      ...
                  < PAGE 1 >
    TRIG JOIN  L/R PAGE  CE NAME  = REFRESH

- **Colours:** games are yellow, rooms white, and the headings and legend cyan,
  all on deep blue. The palette is `PALET` in `lobby.asm`.
- **Grouping:** a game header is drawn whenever the game changes, and always at
  the top of a page. A header is never left alone on the last row. The
  grouping follows the coleco port.
- **Highlight bar:** rooms are drawn in colours 0 and 3 only, so the bar simply
  complements the row's bytes in place. It needs no redraw and has no flicker.

## Controls

| Input | Action |
|---|---|
| Stick up / down | Move the bar between rooms, skipping headers. Crosses into the next or previous page at either end. |
| Stick left / right | Previous / next page. |
| Trigger | Join the highlighted room. |
| CE | Change your name (the shared appkey 1/1/0). |
| = | Refetch the page. |

The first run asks for a name if the shared key is empty.

Sounds:

- **Move:** a quiet 1 kHz tick on each move that goes somewhere. A move that
  goes nowhere is silent.
- **Join:** a rising C-E-G-C arpeggio.

## Protocol

    N:<ENDPOINT>?bin=1&platform=astrocade&pagesize=5&offset=N

- Five 189-byte v1 records fit the 1K reply window, so a page is filled in
  chunks of five. The reply is read in place (`state.inc`).
- An empty page comes back as an HTTP 404. `net.inc` flags it in `V_HTTPE`, and
  the list treats it as the end of the list.
- Only the record offset of each row is kept. On the trigger, that one record
  is fetched again (`pagesize=1`).

The platform tag is `astrocade`. The server accepts any tag, so a game appears
here as soon as its server registers an `astrocade` client URL.

## Joining (`boot.inc`)

Joining a room sends nothing to the lobby. It is a local handoff, the same one
the 2600 port does:

1. Fetch the one record again. Copy its server and client URLs to RAM, because
   every transaction after this repaints the reply window.
2. Split the client URL `scheme://host/path`. Then read the host slots and
   write them back with **slot 8** replaced by that host. CONFIG's lobby
   shortcut claims slot 8 as well.
3. `MOUNT_HOST`.
4. Write the room's server URL to appkey 1/1/`game_type`. The client that boots
   next reads it from there. This step is best effort.
5. `SET_DEVICE_FULLPATH`, then `MOUNT_IMAGE` with a progress bar, then
   `BOOTLOCK` and the swap stub.

Errors show on the status row. Any key goes back to the list.

## Building

    ./build.sh      # or make: build/lobby.bin, exactly 8192 bytes
    make run        # MAME + a fujinet-pc BoIP listener
    make shot       # DEMO build, headless MAME, PNGs in build/astrocde/

Environment variables:

- `ENDPOINT=`: the lobby `/view` URL. Defaults to
  `https://lobby.fujinet.online/view`.
- `PLATFORM=`: defaults to `astrocade`.
- `DEMO=1`: every request is answered from a canned room list
  (`tools/mkdemo.py`) through the real parse, draw and join code, with no
  network. The join stops before any Fuji command. `make shot` uses this with
  `emu/shot.lua` to snapshot page 1, a moved bar, page 2 and the join screen.
  It also logs each sound cue.
- `FUJI_FIRMWARE=`: supplies `checkrom.py`, and zmac when it is not on PATH or
  in `~/Workspace/zmac-1.3`.

`emu/live.lua` runs the same walk against a real lobby, and then joins. To test
without the production server, run `server/` from a scratch directory with a
fresh database on a spare port. Seed it with rooms whose `astrocade` client URL
is `tnfs://SD/5card.bin`, then build with
`ENDPOINT=http://127.0.0.1:<port>/view`.

**Warning:** a live join rewrites host slot 8 and appkey 1/1/`game_type` on the
fujinet-pc it is connected to.

## Budget

The code must fit 0000H–1AFFH of the 8K window. `tools/checksize.py` prints the
size of each module on every build: about 4.1K for the live build and 5.2K for
the DEMO build with its canned records. `build.sh` stamps the `FUJI` claim at
1CFCH.

RAM is screen RAM. The 90 visible lines use 4000H–4E0FH, and the variables,
buffers and stack sit above that; the map is in `lobby.asm`.

## Not done

- CI: the defoogi container has no zmac, so this directory is not in
  `.github/workflows/ci.yml` yet.
- No auto-repeat on the stick. Each push moves the bar one row.
