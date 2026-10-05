# FujiNet Game Lobby for the NES

A standalone NES client for the FujiNet Game Lobby, in the shape of
[`../coleco`](../coleco). It resolves the shared username, lists the lobby's
rooms for platform `nes` a page at a time, and boots the chosen room's game
client through the FujiNet NES cartridge. Before booting, it hands the game
its server URL through `appkey[game_type]`.

It is a 32K NROM image with 8K CHR-ROM and uses the cartridge's 8K of WRAM. It
is built with cc65 and `fujinet-lib-experimental`'s `nes` target.

## Look

- **From fujinet-config's `nes/` port (branch `nes-family-basic`):** the Family
  BASIC skin and the whole platform layer. That means the light-blue double
  frame, the 7x7 font, "?xx ERROR" with a BEEP, a click on every press, and
  the on-screen keyboard. `fujidisp`, `fujiin`, `fujiedit` and `sfx` are synced
  copies; fix them there.
- **From the Google Calendar NES client:** the layout. A header sits in the
  frame's top edge, rules fence the list off from a two-line panel that
  spells out the selected room, and the pad's buttons appear as buttons in a
  legend along the bottom edge.

```
 + FUJINET LOBBY ======= THOM +
 |GAME/ROOM  PAGE 1/2     PLRS|
 +----------------------------+
 |5 Card Stud                 |
 | High Rollers            5/6|      the bar is on a room
 | Bot Table 1             2/8|
 |Battleship                  |
 | Harbor                  1/2|
 +----------------------------+
 |High Rollers                |      the room's full name
 |EU SD/games/5card.nes       |      region, where its client comes from
 +----------------------------+
 |START--RELOAD               |
 + (A)JOIN (B)NAME <>PAGE ====+
```

**Controls:**

| Button | Action |
|---|---|
| Up/Down | Move the selection; moving past the first or last room changes page |
| Left/Right | Page back / forward |
| A | Join the selected room |
| B | Edit your name |
| START | Reload from the first page |

A Family BASIC or Subor keyboard works too: arrows, RETURN, and typing in the
name editor.

## Build

```
make                      build/lobby.nes against lobby.fujinet.online
make LOBBY_URL='N:http://127.0.0.1:8094/view'    a local lobby server
make install              copy into the pico bring-up tree (PICO_NES)
make run                  install, then MAME against a live fujinet-pc
make shot PRESS="12:Down 13:A" AT=20   headless capture: screen text + PNG
```

| Variable | Default | Use |
|---|---|---|
| `FNLIB` | `~/Workspace/fujinet-lib-experimental` | Must be on branch `add-nes`. |
| `PICO_NES` | `~/Workspace/fn-nes/pico/nes` | Optional. Supplies `checkrom.py`, `run.sh` and the MAME device. |

## Testing against a live FujiNet

`tools/nes-shot.sh` needs three things:

- a MAME tree with the FujiNet NES slot (`$PICO_NES/emu/apply.sh`);
- an RS232 fujinet-pc listening for BoIP on `127.0.0.1:9995`;
- a lobby server, either the real one or `../server` run locally with rooms
  seeded for platform `nes`.

Only one MAME can hold the BoIP link at a time.

Joining a room overwrites host slot 8 and the device slot 0 path on that
fujinet-pc, and writes `SD/FujiNet/000101<game_type>.key`.

A fujinet-pc older than fujinet-firmware `da3973c10` does not treat `.nes` as
cartridge ROM media. It mounts the image as a disk, and LOADING stays at 0%.
On such a build, test with the image renamed to `.bin`.

## Deployment

5 Card Stud's "back to lobby" boots `ec.tnfs.io:/nes/lobby.nes`, so that is
where `build/lobby.nes` goes.
