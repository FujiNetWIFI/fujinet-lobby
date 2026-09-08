/* fujiraw.h -- the transactions fujinet-lib's call shapes cannot express on a
 * 1K machine, built on the lib's own exported mailbox primitives. Adapted from
 * fujinet-config/coleco/fujiraw.c: the primitives are exported here (netraw.c
 * addresses the network device through them) and the appkey quartet is added,
 * while the directory/SSID/copy transactions the lobby never runs are dropped.
 *
 * fuji_bus_call() takes its payload as one contiguous buffer and copies its
 * reply into another; both are the right shape everywhere RAM is plentiful.
 * Here they would each need a staging buffer the console does not have -- so
 * these stream their payloads byte by byte through the lib's FN_TOUCH/
 * fn_regwr, pulling from the reply window and from bounded RAM strings, and
 * pad on the fly.
 */

#ifndef FUJIRAW_H
#define FUJIRAW_H

#include <stdbool.h>

/* The streamed-transaction primitives. Order per transaction: fnraw_begin,
 * then any fnraw_param*, then any payload bytes, then fnraw_finish. The TX
 * stream is append-only and survives an NMI mid-transaction. */
void fnraw_begin(unsigned char device, unsigned char cmd);
/* One parameter; `count` is the running parameter count including this one.
 * Parameters ride the TX stream as {size, value bytes little-endian}. */
void fnraw_param8(unsigned char v, unsigned char count);
void fnraw_param16(unsigned int v, unsigned char count);
void fnraw_tx(unsigned char b);
void fnraw_tx_str(const char *s);
void fnraw_tx_padded(const char *s, unsigned int total);
bool fnraw_finish(void);

/* WRITE_HOST_SLOTS takes no parameters and one 256-byte payload -- all eight
 * 32-byte slots, every time. Re-reads the slots and streams them straight
 * back out of the reply window with slot `slot` replaced by `name`. */
bool fnraw_write_host_slot(unsigned char slot, const char *name);

/* SET_DEVICE_FULLPATH from a plain string, NUL-padded to 256. */
bool fnraw_set_device_path(unsigned char dev, unsigned char host_slot,
                           unsigned char mode, const char *fullpath);

/* AppKey, creator/app fixed at the lobby's 1/1 (constants.h). The wire
 * struct for OPEN is 6 bytes: creator_lo, creator_hi, app, key, mode,
 * reserved -- and the reserved byte is load-bearing: a 5-byte send leaves the
 * firmware's transaction_get() waiting forever (intv/fujinet.bas). mode:
 * 0=read, 1=write. */
bool fnraw_appkey_open(unsigned char key, unsigned char mode);
/* READ_APPKEY leaves the reply in the window: a 2-byte little-endian length,
 * then the key data at FN_REPLY + 2. Returns that length (0 on failure). */
unsigned int fnraw_appkey_read(void);
/* Writes strlen(s) bytes -- no NUL, no padding. */
bool fnraw_appkey_write(const char *s);
void fnraw_appkey_close(void);              /* best effort */

#endif /* FUJIRAW_H */
