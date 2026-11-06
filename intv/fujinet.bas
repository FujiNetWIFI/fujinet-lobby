' fujinet.bas -- FujiNet mailbox transport + network/appkey primitives.
' Verbatim copy of fujinet-fujitzee/intv/fujinet.bas (which carries the
' #mb_err/#net_err 16-bit fix for an IntyBASIC v1.4.2 codegen bug --
' fujinet-5cardstud/intv's older copy still has the 8-bit version), with
' one addition: net_read_chunked, since no existing IntyBASIC FujiNet
' client has ever needed to read a reply bigger than the 512-byte FN_RX
' window in one logical fetch. Every game client caps its endpoints under
' 512 bytes instead; the lobby's server list can't be capped that way.
'
' Mailbox layout is the hand-synchronized copy of
' fujinet-firmware/pico/intellivision/firmware/fuji_mailbox.h. Do not change
' these addresses without re-checking that file.
'
' MEMATTR intentionally stops at $9BFF, short of the $9C00-$9FFF mailbox
' itself: on real PiRTO II hardware the RP2040 maps that whole window as
' RAM unconditionally, so declaring less here doesn't affect real hardware.
' But jzIntv's --fujinet peripheral emulation registers its own handler for
' $9C00-$9FFF *after* the cart's generic MEMATTR RAM, and its layered bus
' dispatch lets whichever peripheral registered first answer a given
' address -- so declaring the full $8000-$9FFF range here would silently
' shadow the emulator's FujiNet peripheral with inert RAM, and the mailbox
' would never come up under --fujinet even though it works on real hardware.
    ASM MEMATTR $8000, $9BFF, "+RWN"

    CONST FN_MAGIC0     = $9C00
    CONST FN_MAGIC1     = $9C01
    CONST FN_SEQ        = $9C03
    CONST FN_ACKSEQ     = $9C04
    CONST FN_DEVICE     = $9C05
    CONST FN_CMD        = $9C06
    CONST FN_NPARAM     = $9C07
    CONST FN_TXLEN_LO   = $9C08
    CONST FN_TXLEN_HI   = $9C09
    CONST FN_ERR        = $9C0B
    CONST FN_RXLEN_LO   = $9C0C
    CONST FN_RXLEN_HI   = $9C0D
    CONST FN_REPLY_CMD  = $9C0E
    CONST FN_PARAM_SIZE = $9C10
    CONST FN_PARAM_VAL  = $9C20
    CONST FN_TX         = $9C40
    CONST FN_RX         = $9D40   ' 512 bytes max ($9D40-$9F3F)

    ' Boot-progress cells (RP2040-published), used by st_boot.bas while
    ' polling a MOUNT_IMAGE transaction that may run far longer than an
    ' ordinary mailbox round trip.
    CONST FN_BOOT_STATE = $9C18
    CONST FN_BOOT_PCT   = $9C19
    CONST FN_BOOT_ERR   = $9C1A
    CONST FUJI_BOOT_FAILED = $80

    CONST FUJICMD_ACK = $06
    CONST FUJICMD_NAK = $15

    ' Fuji device (config/appkey/mount) commands.
    CONST FUJI_DEVICEID        = $70
    CONST FUJICMD_OPEN_APPKEY  = $DC
    CONST FUJICMD_CLOSE_APPKEY = $DB
    CONST FUJICMD_WRITE_APPKEY = $DE
    CONST FUJICMD_READ_APPKEY  = $DD
    CONST FUJICMD_READ_HOST_SLOTS  = $F4
    CONST FUJICMD_WRITE_HOST_SLOTS = $F3
    CONST FUJICMD_MOUNT_HOST       = $F9
    CONST FUJICMD_MOUNT_IMAGE      = $F8
    CONST FUJICMD_SET_DEVICE_FULLPATH = $E2
    CONST FUJICMD_QRCODE_INPUT  = $BC
    CONST FUJICMD_QRCODE_ENCODE = $BD
    CONST FUJICMD_QRCODE_LENGTH = $BE
    CONST FUJICMD_QRCODE_OUTPUT = $BF

    CONST MODE_READ = 1

    ' Network device (N1:) commands.
    CONST NET_DEVICEID = $71
    CONST NETCMD_OPEN   = $4F
    CONST NETCMD_CLOSE  = $43
    CONST NETCMD_READ   = $52
    CONST NETCMD_STATUS = $53

    CONST OPEN_MODE_HTTP_GET_H = $0C
    CONST OPEN_TRANS_NONE = $00

    ' fn_ok: 1 = last transaction produced ACK, 0 = timeout or NAK.
    ' mb_err: FN_ERR value on failure (0 on timeout, since the RP2040 never answered).
    DIM fn_ok, #mb_err
    DIM mb_dev, mb_cmd, mb_nparam, mb_seq
    DIM #fn_txlen
    DIM #fn_t          ' generic frame-count timeout counter
    DIM #fn_src         ' VARPTR source for putstr/getstr
    DIM fn_len, fn_i    ' generic length/index for putstr/getstr

' ---------------------------------------------------------------------------
' fn_wait_mailbox: bounded wait for the RP2040 magic bytes at boot.
' ---------------------------------------------------------------------------
fn_wait_mailbox: PROCEDURE
    #fn_t = 0
    WHILE ((PEEK(FN_MAGIC0) AND 255) <> 70) AND ((PEEK(FN_MAGIC1) AND 255) <> 78) AND (#fn_t < 180)
        #fn_t = #fn_t + 1
        WAIT
    WEND
    IF #fn_t >= 180 THEN
        fn_ok = 0
    ELSE
        fn_ok = 1
    END IF
END

' ---------------------------------------------------------------------------
' fn_transact: issue the transaction described by mb_dev/mb_cmd/mb_nparam/
' #fn_txlen (payload already staged at FN_TX) and block for the reply. seq
' is ALWAYS derived from the RP2040's own FN_ACKSEQ, never from a local
' counter -- a console reset zeroes IntyBASIC vars but not the RP2040.
' ---------------------------------------------------------------------------
fn_transact: PROCEDURE
    POKE (FN_DEVICE), mb_dev
    POKE (FN_CMD), mb_cmd
    POKE (FN_NPARAM), mb_nparam
    POKE (FN_TXLEN_LO), #fn_txlen AND 255
    POKE (FN_TXLEN_HI), #fn_txlen / 256

    mb_seq = (PEEK(FN_ACKSEQ) AND 255) + 1
    IF mb_seq = 0 THEN mb_seq = 1
    POKE (FN_SEQ), mb_seq

    #fn_t = 0
    WHILE ((PEEK(FN_ACKSEQ) AND 255) <> mb_seq) AND (#fn_t < 900)
        #fn_t = #fn_t + 1
        WAIT
    WEND

    IF #fn_t >= 900 THEN
        fn_ok = 0
        #mb_err = 0
        RETURN
    END IF

    IF (PEEK(FN_REPLY_CMD) AND 255) <> FUJICMD_ACK THEN
        fn_ok = 0
        #mb_err = (PEEK(FN_ERR) AND 255)
        RETURN
    END IF

    fn_ok = 1
END

' ---------------------------------------------------------------------------
' fn_param: stage transaction parameter #pm_i (0-based), pm_size bytes
' (1 or 2), value #pm_val, little-endian.
' ---------------------------------------------------------------------------
DIM pm_i, pm_size
DIM #pm_val
fn_param: PROCEDURE
    POKE (FN_PARAM_SIZE + pm_i), pm_size
    POKE (FN_PARAM_VAL + pm_i * 4), #pm_val AND 255
    IF pm_size > 1 THEN POKE (FN_PARAM_VAL + pm_i * 4 + 1), #pm_val / 256
END

' ---------------------------------------------------------------------------
' fn_putstr: append fn_len ASCII bytes into FN_TX at #fn_txlen, from #fn_src
' (a ROM DATA VARPTR or a RAM address). Advances #fn_txlen.
' ---------------------------------------------------------------------------
fn_putstr: PROCEDURE
    FOR fn_i = 0 TO fn_len - 1
        POKE (FN_TX + #fn_txlen + fn_i), PEEK(#fn_src + fn_i) AND 255
    NEXT fn_i
    #fn_txlen = #fn_txlen + fn_len
END

' ---------------------------------------------------------------------------
' fn_strlen: scan the NUL-padded field at #fn_src (max ls_max bytes), set
' fn_len to the length up to (not including) the first NUL.
' ---------------------------------------------------------------------------
DIM ls_max
fn_strlen: PROCEDURE
    fn_len = 0
    WHILE (fn_len < ls_max) AND ((PEEK(#fn_src + fn_len) AND 255) <> 0)
        fn_len = fn_len + 1
    WEND
END

' ---------------------------------------------------------------------------
' net_open: open devicespec (ASCII bytes already staged at FN_TX, length in
' #fn_txlen) for HTTP GET.
' ---------------------------------------------------------------------------
net_open: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_OPEN
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = OPEN_MODE_HTTP_GET_H : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = OPEN_TRANS_NONE : GOSUB fn_param
    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' net_status: query byte count available. #net_avail = total bytes; #net_err
' is the 4th byte of the NDeviceStatus reply (1 = SUCCESS) -- an HTTP error
' response still has a real, readable body, so `avail` alone can't tell it
' apart from a good response; #net_err is what actually can.
' ---------------------------------------------------------------------------
DIM #net_avail
DIM #net_err
net_status: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_STATUS
    mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok THEN
        #net_avail = (PEEK(FN_RX) AND 255) + (PEEK(FN_RX + 1) AND 255) * 256
        #net_err = (PEEK(FN_RX + 3) AND 255)
        IF #net_err <> 1 THEN fn_ok = 0
    ELSE
        #net_avail = 0
    END IF
END

' ---------------------------------------------------------------------------
' net_read: read #net_readlen bytes into FN_RX. #net_gotlen is captured from
' RXLEN before the following CLOSE transaction overwrites it.
' ---------------------------------------------------------------------------
DIM #net_readlen
DIM #net_gotlen
net_read: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_READ
    mb_nparam = 1
    pm_i = 0 : pm_size = 2 : #pm_val = #net_readlen : GOSUB fn_param
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok THEN
        #net_gotlen = (PEEK(FN_RXLEN_LO) AND 255) + (PEEK(FN_RXLEN_HI) AND 255) * 256
    ELSE
        #net_gotlen = 0
    END IF
END

net_close: PROCEDURE
    mb_dev = NET_DEVICEID
    mb_cmd = NETCMD_CLOSE
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' api_call: full round trip for a reply that fits in one FN_RX window (<=512
' bytes) -- open, settle-poll status, read up to #net_readlen bytes, close.
' Not used by the lobby's own list fetch (that needs net_read_chunked
' instead, below); kept for anything else that wants the simple one-shot
' form, matching every other IntyBASIC FujiNet client's usage.
' ---------------------------------------------------------------------------
DIM ac_i
DIM #ac_prev
api_call: PROCEDURE
    GOSUB net_open
    IF fn_ok = 0 THEN RETURN

    #ac_prev = 0
    FOR ac_i = 0 TO 19
        WAIT
        GOSUB net_status
        IF fn_ok = 0 THEN RETURN
        IF #net_avail > 0 AND #net_avail = #ac_prev THEN EXIT FOR
        #ac_prev = #net_avail
    NEXT ac_i

    IF #net_avail < #net_readlen THEN #net_readlen = #net_avail

    GOSUB net_read
    GOSUB net_close
END

' ---------------------------------------------------------------------------
' net_open_settled: open + settle-poll status only, leaving the connection
' open for the caller to drive net_read itself, possibly more than once.
' Factored out of api_call so net_read_chunked can share the same settle
' logic without also performing a single net_read/net_close.
' ---------------------------------------------------------------------------
net_open_settled: PROCEDURE
    GOSUB net_open
    IF fn_ok = 0 THEN RETURN

    #ac_prev = 0
    FOR ac_i = 0 TO 19
        WAIT
        GOSUB net_status
        IF fn_ok = 0 THEN RETURN
        IF #net_avail > 0 AND #net_avail = #ac_prev THEN EXIT FOR
        #ac_prev = #net_avail
    NEXT ac_i
END

' ---------------------------------------------------------------------------
' AppKey. Wire struct is 6 bytes: creator_lo, creator_hi, app, key, mode,
' reserved. mode: 0=read, 1=write. Sending only 5 bytes (omitting reserved)
' leaves the firmware's transaction_get() blocked waiting for a byte that
' never arrives, which reads back as a timeout, not a protocol error.
' ---------------------------------------------------------------------------
DIM ak_creator_lo, ak_creator_hi, ak_app, ak_key, ak_mode

appkey_open: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_OPEN_APPKEY
    mb_nparam = 0
    POKE (FN_TX + 0), ak_creator_lo
    POKE (FN_TX + 1), ak_creator_hi
    POKE (FN_TX + 2), ak_app
    POKE (FN_TX + 3), ak_key
    POKE (FN_TX + 4), ak_mode
    POKE (FN_TX + 5), 0 ' reserved
    #fn_txlen = 6
    GOSUB fn_transact
END

' appkey_read: caller sets #fn_src (destination) and ls_max (destination
' buffer size, including the NUL this always writes at fn_len). The rs232
' transport prepends a 2-byte little-endian length ahead of the actual
' appkey bytes (unlike a network READ) -- use that embedded prefix as the
' real data length and skip past it.
appkey_read: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_READ_APPKEY
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok THEN
        fn_len = (PEEK(FN_RX) AND 255) + (PEEK(FN_RX + 1) AND 255) * 256
        IF fn_len > ls_max - 1 THEN fn_len = ls_max - 1
        FOR fn_i = 0 TO fn_len - 1
            POKE (#fn_src + fn_i), PEEK(FN_RX + 2 + fn_i) AND 255
        NEXT fn_i
        POKE (#fn_src + fn_len), 0
    ELSE
        fn_len = 0
    END IF
END

' Writes fn_len bytes from #fn_src as the appkey payload.
appkey_write: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_WRITE_APPKEY
    mb_nparam = 0
    FOR fn_i = 0 TO fn_len - 1
        POKE (FN_TX + fn_i), PEEK(#fn_src + fn_i) AND 255
    NEXT fn_i
    #fn_txlen = fn_len
    GOSUB fn_transact
END

appkey_close: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_CLOSE_APPKEY
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' fj_read_host_slots / fj_write_host_slots / fj_mount_host /
' fj_set_device_fullpath: the Fuji-device subset st_boot.bas needs. Payload
' sizes verified against fujinet-firmware's rs232 device handlers, per
' fujinet-config/intv/fujicmd.bas's header comment (transaction_get() fails
' if fewer than the expected byte count arrives, so SET_DEVICE_FULLPATH
' must be sent as a full 256-byte NUL-padded payload).
' ---------------------------------------------------------------------------
DIM fc_hs, fc_ds, fc_mode, fc_i

fj_read_host_slots: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_READ_HOST_SLOTS
    mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok THEN
        FOR fc_i = 0 TO 255
            POKE (SC_HOSTS + fc_i), PEEK(FN_RX + fc_i) AND 255
        NEXT fc_i
    END IF
END

fj_write_host_slots: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_WRITE_HOST_SLOTS
    mb_nparam = 0
    FOR fc_i = 0 TO 255
        POKE (FN_TX + fc_i), PEEK(SC_HOSTS + fc_i) AND 255
    NEXT fc_i
    #fn_txlen = 256
    GOSUB fn_transact
END

fj_mount_host: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_MOUNT_HOST
    mb_nparam = 1
    pm_i = 0 : pm_size = 1 : #pm_val = fc_hs : GOSUB fn_param
    #fn_txlen = 0
    GOSUB fn_transact
END

fj_set_device_fullpath: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_SET_DEVICE_FULLPATH
    mb_nparam = 3
    pm_i = 0 : pm_size = 1 : #pm_val = fc_ds   : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = fc_hs   : GOSUB fn_param
    pm_i = 2 : pm_size = 1 : #pm_val = fc_mode : GOSUB fn_param

    ls_max = 255 : GOSUB fn_strlen
    fn_len = fn_len + 1
    #fn_txlen = 0
    GOSUB fn_putstr

    FOR fc_i = #fn_txlen TO 255
        POKE (FN_TX + fc_i), 0
    NEXT fc_i
    #fn_txlen = 256

    GOSUB fn_transact
END

' ---------------------------------------------------------------------------
' QR code encoding.
'
' The FujiNet does the encoding; we hand it a string, ask for version 1, and
' read back the module matrix. Version 1 is 21x21, which is the largest symbol
' that fits the colored-squares grid at one module per square -- version 2 is
' 25x25 and will not fit at any placement, so the version is pinned rather than
' left to the firmware to choose.
'
' Unlike SIO, this bus carries as many parameters as the descriptor declares,
' so ENCODE's three arguments all go on the wire normally.
' ---------------------------------------------------------------------------

DIM #qr_len

' qr_input: append fn_len bytes at #fn_src to the encoder's input buffer.
qr_input: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_QRCODE_INPUT
    mb_nparam = 1
    pm_i = 0 : pm_size = 2 : #pm_val = fn_len : GOSUB fn_param

    #fn_txlen = 0
    GOSUB fn_putstr

    GOSUB fn_transact
END

' qr_encode_v1: encode what was staged, as version 1 / ECC LOW, no url
' shortening (the shortener returns a LAN-local address, which is no use to a
' phone that is not on the same network).
qr_encode_v1: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_QRCODE_ENCODE
    mb_nparam = 3
    pm_i = 0 : pm_size = 1 : #pm_val = 1 : GOSUB fn_param   ' version 1
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param   ' ECC LOW
    pm_i = 2 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param   ' no shortening

    #fn_txlen = 0
    GOSUB fn_transact
END

' qr_length: select the output format and read back its size into #qr_len.
' Format 0 is the raw binary matrix. Selecting a format re-renders the symbol
' that is already encoded, so this stays valid after ENCODE cleared the input.
qr_length: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_QRCODE_LENGTH
    mb_nparam = 1
    pm_i = 0 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param   ' binary output

    #fn_txlen = 0
    GOSUB fn_transact

    IF fn_ok THEN
        #qr_len = (PEEK(FN_RX) AND 255) + (PEEK(FN_RX + 1) AND 255) * 256
    ELSE
        #qr_len = 0
    END IF
END

' qr_output: read #qr_len bytes of encoded output into SC_QRRAW.
'
' A version 1 symbol is 57 bytes, comfortably inside FN_RX's 512-byte window,
' so this never needs chunking. It is destructive on the FujiNet side -- the
' bytes are erased as they are sent -- so it must not be retried.
qr_output: PROCEDURE
    mb_dev = FUJI_DEVICEID
    mb_cmd = FUJICMD_QRCODE_OUTPUT
    mb_nparam = 1
    pm_i = 0 : pm_size = 2 : #pm_val = #qr_len : GOSUB fn_param

    #fn_txlen = 0
    GOSUB fn_transact

    IF fn_ok THEN
        FOR fn_i = 0 TO #qr_len - 1
            POKE (SC_QRRAW + fn_i), PEEK(FN_RX + fn_i) AND 255
        NEXT fn_i
    END IF
END
