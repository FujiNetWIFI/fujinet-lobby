/* fujiraw.h -- the transactions fujinet-lib's call shapes do not fit, built
 * on the lib's own exported mailbox primitives (fn_regwr, FN_TXPAGE,
 * fn_commit).
 *
 * The union of fujinet-config sms/fujiraw.c (host slot write, MOUNT_IMAGE
 * without waiting, the boot counters, the long commit) and the nes branch's
 * nes/fujiraw.c (the primitives exported, so netraw.c can address the
 * network device through them, and the appkey quartet); the directory, SSID
 * and copy transactions the lobby never runs are dropped.
 *
 * fuji_bus_call() takes its payload as one contiguous buffer. These payloads
 * are assembled from pieces -- a URL from ROM literals plus paging numbers,
 * all eight host slots with one replaced out of the reply window -- so they
 * are streamed byte by byte and padded on the fly.
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
void fnraw_tx(unsigned char b);
void fnraw_tx_str(const char *s);
void fnraw_tx_padded(const char *s, unsigned int total);
bool fnraw_finish(void);
/* fnraw_finish() with the fuse scaled ~12x, for what the cartridge gives
 * longer than the lib's fn_commit() waits: 90 s for the network device's
 * OPEN and CLOSE (an HTTPS round trip from a cold ESP32 can be slow). The
 * cart's own timeout is then the one that surfaces, as its error code. */
bool fnraw_finish_long(void);
bool fnraw_reply_ok(void);

/* WRITE_HOST_SLOTS takes no parameters and one 256-byte payload -- all eight
 * 32-byte slots, every time. Re-reads the slots and streams them straight
 * back out of the reply window with slot `slot` replaced by `name`. */
bool fnraw_write_host_slot(unsigned char slot, const char *name);

/* SET_DEVICE_FULLPATH from a plain string, NUL-padded to exactly 256. */
bool fnraw_set_device_path(unsigned char dev, unsigned char host_slot,
                           unsigned char mode, const char *fullpath);

/* MOUNT_IMAGE, committed without waiting. Every server pushes the whole
 * image to the cartridge before it ACKs the mount, so a caller that wants to
 * watch the push polls the boot registers until FN_ACKSEQ reaches the
 * sequence number returned here -- and writes NOTHING to the mailbox until
 * it does (a register write would only queue up behind the transaction). */
unsigned char fnraw_mount_start(unsigned char dev, unsigned char mode);

/* One of the cart's 24-bit little-endian boot counters (FN_BOOTGOT,
 * FN_BOOTTOT). The cart writes them low byte first while the push runs, so
 * it is read until two reads agree. */
unsigned long fnraw_boot_bytes(volatile unsigned char *p);

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
