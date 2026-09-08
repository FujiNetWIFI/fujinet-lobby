/* fujiraw.c -- streamed-payload transactions over fujinet-lib's mailbox
 * primitives. See fujiraw.h for why these exist at all; adapted from
 * fujinet-config/coleco/fujiraw.c.
 *
 * Everything here leans on what bus/coleco exports: fn_sink (the volatile
 * sink that keeps sccz80 from discarding a hotspot read), FN_TOUCH,
 * fn_regwr() and fn_commit().
 */

#include <fujinet-fuji.h>
#include <fujinet-bus-coleco.h>

#include "constants.h"
#include "fujiraw.h"

#define PAYLOAD_LEN 256

void fnraw_begin(unsigned char device, unsigned char cmd)
{
    fn_regwr(FNR_DATA_RST, 0);
    fn_regwr(FNR_DEVICE, device);
    fn_regwr(FNR_CMD, cmd);
    fn_regwr(FNR_NPARAM, 0);
}

void fnraw_param8(unsigned char v, unsigned char count)
{
    FN_TOUCH(FN_TXPAGE + 1);
    FN_TOUCH(FN_TXPAGE + v);
    fn_regwr(FNR_NPARAM, count);
}

void fnraw_param16(unsigned int v, unsigned char count)
{
    FN_TOUCH(FN_TXPAGE + 2);
    FN_TOUCH(FN_TXPAGE + (unsigned char)(v & 0xFF));
    FN_TOUCH(FN_TXPAGE + (unsigned char)(v >> 8));
    fn_regwr(FNR_NPARAM, count);
}

void fnraw_tx(unsigned char b)
{
    FN_TOUCH(FN_TXPAGE + b);
}

void fnraw_tx_str(const char *s)
{
    while (*s)
        FN_TOUCH(FN_TXPAGE + (unsigned char)*s++);
}

void fnraw_tx_padded(const char *s, unsigned int total)
{
    unsigned int n = 0;

    while (*s && n < total) {
        FN_TOUCH(FN_TXPAGE + (unsigned char)*s++);
        n++;
    }
    while (n++ < total)
        FN_TOUCH(FN_TXPAGE);
}

bool fnraw_finish(void)
{
    return fn_commit() == FN_OK && FN_REPLYCMD == FUJICMD_ACK;
}

bool fnraw_write_host_slot(unsigned char slot, const char *name)
{
    volatile unsigned char *r;
    unsigned char i, j;

    /* Fresh window first: the write streams the other seven slots straight
     * back out of it. The reply is only repainted by a commit, so it holds
     * still while the TX stream is built. */
    if (!FUJICALL(FUJICMD_READ_HOST_SLOTS))
        return false;

    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_WRITE_HOST_SLOTS);
    for (i = 0; i < HOST_SLOTS; i++) {
        if (i == slot) {
            fnraw_tx_padded(name, HOST_STRIDE);
        } else {
            r = FN_REPLY + (unsigned int)i * HOST_STRIDE;
            for (j = 0; j < HOST_STRIDE; j++)
                FN_TOUCH(FN_TXPAGE + r[j]);
        }
    }
    return fnraw_finish();
}

bool fnraw_set_device_path(unsigned char dev, unsigned char host_slot,
                           unsigned char mode, const char *fullpath)
{
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_SET_DEVICE_FULLPATH);
    fnraw_param8(dev, 1);
    fnraw_param8(host_slot, 2);
    fnraw_param8(mode, 3);
    fnraw_tx_padded(fullpath, PAYLOAD_LEN);
    return fnraw_finish();
}

bool fnraw_appkey_open(unsigned char key, unsigned char mode)
{
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_OPEN_APPKEY);
    fnraw_tx(AK_CREATOR_LO);
    fnraw_tx(AK_CREATOR_HI);
    fnraw_tx(AK_APP);
    fnraw_tx(key);
    fnraw_tx(mode);
    fnraw_tx(0);                /* reserved, and required -- see fujiraw.h */
    return fnraw_finish();
}

unsigned int fnraw_appkey_read(void)
{
    volatile unsigned char *r;

    if (!FUJICALL(FUJICMD_READ_APPKEY))
        return 0;
    r = FN_REPLY;
    return (unsigned int)r[0] | ((unsigned int)r[1] << 8);
}

bool fnraw_appkey_write(const char *s)
{
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_WRITE_APPKEY);
    fnraw_tx_str(s);
    return fnraw_finish();
}

void fnraw_appkey_close(void)
{
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_CLOSE_APPKEY);
    fnraw_finish();
}
