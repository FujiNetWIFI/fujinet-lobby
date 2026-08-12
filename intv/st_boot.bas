' st_boot.bas -- ST_BOOT: reproduce clients/src/main.c's mount() on top of
' fujinet-config/intv/st_boot.bas's SET_DEVICE_FULLPATH + MOUNT_IMAGE
' machinery (that half is copied near-verbatim; only DEVICE_SLOT/MODE_READ
' and the error-code print differ in spelling since this program has no
' fujicmd.bas of its own -- those wrappers moved into fujinet.bas).
'
' Entered with sel_rec already pointing at the chosen record in SC_RECS.
' Does the C client's mount() steps in order: strip the URL scheme, split
' host/filename, find-or-claim a host slot, mount it, stage the full path,
' write the server URL to the appkey the booted game will read, then hand
' off to MOUNT_IMAGE with its own progress bar. On any failure, falls back
' to state = ST_LIST rather than main.c's "press RETURN to continue" pause
' -- there's no keyboard to dismiss a message with, so failures are just
' shown briefly and the list resumes.

    DIM bt_hstart, bt_t, bt_seq, bt_pct, bt_lastpct
    DIM #bt_addr, bt_i, bt_j, bt_c, bt_slash, bt_found, bt_slot

do_boot: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0,0) COLOR COL_NORMAL,"MOUNTING"

    #bt_addr = SC_RECS + sel_rec * REC_STRIDE

    ' Sanity, mirroring main.c's mount(): a record with game_type=0 has
    ' nothing valid to hand off via appkey, so refuse to boot it.
    IF (PEEK(#bt_addr + REC_GAMETYPE) AND 255) = 0 THEN
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"INVALID ENTRY       "
        GOSUB boot_pause
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
    bt_slash = -1
    bt_j = bt_i
    WHILE bt_j < fn_len AND bt_slash = -1
        IF (PEEK(#bt_addr + REC_CLIENTURL + bt_j) AND 255) = 47 THEN bt_slash = bt_j
        bt_j = bt_j + 1
    WEND
    IF bt_slash = -1 THEN
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"INVALID CLIENT URL  "
        GOSUB boot_pause
        state = ST_LIST
        RETURN
    END IF

    ' Host slot: read the table, case-insensitively match the host against
    ' all 8 slots; if not found, overwrite the LAST slot (7) and write the
    ' table back -- same policy as main.c's mount().
    GOSUB fj_read_host_slots
    bt_found = 0
    FOR bt_slot = 0 TO NUM_HOST_SLOTS - 1
        GOSUB bt_slot_matches
        IF bt_c = 1 THEN
            bt_found = 1
            EXIT FOR
        END IF
    NEXT bt_slot
    IF bt_found = 0 THEN
        bt_slot = NUM_HOST_SLOTS - 1
        FOR bt_j = 0 TO bt_slash - bt_i - 1
            POKE (SC_HOSTS + bt_slot * HOST_NAME_LEN + bt_j), PEEK(#bt_addr + REC_CLIENTURL + bt_i + bt_j) AND 255
        NEXT bt_j
        POKE (SC_HOSTS + bt_slot * HOST_NAME_LEN + (bt_slash - bt_i)), 0
        GOSUB fj_write_host_slots
    END IF

    host_slot = bt_slot
    fc_hs = host_slot
    GOSUB fj_mount_host
    IF fn_ok = 0 THEN
        PRINT AT screenpos(0,11) COLOR COL_ERROR,"MOUNT FAILED        "
        GOSUB boot_pause
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

' bt_slot_matches: case-insensitive compare of SC_HOSTS slot `bt_slot`
' against the host substring [bt_i, bt_slash) of REC_CLIENTURL. Sets bt_c
' to 1/0. IntyBASIC procedures take no arguments; bt_slot/bt_i/bt_slash are
' already module globals.
bt_slot_matches: PROCEDURE
    bt_c = 1
    IF (PEEK(SC_HOSTS + bt_slot * HOST_NAME_LEN) AND 255) = 0 THEN
        bt_c = 0
        RETURN
    END IF
    FOR bt_j = 0 TO bt_slash - bt_i - 1
        bt_hstart = PEEK(SC_HOSTS + bt_slot * HOST_NAME_LEN + bt_j) AND 255
        IF bt_hstart >= 97 AND bt_hstart <= 122 THEN bt_hstart = bt_hstart - 32
        bt_pct = PEEK(#bt_addr + REC_CLIENTURL + bt_i + bt_j) AND 255
        IF bt_pct >= 97 AND bt_pct <= 122 THEN bt_pct = bt_pct - 32
        IF bt_hstart <> bt_pct THEN bt_c = 0
    NEXT bt_j
    IF (PEEK(SC_HOSTS + bt_slot * HOST_NAME_LEN + (bt_slash - bt_i)) AND 255) <> 0 THEN bt_c = 0
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
boot_draw_progress: PROCEDURE
    s_row = 5
    FOR s_i = 0 TO SCREEN_COLS - 1
        s_c = 32
        IF s_i < (bt_pct * SCREEN_COLS) / 100 THEN s_c = 35   ' '#'
        #BACKTAB(s_row * SCREEN_COLS + s_i) = (s_c - 32) * 8 + COL_HILIGHT
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
    state = ST_LIST
END
