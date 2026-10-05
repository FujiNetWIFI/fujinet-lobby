/* netraw.h -- lobby fetches over the network device (0x71), with the reply
 * left in the cartridge's window at FN_REPLY. ../coleco/netraw.c, retargeted.
 *
 * fujinet-lib's own network path copies every read into a caller buffer and
 * builds the URL in another. Neither is needed: the URL streams into the TX
 * page from ROM literals, and each record is picked out of the window field
 * by field (st_list.c). The transactions are the same byte-for-byte as
 * intv/fujinet.bas's proven net_open/net_status/net_read/net_close shapes.
 */

#ifndef NETRAW_H
#define NETRAW_H

#include <stdbool.h>

/* OPEN the lobby endpoint for HTTP GET with the given paging, then
 * settle-poll STATUS until the byte count holds still (a cold HTTP fetch
 * trickles in). The URL is streamed into the TX page from ROM literals --
 * no RAM buffer. False on open/status failure (an empty page 404s here). */
bool netraw_lobby_open(unsigned int offset, unsigned char pagesize);

/* Why the last netraw_lobby_open() returned false. An empty page arrives as
 * an HTTP 404, which the network device reports as NET_ERR_NOT_FOUND; that
 * is not a fault, and leaves netraw_fault false. Anything else is: the cart
 * timing out, a NAK, or the device failing to reach the server at all (207,
 * not connected, when nothing is listening). netraw_err is the code to show:
 * the network device's error, else the cart's (fail()'s convention). */
#define NET_ERR_NOT_FOUND 170
extern bool netraw_fault;
extern unsigned char netraw_err;

/* READ `want` bytes; the data sits at FN_REPLY until the next committed
 * transaction repaints it. Returns the byte count actually read (0 on
 * failure); a short count means the reply ran out -- a partial record cannot
 * be rejoined across window repaints, so callers truncate there. */
unsigned int netraw_read(unsigned int want);

void netraw_close(void);        /* best effort */

#endif /* NETRAW_H */
