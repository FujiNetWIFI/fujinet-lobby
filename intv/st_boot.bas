' st_boot.bas -- ST_BOOT: reproduce clients/src/main.c's mount() on top of
' fujinet-config/intv/st_boot.bas's SET_DEVICE_FULLPATH + MOUNT_IMAGE
' machinery (that half is copied near-verbatim; only DEVICE_SLOT/MODE_READ
' and the error-code print differ in spelling since this program has no
' fujicmd.bas of its own -- those wrappers moved into fujinet.bas).
'
' Entered with sel_rec already pointing at the chosen record in SC_RECS.
' Does the C client's mount() steps in order: strip the URL scheme, split
' host/filename, claim the last host slot, mount it, stage the full path,
' write the server URL to the appkey the booted game will read, then hand
' off to MOUNT_IMAGE with its own progress bar. On any failure, falls back
' to state = ST_LIST rather than main.c's "press RETURN to continue" pause
' -- there's no keyboard to dismiss a message with, so failures are just
' shown briefly and the list resumes.

    CONST BT_SLASH_NONE = 255

    DIM bt_t, bt_seq, bt_pct, bt_lastpct
    DIM #bt_addr, bt_i, bt_j, bt_c, bt_slash, bt_slot

do_boot: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0,0) COLOR COL_NORMAL,"MOUNTING"

    #bt_addr = SC_RECS + sel_rec * REC_STRIDE

    ' Sanity, mirroring main.c's mount(): a record with game_type=0 has
    ' nothing valid to hand off via appkey, so refuse to boot it.
    IF (PEEK(#bt_addr + REC_GAMETYPE) AND 255) = 0 THEN
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"INVALID ENTRY       "
        GOSUB boot_pause
        list_shown = 0
        state = ST_LIST
        RETURN
    END IF

    ' Strip a leading scheme ("tnfs://", "https://", ...) from client_url if
    ' present; assume TNFS otherwise, same as the C client.
    ls_max = 65 : #fn_src = #bt_addr + REC_CLIENTURL : GOSUB fn_strlen
    bt_i = 0
    WHILE bt_i < fn_len - 2
        bt_c = PEEK(#bt_addr + REC_CLIENTURL + bt_i) AND 255
        IF bt_c = 58 THEN   ' ':'
            IF (PEEK(#bt_addr + REC_CLIENTURL + bt_i + 1) AND 255) = 47 THEN
                IF (PEEK(#bt_addr + REC_CLIENTURL + bt_i + 2) AND 255) = 47 THEN
                    bt_i = bt_i + 3
                    EXIT WHILE
                END IF
            END IF
        END IF
        bt_i = bt_i + 1
    WEND
    IF bt_i >= fn_len - 2 THEN bt_i = 0   ' no "://" found -- use the whole field

    ' host = up to (not including) the first '/' from bt_i; filename =
    ' everything from that '/' onward (kept, with its leading '/', so
    ' SC_BOOTPATH is a proper absolute path).
    '
    ' Sentinel is 255, not -1: bt_slash is an IntyBASIC "8-bit" variable,
    ' which lives in the console's System RAM ($100-$1EF) -- physically
    ' 8 bits wide. Storing -1 ($FFFF) there only latches the low byte, so
    ' it silently reads back as 255, not -1, and every "= -1" comparison
    ' against it would then be comparing against a value that can never
    ' occur. 255 is outside any real index into a <=65-byte field, so it's
    ' safe as a sentinel that actually survives the round trip.
    bt_slash = BT_SLASH_NONE
    bt_j = bt_i
    WHILE bt_j < fn_len AND bt_slash = BT_SLASH_NONE
        IF (PEEK(#bt_addr + REC_CLIENTURL + bt_j) AND 255) = 47 THEN bt_slash = bt_j
        bt_j = bt_j + 1
    WEND
    IF bt_slash = BT_SLASH_NONE THEN
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"INVALID CLIENT URL  "
        GOSUB boot_pause
        list_shown = 0
        state = ST_LIST
        RETURN
    END IF

    ' Host slot: always the very last slot, and always overwritten fresh --
    ' the other 7 slots on a real unit accumulate whatever other clients
    ' (or old, pre-split builds of this one) have left in them, so a
    ' case-insensitive "does it already match" check could get fooled by
    ' leftover garbage and skip writing the real value. fj_read_host_slots
    ' still runs first so the WRITE_HOST_SLOTS payload preserves the other
    ' 7 slots untouched.
    GOSUB fj_read_host_slots
    bt_slot = NUM_HOST_SLOTS - 1
    FOR bt_j = 0 TO bt_slash - bt_i - 1
        POKE (SC_HOSTS + bt_slot * HOST_NAME_LEN + bt_j), PEEK(#bt_addr + REC_CLIENTURL + bt_i + bt_j) AND 255
    NEXT bt_j
    POKE (SC_HOSTS + bt_slot * HOST_NAME_LEN + (bt_slash - bt_i)), 0
    GOSUB fj_write_host_slots

    host_slot = bt_slot
    fc_hs = host_slot
    GOSUB fj_mount_host
    IF fn_ok = 0 THEN
        PRINT AT screenpos(0,8) COLOR COL_DIM,"ERR CODE:"
        #s_val = #mb_err
        PRINT AT screenpos(10,8) COLOR COL_VALUE,<.3>#s_val
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"MOUNT FAILED        "
        GOSUB boot_pause
        list_shown = 0
        state = ST_LIST
        RETURN
    END IF

    ' SC_BOOTPATH = filename, starting at the '/' found above.
    fn_len = fn_len - bt_slash
    FOR bt_j = 0 TO fn_len - 1
        POKE (SC_BOOTPATH + bt_j), PEEK(#bt_addr + REC_CLIENTURL + bt_slash + bt_j) AND 255
    NEXT bt_j
    POKE (SC_BOOTPATH + fn_len), 0

    ' Write the server URL to appkey key_id = game_type -- the whole point
    ' of the handoff: the booted game reads this to know which server to
    ' connect to. Creator/app match the shared username slot (1/1); the
    ' key_id is per-game, taken straight off the wire record.
    ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1
    ak_key = PEEK(#bt_addr + REC_GAMETYPE) AND 255
    ak_mode = 1
    GOSUB appkey_open
    IF fn_ok THEN
        ls_max = 65 : #fn_src = #bt_addr + REC_SERVERURL : GOSUB fn_strlen
        #fn_src = #bt_addr + REC_SERVERURL
        GOSUB appkey_write
        GOSUB appkey_close
    END IF

    s_row = 2 : s_col = 0 : s_max = SCREEN_COLS : s_col_color = COL_VALUE
    #s_src = SC_BOOTPATH : GOSUB scr_puts
    PRINT AT screenpos(0,11) COLOR COL_DIM,"DO NOT POWER OFF"

    fc_ds = DEVICE_SLOT : fc_hs = host_slot : fc_mode = MODE_READ
    #fn_src = SC_BOOTPATH
    GOSUB fj_set_device_fullpath
    IF fn_ok = 0 THEN
        GOSUB boot_fail
        RETURN
    END IF

    GOSUB boot_mount_with_progress

    ' Only reached on failure/timeout -- success resets the console.
    GOSUB boot_fail
END

boot_pause: PROCEDURE
    FOR bt_t = 0 TO 89
        WAIT
    NEXT bt_t
END

' ---------------------------------------------------------------------------
' boot_mount_with_progress: MOUNT_IMAGE(DEVICE_SLOT, MODE_READ), no TX
' payload, with a long timeout instead of fn_transact's 900 frames, and a
' progress-bar redraw whenever FN_BOOT_PCT changes. Verbatim from
' fujinet-config/intv/st_boot.bas.
' ---------------------------------------------------------------------------
    CONST BOOT_TIMEOUT_FRAMES = 3600   ' ~60s at 60Hz

boot_mount_with_progress: PROCEDURE
    POKE (FN_DEVICE), FUJI_DEVICEID
    POKE (FN_CMD), FUJICMD_MOUNT_IMAGE
    POKE (FN_NPARAM), 2
    pm_i = 0 : pm_size = 1 : #pm_val = DEVICE_SLOT : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = MODE_READ : GOSUB fn_param
    POKE (FN_TXLEN_LO), 0
    POKE (FN_TXLEN_HI), 0

    bt_seq = (PEEK(FN_ACKSEQ) AND 255) + 1
    IF bt_seq = 0 THEN bt_seq = 1
    POKE (FN_SEQ), bt_seq

    bt_lastpct = 255
    bt_t = 0
    WHILE ((PEEK(FN_ACKSEQ) AND 255) <> bt_seq) AND (bt_t < BOOT_TIMEOUT_FRAMES)
        WAIT
        bt_t = bt_t + 1
        bt_pct = PEEK(FN_BOOT_PCT) AND 255
        IF bt_pct <> bt_lastpct THEN
            GOSUB boot_draw_progress
            bt_lastpct = bt_pct
        END IF
    WEND

    IF bt_t >= BOOT_TIMEOUT_FRAMES THEN
        fn_ok = 0
        #mb_err = 0
        RETURN
    END IF

    IF (PEEK(FN_REPLY_CMD) AND 255) <> FUJICMD_ACK THEN
        fn_ok = 0
        #mb_err = PEEK(FN_ERR) AND 255
        RETURN
    END IF

    ' mb_err = 0xEE here is jzIntv's emulator saying "the push worked, but I
    ' can't actually reboot the cart" -- expected on jzIntv, not a failure
    ' on real hardware. Every other value is a genuine mapping/decode error.
    IF (PEEK(FN_BOOT_STATE) AND 255) = FUJI_BOOT_FAILED THEN
        fn_ok = 0
        #mb_err = PEEK(FN_BOOT_ERR) AND 255
        RETURN
    END IF

    fn_ok = 1
END

' boot_draw_progress: 20-cell bar on row 5, percentage on row 6.
'
' Filled cells are the solid GRAM block (constants.bas's GLYPH_BLOCK, uploaded
' at lb_boot_start and shared with the character grid's cursor) rather than the
' '#' this used to draw, so the bar reads as one unbroken run with no gaps
' between cells. A color stack GRAM cell is card*8 + GRAM_SELECT + foreground.
boot_draw_progress: PROCEDURE
    s_row = 5
    FOR s_i = 0 TO SCREEN_COLS - 1
        IF s_i < (bt_pct * SCREEN_COLS) / 100 THEN
            #BACKTAB(s_row * SCREEN_COLS + s_i) = GLYPH_BLOCK * 8 + GRAM_SELECT + COL_HILIGHT
        ELSE
            #BACKTAB(s_row * SCREEN_COLS + s_i) = CS_BLACK   ' card 0 = blank
        END IF
    NEXT s_i

    s_row = 6 : GOSUB scr_row_clear
    #s_val = bt_pct
    PRINT AT screenpos(8,6) COLOR COL_VALUE,<.3>#s_val
    PRINT AT screenpos(11,6) COLOR COL_VALUE,"%"
END

boot_fail: PROCEDURE
    PRINT AT screenpos(0,8) COLOR COL_DIM,"ERR CODE:"
    #s_val = #mb_err
    PRINT AT screenpos(10,8) COLOR COL_VALUE,<.3>#s_val
    PRINT AT screenpos(0,11) COLOR COL_ERROR,"BOOT FAILED         "
    FOR bt_t = 0 TO 119
        WAIT
    NEXT bt_t
    list_shown = 0
    state = ST_LIST
END
