/* netraw.h -- lobby fetches over the network device (0x71), with the reply
 * left in the cartridge's window at FN_REPLY.
 *
 * fujinet-lib's own network path (common/network_read.c over bus/coleco/
 * network_bus_coleco.c) sets FUJI_FIELD_REPLY and copies every read into a
 * caller buffer -- exactly the RAM this machine does not have. These issue
 * the same transactions byte-for-byte as intv/fujinet.bas's proven net_open/
 * net_status/net_read/net_close shapes and leave the data addressable in the
 * window instead, where st_list.c paints it straight to VRAM.
 */

#ifndef NETRAW_H
#define NETRAW_H

#include <stdbool.h>

/* OPEN the lobby endpoint for HTTP GET with the given paging, then
 * settle-poll STATUS until the byte count holds still (a cold HTTP fetch
 * trickles in). The URL is streamed into the TX page from ROM literals --
 * no RAM buffer. False on open/status failure (an empty page 404s here). */
bool netraw_lobby_open(unsigned int offset, unsigned char pagesize);

/* READ `want` bytes; the data sits at FN_REPLY until the next committed
 * transaction repaints it. Returns the byte count actually read (0 on
 * failure); a short count means the reply ran out -- a partial record cannot
 * be rejoined across window repaints, so callers truncate there. */
unsigned int netraw_read(unsigned int want);

void netraw_close(void);        /* best effort */

#endif /* NETRAW_H */
