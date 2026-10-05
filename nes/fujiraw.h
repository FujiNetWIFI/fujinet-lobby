/* fujiraw.h -- the transactions fujinet-lib's call shapes do not fit, built
 * on the lib's own exported mailbox primitives (fn_regwr, fn_tx, fn_commit).
 *
 * The union of fujinet-config nes/fujiraw.c (host slot write, MOUNT_IMAGE
 * without waiting) and ../coleco/fujiraw.c (the primitives exported, so
 * netraw.c can address the network device through them, and the appkey
 * quartet); the directory and SSID transactions the lobby never runs are
 * dropped.
 *
 * fuji_bus_call() takes its payload as one contiguous buffer. These payloads
 * are assembled from pieces -- a URL from ROM literals plus paging numbers,
 * all eight host slots with one replaced out of the reply window -- so they
 * are streamed byte by byte and padded on the fly.
 *
 * Every mailbox store goes through fn_regwr()/fn_tx(): never a read-modify-
 * write instruction on $5500-$57FF (fujinet-bus-nes.h's ONE RULE).
 */

#ifndef FUJIRAW_H
#define FUJIRAW_H

#include <stdbool.h>

/* The streamed-transaction primitives. Order per transaction: fnraw_begin,
 * then any fnraw_param*, then any payload bytes, then fnraw_finish. */
void fnraw_begin(unsigned char device, unsigned char cmd);
/* One parameter; `count` is the running parameter count including this one.
 * Parameters ride the TX stream as {size, value bytes little-endian}. */
void fnraw_param8(unsigned char v, unsigned char count);
void fnraw_param16(unsigned int v, unsigned char count);
void fnraw_tx_str(const char *s);
void fnraw_tx_padded(const char *s, unsigned int total);
bool fnraw_finish(void);
bool fnraw_reply_ok(void);

/* WRITE_HOST_SLOTS takes no parameters and one 256-byte payload -- all eight
 * 32-byte slots, every time. Re-reads the slots and streams them straight
 * back out of the reply window with slot `slot` replaced by `name`. */
bool fnraw_write_host_slot(unsigned char slot, const char *name);

/* SET_DEVICE_FULLPATH from a plain string, NUL-padded to exactly 256. */
bool fnraw_set_device_path(unsigned char dev, unsigned char host_slot,
                           unsigned char mode, const char *fullpath);

/* MOUNT_IMAGE without waiting for the reply. The FujiNet pushes the whole
 * image to the cartridge before it answers, so a caller that wants to show
 * the transfer polls the boot registers until FN_ACKSEQ reads the returned
 * sequence number, then checks fnraw_reply_ok(). */
unsigned char fnraw_mount_start(unsigned char dev, unsigned char mode);

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
