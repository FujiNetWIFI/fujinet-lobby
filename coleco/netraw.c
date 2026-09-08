/* netraw.c -- see netraw.h. The transaction shapes mirror intv/fujinet.bas
 * byte-for-byte (STATUS with two zero 8-bit params, READ with one 16-bit
 * length param), because this is the first client to drive the network
 * device over the coleco mailbox and those shapes are the proven ones.
 */

#include <fujinet-network.h>

#include "fujiraw.h"
#include "netraw.h"
#include "state.h"

/* Append v as decimal ASCII (no leading zeros) straight into the TX page. */
static void tx_dec(unsigned int v)
{
    unsigned int div = 10000u;
    unsigned char started = 0;
    unsigned char d;

    for (; div > 1; div /= 10u) {
        d = (unsigned char)((v / div) % 10u);
        if (d != 0 || started) {
            fnraw_tx((unsigned char)('0' + d));
            started = 1;
        }
    }
    fnraw_tx((unsigned char)('0' + (unsigned char)(v % 10u)));
}

/* NDeviceStatus: bytes 0-1 = bytes available (LE), byte 3 = error code,
 * 1 = SUCCESS. An HTTP error response still has a real, readable body, so
 * `avail` alone cannot tell it apart from a good reply; the error byte can. */
static bool netraw_status(unsigned int *avail)
{
    volatile unsigned char *r;

    fnraw_begin(FUJI_DEVICEID_NETWORK, FUJICMD_STATUS);
    fnraw_param8(0, 1);
    fnraw_param8(0, 2);
    if (!fnraw_finish()) {
        *avail = 0;
        return false;
    }
    r = FN_REPLY;
    *avail = (unsigned int)r[0] | ((unsigned int)r[1] << 8);
    return r[3] == 1;
}

bool netraw_lobby_open(unsigned int offset, unsigned char pagesize)
{
    unsigned int prev, avail;
    unsigned char i;

    fnraw_begin(FUJI_DEVICEID_NETWORK, FUJICMD_OPEN);
    fnraw_param8(OPEN_MODE_HTTP_GET_H, 1);
    fnraw_param8(OPEN_TRANS_NONE, 2);
    fnraw_tx_str(LOBBY_URL_BASE);
    tx_dec(pagesize);
    fnraw_tx_str("&offset=");
    tx_dec(offset);
    if (!fnraw_finish())
        return false;

    /* Settle: poll until the available count is nonzero and holds still
     * across consecutive polls, one vblank apart -- intv's net_open_settled.
     * Falling out of the loop with zero bytes is not a failure here; the
     * first read simply comes back short and the caller shows an empty
     * page. */
    prev = 0;
    for (i = 0; i < 20; i++) {
        wait_frames(1);
        if (!netraw_status(&avail))
            return false;
        if (avail != 0 && avail == prev)
            break;
        prev = avail;
    }
    return true;
}

unsigned int netraw_read(unsigned int want)
{
    fnraw_begin(FUJI_DEVICEID_NETWORK, FUJICMD_READ);
    fnraw_param16(want, 1);
    if (!fnraw_finish())
        return 0;
    return (unsigned int)FN_RXLEN_LO | ((unsigned int)FN_RXLEN_HI << 8);
}

void netraw_close(void)
{
    fnraw_begin(FUJI_DEVICEID_NETWORK, FUJICMD_CLOSE);
    fnraw_finish();
}
