/* constants.h -- the NES Lobby client's fixed numbers.
 *
 * RAM RULES. The NES has 2K of its own (zero page, the hardware stack, cc65's
 * PPU write buffer and the C stack) and, behind a FujiNet cartridge, 8K of
 * WRAM at $6000 that cc65's NES target uses for DATA/BSS. That is roomy next
 * to the ColecoVision this is ported from (../coleco), so one rule there is
 * relaxed and the rest are kept:
 *
 *   - A page's rooms ARE copied to WRAM as they are drawn (st_list.c's
 *     `rooms`), bounded at LIST_ROWS entries of fixed width. That is what
 *     lets the detail panel follow the selection bar, and lets a join act on
 *     the room it showed instead of re-fetching it.
 *   - Reads still come one 189-byte record at a time through the
 *     cartridge's 1K reply window at $5000; a whole page (3 + 16 x 189
 *     bytes) does not fit in one reply.
 *   - The reply window is only repainted by committing a transaction, so
 *     nothing reads a record out of it after a later transaction.
 *   - No arrays as locals beyond a few bytes, no printf family.
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* States. The name editor is a blocking call, not a state, because
 * fn_edit() runs no mailbox transactions and returns in place. */
enum {
  ST_LIST = 0,
  ST_BOOT
};

/* Screen geometry. cc65's conio is 32x28 (NTSC); the Family BASIC frame
 * (fujidisp.h) is rows 2-25, cols 1-30, with a 28-column text area inside.
 * Laid out the way the Google Calendar NES client lays out its screens:
 *
 *   row 2      the frame's top edge: "FUJINET LOBBY" left, username right
 *   row 3      column labels and the page counter
 *   row 4      rule
 *   rows 5-19  game headers and their room rows; the bar is on a room
 *   row 20     rule
 *   rows 21-22 the selected room, spelled out
 *   row 23     rule
 *   row 24     status
 *   row 25     the frame's bottom edge, with the button legend set into it
 */
#define LABEL_ROW     3
#define RULE1_ROW     4
#define LIST_TOP      5
#define LIST_ROWS     15
#define LIST_LAST_ROW (LIST_TOP + LIST_ROWS - 1)
#define RULE2_ROW     20
#define PANEL_ROW     21        /* and PANEL_ROW + 1 */
#define RULE3_ROW     23
#define STATUS_ROW    24
#define LEGEND_ROW    25        /* = FRAME_B */

#define GAME_COL      2         /* header text, col 2..17 */
#define GAME_LEN      16
#define ROOM_COL      3         /* room name, col 3..23; players end col 29 */
#define ROOM_LEN      21
#define PLAYERS_END   29

/* Tiles this client adds below $20 (font.txt). */
#define T_RULE        0x19
#define T_TEE_L       0x0E
#define T_TEE_R       0x0F
#define T_BTN_A       0x08
#define T_BTN_B       0x09
#define T_PAD_L       0x0B
#define T_PAD_R       0x0C

/* The lobby wire format, bin=1 (clients/src/main.c's ServerDetails, packed).
 * Reply: 3-byte header {server_count, reserved, reserved}, then server_count
 * 189-byte records. Frozen forever on the server side (server/model.go). */
#define REC_GAMETYPE   0        /*   1  appkey key_id for the URL handoff */
#define REC_GAME       1        /*  17  group header text */
#define REC_SERVER     18       /*  33  the selectable room row */
#define REC_SERVERURL  51       /*  65  written to appkey[game_type] */
#define REC_CLIENTURL  116      /*  65  TNFS host+path to mount and boot */
#define REC_REGION     181      /*   3  shown in the panel */
#define REC_ONLINE     184      /*   1  unused */
#define REC_PLAYERS    185      /*   1 */
#define REC_MAXPLAYERS 186      /*   1 */
#define REC_PINGAGE    187      /*   2  unused */
#define REC_STRIDE     189

#define GAME_FIELD     17
#define SERVER_FIELD   33
#define URL_FIELD      65
#define REGION_FIELD   3

/* Records asked for per page. Sixteen can never all draw in 15 rows (16
 * records need at least 17 once one header is paid for), which is what makes
 * "drawn < count" a reliable more-pages signal alongside a full reply. */
#define PAGESIZE      16
#define PSTK_MAX      10        /* page-start stack depth, same as intv */

/* Username appkey: creator=1/app=1/key=0, the slot every FujiNet client
 * shares. The booted game's server URL goes to key = game_type. */
#define AK_CREATOR_LO 1
#define AK_CREATOR_HI 0
#define AK_APP        1
#define NAME_MAX      8         /* 2-8 chars, A-Z/0-9, like intv/st_name.bas */

#define HOST_SLOTS    8
#define HOST_STRIDE   32        /* READ_HOST_SLOTS: 8 x 32 bytes */
#define PAYLOAD_LEN   256       /* SET_DEVICE_FULLPATH's fixed buffer */

/* The NES has exactly one thing to mount into: the cartridge. */
#define DEVICE_SLOT   0
#define MODE_READ     1

/* fn_commit()'s wait, in seconds, around the network device's OPEN and
 * CLOSE: the cartridge gives those 90 s (an HTTPS round trip from a cold
 * ESP32 can be slow), so ours must outlast it for the cart's own timeout to
 * be the one that surfaces. */
#define NET_TIMEOUT   95

/* The endpoint, minus the two paging numbers netraw streams in per fetch.
 * Overridable for testing -- LOBBY_URL='N:http://127.0.0.1:8080/view' in
 * make's environment lands here through the generated build/lobby_url.h. */
#include "lobby_url.h"
#ifndef LOBBY_URL_BASE
#define LOBBY_URL_BASE \
  "N:https://lobby.fujinet.online/view?bin=1&platform=nes&pagesize="
#endif

#endif /* CONSTANTS_H */
