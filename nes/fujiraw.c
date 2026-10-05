/* fujiraw.c -- streamed-payload transactions over fujinet-lib's NES mailbox
 * primitives. See fujiraw.h for why these exist at all.
 */

#include <fujinet-fuji.h>
#include <fujinet-bus-nes.h>

#include "constants.h"
#include "fujiraw.h"

void fnraw_begin(unsigned char device, unsigned char cmd)
{
    fn_regwr(FNR_DATA_RST, 0);
    fn_regwr(FNR_DEVICE, device);
    fn_regwr(FNR_CMD, cmd);
    fn_regwr(FNR_NPARAM, 0);
}

void fnraw_param8(unsigned char v, unsigned char count)
{
    fn_tx(1);
    fn_tx(v);
    fn_regwr(FNR_NPARAM, count);
}

void fnraw_param16(unsigned int v, unsigned char count)
{
    fn_tx(2);
    fn_tx((unsigned char)(v & 0xFF));
    fn_tx((unsigned char)(v >> 8));
    fn_regwr(FNR_NPARAM, count);
}

void fnraw_tx_str(const char *s)
{
    while (*s)
        fn_tx((unsigned char)*s++);
}

void fnraw_tx_padded(const char *s, unsigned int total)
{
    unsigned int n = 0;

    while (*s && n < total) {
        fn_tx((unsigned char)*s++);
        n++;
    }
    while (n++ < total)
        fn_tx(0);
}

bool fnraw_reply_ok(void)
{
    return FN_ERRCODE == FN_OK && FN_REPLYCMD == FUJICMD_ACK;
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
                fn_tx(r[j]);
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

unsigned char fnraw_mount_start(unsigned char dev, unsigned char mode)
{
    unsigned char want = (unsigned char)(FN_ACKSEQ + 1);

    if (want == 0)
        want = 1;               /* 0 means "never used" */
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_MOUNT_IMAGE);
    fnraw_param8(dev, 1);
    fnraw_param8(mode, 2);
    fn_regwr(FNR_SEQ, want);
    return want;
}

bool fnraw_appkey_open(unsigned char key, unsigned char mode)
{
    fnraw_begin(FUJI_DEVICEID_FUJINET, FUJICMD_OPEN_APPKEY);
    fn_tx(AK_CREATOR_LO);
    fn_tx(AK_CREATOR_HI);
    fn_tx(AK_APP);
    fn_tx(key);
    fn_tx(mode);
    fn_tx(0);                   /* reserved, and required -- see fujiraw.h */
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
