# FujiNet Game Lobby for the Emerson Arcadia 2001

A standalone Signetics 2650 assembly client (Macroassembler AS) for the
FujiNet Arcadia cartridge (`fujinet-firmware/pico/arcadia`). It lists the
lobby's rooms for platform `arcadia`, grouped by game, and joins one.

CONFIG's **9 LOBBY** (`fujinet-config/arcadia`) boots this image from
`ec.tnfs.io:/arcadia/lobby.bin`.

Pieces come from other ports:

- **From `fujinet-config/arcadia`:** display, input, the grid editor, the
  mailbox, and the mount, progress bar and swap.
- **From `fujinet-5cardstud/arcadia`:** the N: round trip and the reply
  slices.
- **From the Astrocade and SMS lobbies:** grouping, paging, the name rules
  and the join.

## Screen

16 x 13 cells, 13-line multicolor. Games are yellow and rooms white, on blue.

    LOBBY       THOMC     black title band
    5 CARD STUD ====      game header + yellow mosaic rule
    |HIGH ROLLER 2/8      yellow bracket, room, players now/max
    |AI TABLE: E 1/8      selected room: black bar
    `THE BASEMEN 0/8      bracket corner on the group's last room
    BATTLESHIP =====
    `NAVY YARD   1/2

    ---< PAGE 2 >---      box-glyph rule, arrows only where a page exists
    AI TABLE: EASY B      detail panel: the selected room's full name
    OTS ONLY PLEASE         (up to 32 characters)
    FIRE JOIN ROOM        legend, four lines in turn

Block graphics:

- **Header rules:** after the game name come a `$C0` switch and mosaic cells.
- **Brackets and the page rule:** the 2637's built-in box glyphs.
- **Splash and join screens:** the splash has checker stripes; the join
  screen has the progress bar.
- **Arrows:** the UDCs.

## Controls

| Input | Action |
|---|---|
| Disc up / down, keypad 8 | Move between rooms. Moving past either end goes to the next or previous page. |
| Disc left / right, keypad 4 / 6 | Previous / next page |
| FIRE (= keypad 2), Enter | Join the selected room |
| 0 | Reload from page 1 |
| Clear | Change your name |

Directions auto-repeat. There is no way back to CONFIG: the Arcadia has no
exit-to-config, and RESET restarts the lobby.

## Name

The name is the shared username, appkey 1/1/0. It must be 2-8 letters or
digits.

- **First run:** if no valid name is stored, the editor opens first.
- **Saving:** a name is written only when you accept a valid one.
- **Escaping with nothing stored:** you play as `ARCADIA` for this session,
  and the name is not written.

## Protocol

    N:<ENDPOINT>?bin=1&platform=arcadia&pagesize=6&offset=N

**One fetch is one page.** The cart returns at most 1024 bytes, so 5 of the
189-byte records arrive whole. The 6th arrives cut off, but its game name is
present, which tells the lobby two things without another request:

- whether a next page exists;
- whether the 5th room ends its game group.

The reply stays in the window for the life of the page. The list, the detail
panel and the join all read it in place (`rx.inc`). A page holds 8 rows. A
game header is never left alone on the last row, and it repeats at the top of
a page.

**Status codes:**

| nDevStatus | Means | Shown as |
|---|---|---|
| 170 | 404: no rooms for this platform | NO ROOMS OPEN |
| anything else, or a transport failure | | NETWORK ERROR, with the code (`CF` = server unreachable) |

## Joining (`join.inc`, `boot.inc`)

Joining sends nothing to the lobby server.

1. Copy the record's server and client URLs to RAM. Every commit repaints the
   window.
2. Split `scheme://host/path`. Write the host into **host slot 8**: read the
   slots, then stream them back with slot 8 replaced. Then `MOUNT_HOST`.
3. Write the server URL to **appkey 1/1/<game type>**. This step is best
   effort.
4. `SET_DEVICE_FULLPATH`, then `MOUNT_IMAGE` with the progress bar, then
   `BOOTLOCK` and the swap stub.

Failures show the reason and return to the same page: BAD CLIENT URL, HOSTS
ERROR, HOST ERROR, PATH ERROR, MOUNT ERROR, ROM TOO BIG, TRUNCATED, NOT A
CART.

**The handoff is one-sided for now.** 5 Card Stud, Battleship and Fujitzee
for the Arcadia still use their built-in server URLs; none of them reads the
appkey yet.

## Building

    make            # build/lobby.bin: 8192 bytes, FUJI claim stamped
    make run        # MAME + a fujinet-pc BoIP listener (run.sh)
    make drive STEPS='t:PAGE 1;k:down;d;s'   # headless scripted session

`asl` and `p2bin` must be on PATH or in `~/asl`. `build.sh` runs
`tools/mkimage.py`, `checkrom.py`, `checkdepth.py` (the 8-entry return
stack; the current maximum is 5) and `checksize.py`.

| Variable | Default |
|---|---|
| `ENDPOINT=` | `https://lobby.fujinet.online/view` |
| `PLATFORM=` | `arcadia`. Another tag shows that platform's rooms, which is handy for a look at real data. |

After using either, run `make clean`, so the next plain `make` produces the
release image.

Budget: about 3.1K of block 1's 4096 bytes and 2.0K of block 2's 2816.

## Testing

Use a private rig, not production:

- **fujinet-pc:** a copy of `fujinet-pc-rs232/build/dist` with `[BOIP]
  port=9985`. Strip the token sections from the ini, and put Arcadia images
  under `SD/arcadia/`.
- **Lobby server:** `server/` built into a scratch directory with a fresh
  database, run with `-srvaddr 127.0.0.1:8096`. Seed it with POST `/server`
  rooms whose `arcadia` client URL is `tnfs://SD/arcadia/<game>.bin`.
- **Build and run:** `ENDPOINT=http://127.0.0.1:8096/view make`, then
  `make drive FUJINET_TCP=127.0.0.1:9985 STEPS=...`.

**Warning:** a live join rewrites host slot 8 and appkey 1/1/<game type> on
that fujinet-pc.
