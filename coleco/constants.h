/* constants.h -- the ColecoVision Lobby client's fixed numbers.
 *
 * RAM RULES (the machine has 1K, of which ~700 bytes are usable once OS7's
 * tables and the stack are paid for -- same preamble as
 * fujinet-config/coleco/constants.h, and the same discipline):
 *
 *   - NEVER buffer tabular or list data: the server list is drawn straight
 *     out of the cartridge's reply window at $F800, one 189-byte record per
 *     network READ, and never lands in console RAM. The only reply-to-RAM
 *     copies in the program are the boot handoff's `url_copy`/`boot_copy`
 *     (st_boot.c), `prevgame` (the 17 bytes the header grouping needs across
 *     window repaints) and `username` -- all bounded.
 *   - No arrays as locals, no printf family. The stack is ~250 bytes.
 *   - The reply window is only repainted by committing a transaction, so any
 *     record acted on after later transactions must be RE-FETCHED first
 *     (st_boot.c's pagesize=1 read), never remembered.
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* States. The name editor is a blocking call, not a state, because
 * fn_edit() runs no mailbox transactions and returns in place. */
enum {
  ST_LIST = 0,
  ST_BOOT
};

/* Screen geometry: 32x24 Graphics 1, same layout family as fujinet-config:
 * row 1 title + page counter + username, row 3 column labels, rows 5-20 the
 * list window, rows 22-23 status and keypad legend. */
#define LIST_TOP      5
#define LIST_ROWS     16
#define LIST_LAST_ROW (LIST_TOP + LIST_ROWS - 1)
#define STATUS_ROW    22
#define LEGEND_ROW    23

#define GAME_LEN      16        /* header text, col 1..16 */
#define ROOM_LEN      23        /* room name, col 2..24; players end col 30 */

/* The lobby wire format, bin=1 (clients/src/main.c's ServerDetails, packed).
 * Reply: 3-byte header {server_count, reserved, reserved}, then server_count
 * 189-byte records. Frozen forever on the server side (server/model.go). */
#define REC_GAMETYPE   0        /*   1  appkey key_id for the URL handoff */
#define REC_GAME       1        /*  17  group header text */
#define REC_SERVER     18       /*  33  the selectable room row */
#define REC_SERVERURL  51       /*  65  written to appkey[game_type] */
#define REC_CLIENTURL  116      /*  65  TNFS host+path to mount and boot */
#define REC_REGION     181      /*   3  unused */
#define REC_ONLINE     184      /*   1  unused */
#define REC_PLAYERS    185      /*   1 */
#define REC_MAXPLAYERS 186      /*   1 */
#define REC_PINGAGE    187      /*   2  unused */
#define REC_STRIDE     189

/* Records asked for per page. Sixteen can never all draw (16 records need at
 * least 17 rows once one header is paid for), which is what makes
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

/* The ColecoVision has exactly one thing to mount into: the cartridge. */
#define DEVICE_SLOT   0
#define MODE_READ     1

/* The endpoint, minus the two paging numbers netraw streams in per fetch.
 * Overridable for testing -- LOBBY_URL='N:http://127.0.0.1:8080/view' in
 * make's environment lands here through the generated build/lobby_url.h. */
#include "lobby_url.h"
#ifndef LOBBY_URL_BASE
#define LOBBY_URL_BASE \
  "N:https://lobby.fujinet.online/view?bin=1&platform=coleco&pagesize="
#endif

#endif /* CONSTANTS_H */
