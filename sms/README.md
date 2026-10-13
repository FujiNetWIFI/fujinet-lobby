# FujiNet Game Lobby for the Sega Master System

A standalone Master System client for the FujiNet Game Lobby, in the shape of
[`../coleco`](../coleco) and the NES port (branch `nes`). It resolves the
shared username, lists the lobby's rooms for platform `sms` a page at a time,
and boots the chosen room's game client through the FujiNet Master System
cartridge. Before booting, it hands the game its server URL through
`appkey[game_type]`.

It is a 32K image with the cartridge's "FUJI" claim, so the mailbox stays live
while it runs. It is built with z88dk's `+sms` target and
`fujinet-lib-experimental`'s `sms` target.

## Look

The look comes from **fujinet-config's `sms/` port** (branch
`sms-phantasy-star`), along with the whole platform layer: Phantasy Star's
windows, the pointer notched into the window's left edge, the message window
typing itself out, the Pause COMMAND window, the on-screen keyboard, the
progress window for the boot, and the sounds. `fujidisp`, `fujiin`,
`fujiedit`, `fujisnd`, `snd_data`, `fujisplash` and `font.txt` are synced
copies; fix them there.

What the lobby adds is **colour**, in `ink.c`:

| Row | Colour |
|---|---|
| Game header | yellow |
| Room with at least one player | white |
| Empty room | grey, one step dimmer |

The CONFIG font is one 1bpp colour per tile, so each colour is a second copy
of the font: tiles 160-255 for the headers and 288-383 for empty rooms. The
values are `COL_GAME` and `COL_DIM` in `constants.h`.

Unlike CONFIG's lists, the room list uses every row. A page spends rows on
game headers, and the colours do the separating that CONFIG's spacing does.

```
 +- GAME LOBBY ---------- THOM -+
 |                              |
 |5 Card Stud                   |      yellow
 > Bot Table 1              2/8 |      white: someone is there
 | Bot Table 2              0/8 |      grey: empty
 |Fujirkle                      |
 | Fujirkle US              1/4 |
 |  ...                         |
 +------------------------------+
 +------------------- PAGE 1/2 -+
 |                              |
 |Bot Table 1                 US|      the room's full name, its region
 |                              |
 |1 JOIN  2 REFRESH  PAUSE MENU |
 +------------------------------+
```

**Controls:**

| Button | Action |
|---|---|
| Up/Down | Move the pointer; moving past the first or last room changes page |
| Left/Right | Page back / forward |
| 1 | Join the selected room |
| 2 | Reload from the first page |
| Pause | COMMAND: REFRESH, NAME (edit your name), CONFIG (back to the cartridge's CONFIG) |

## Build

```
make                      build/lobby.sms against lobby.fujinet.online
make LOBBY_URL='N:http://127.0.0.1:8095/view'    a local lobby server
make install              copy into the pico bring-up tree (PICO_SMS)
make run                  install, then MAME against a live fujinet-pc
make shot PRESS="16:Down 17:B1" AT=26   headless capture: screen text + PNG
```

| Variable | Default | Use |
|---|---|---|
| `Z88DK` | `~/Workspace/z88dk` | A z88dk with the `+sms` target. |
| `FNLIB` | `~/Workspace/fnlib-sms` | fujinet-lib-experimental on branch `add-sms`. |
| `PICO_SMS` | `~/Workspace/fn-sms/pico/sms` | Optional. Supplies `run.sh` and the MAME device. |

## Testing against a live FujiNet

`tools/sms-shot.sh` needs three things:

- a MAME tree with the FujiNet Master System slot
  (`$PICO_SMS/emu/apply.sh`);
- an RS232 fujinet-pc listening for BoIP on `127.0.0.1:9995`;
- a lobby server, either the real one or `../server` run locally with rooms
  seeded for platform `sms`.

Only one MAME can hold the BoIP link at a time.

The script prints the name table as text and tags each row with the inks its
text uses (`G` header, `D` dim room, `W` white). A `*` marks the selected row.
It also prints the cartridge's `fujinet_mode`, which is nonzero once a joined
game has been swapped in. The list is up by about 16 s: the SMS1 BIOS and the
splash fanfare come first.

Seed a local lobby with rooms whose `clients` include
`{"platform":"sms","url":"tnfs://SD/sms/alexkidd.bin"}`. The URL needs a
scheme, or the server rejects it.

Joining a room overwrites host slot 8 and the device slot 0 path on that
fujinet-pc, and writes `SD/FujiNet/000101<game_type>.key`. Back up
`fnconfig.ini` and the keys first. fujinet-pc keeps the new slot in memory
until it restarts.

A fujinet-pc without the `.sms` media type (fujinet-firmware 302d4950d) mounts
a `.sms` image as a disk. Nothing is pushed to the cartridge, and the progress
window ends with NOT A CARTRIDGE IMAGE. On such a build, test with the image
renamed to `.bin`.

## Deployment

CONFIG's GAME LOBBY command boots `ec.tnfs.io:/sms/lobby.sms`, and so does the
"back to lobby" in the SMS game clients. That is where `build/lobby.sms` goes.
