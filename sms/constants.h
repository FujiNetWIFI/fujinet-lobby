/* constants.h -- the Master System Lobby client's fixed numbers.
 *
 * RAM RULES. The SMS has 8K at $C000, shared by DATA/BSS and the stack,
 * which grows down from $DFF0. The display layer's name-table shadow takes
 * 1.5K of it; the rest is roomy next to the ColecoVision this descends from
 * (../coleco), so it keeps the NES port's relaxation (nes branch, nes/):
 *
 *   - A page's rooms ARE copied to RAM as they are drawn (st_list.c's
 *     `rooms`), bounded at LIST_ROWS entries of fixed width. That is what
 *     lets the message window follow the pointer, and lets a join act on
 *     the room it showed instead of re-fetching it.
 *   - Reads still come one 189-byte record at a time through the
 *     cartridge's 1K reply window at $B000; a whole page (3 + 16 x 189
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

/* Screen geometry, inside the SMS CONFIG's two windows (fujidisp.h):
 *
 *   row 0      the main window's top border: "GAME LOBBY" set into it on
 *              the left, the username on the right
 *   row 1      blank, as in every CONFIG screen: the font's capitals are a
 *              full 8 rows tall, so text here would touch the title
 *   rows 2-16  game headers and their room rows, one per row (CONFIG's own
 *              lists use every other row; a lobby page spends rows on
 *              headers, so it packs them); the pointer notches into the
 *              left border beside the selected room
 *   row 17     the main window's bottom border
 *   row 18     the message window's top border, "PAGE X/Y" set into it
 *   row 20     the selected room spelled out; prompts and errors
 *   row 22     the button legend
 *
 * Column 30 stays clear in the list: the Pause COMMAND window's right edge
 * is column 29, and anything in 30 would show beside it.
 */
#define LIST_TOP      2
#define LIST_ROWS     15
#define LIST_LAST_ROW (LIST_TOP + LIST_ROWS - 1)

#define GAME_COL      1         /* header text, col 1..16 */
#define GAME_LEN      16
#define ROOM_COL      2         /* room name, col 2..23; "99/99" ends col 29 */
#define ROOM_LEN      22
#define PLAYERS_END   29
#define EDGE_END      28        /* last column of text set into a border */
#define DETAIL_LEN    27        /* message window: room name in col 1..27, */
                                /* region right-aligned to col 30 */

/* The list's colours. The font is 1bpp, one colour per tile, so a colour is
 * a whole copy of the font (ink.c): white is CONFIG's own copy at tile 32,
 * and the two below get copies of their own at tiles 160 and 288. Their
 * sprite-palette mirrors are set to the same value, so the selected row's
 * invisible A_SEL bit stays invisible. */
#define PAL_GAME      4         /* game headers */
#define PAL_DIM       5         /* rooms nobody is in */
#define COL_GAME      0x0F      /* yellow, CONFIG's accent */
#define COL_DIM       0x2A      /* light grey; a room with players is white */

/* The lobby wire format, bin=1 (clients/src/main.c's ServerDetails, packed).
 * Reply: 3-byte header {server_count, reserved, reserved}, then server_count
 * 189-byte records. Frozen forever on the server side (server/model.go). */
#define REC_GAMETYPE   0        /*   1  appkey key_id for the URL handoff */
#define REC_GAME       1        /*  17  group header text */
#define REC_SERVER     18       /*  33  the selectable room row */
#define REC_SERVERURL  51       /*  65  written to appkey[game_type] */
#define REC_CLIENTURL  116      /*  65  TNFS host+path to mount and boot */
#define REC_REGION     181      /*   3  shown in the message window */
#define REC_ONLINE     184      /*   1  unused */
#define REC_PLAYERS    185      /*   1  0 draws the room dim */
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
#define FULLLEN       120       /* st_boot.c: the longest name it titles */

/* The Master System has exactly one thing to mount into: the cartridge. */
#define DEVICE_SLOT   0
#define MODE_READ     1

/* The endpoint, minus the two paging numbers netraw streams in per fetch.
 * Overridable for testing -- LOBBY_URL='N:http://127.0.0.1:8080/view' in
 * make's environment lands here through the generated build/lobby_url.h
 * (zcc strips the quotes from a -D string, hence a header). */
#include "lobby_url.h"
#ifndef LOBBY_URL_BASE
#define LOBBY_URL_BASE \
  "N:https://lobby.fujinet.online/view?bin=1&platform=sms&pagesize="
#endif

#endif /* CONSTANTS_H */
