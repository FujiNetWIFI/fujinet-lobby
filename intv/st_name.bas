' st_name.bas -- shared FujiNet lobby username (appkey creator=1/app=1/
' key=0), same slot the C clients and every IntyBASIC game client use.
' Ported from fujinet-5cardstud/intv/5card.bas's boot-time name resolution
' and name_entry_screen, but using fujinet-config/intv/input.bas's
' grid_entry instead of a disc letter-picker -- grid_entry lets the same
' code serve both the initial prompt and later re-edits from the list
' screen (keypad 1), and validate/copy-back on accept only, matching
' st_hosts.bas's hosts_edit_selected pattern for a shared/persisted value.

    DIM nm_ok, nm_i, nm_j, nm_char, nm_try

' ---------------------------------------------------------------------------
' lb_resolve_name: called once at boot. Tries the shared appkey (with one
' retry -- a cold RP2040/ESP32 link right after the mailbox comes up can
' time out the very first transaction), validates it's 2-8 chars of
' A-Z0-9 (the slot is shared by every FujiNet client, so it can hold junk
' left by something else, and an unescaped character would corrupt the
' query string), and falls back to the entry grid if empty/invalid.
' ---------------------------------------------------------------------------
lb_resolve_name: PROCEDURE
    nm_ok = 0
    FOR nm_try = 0 TO 1
        ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1 : ak_key = 0 : ak_mode = 0
        GOSUB appkey_open
        IF fn_ok THEN EXIT FOR
    NEXT nm_try

    IF fn_ok THEN
        #fn_src = SC_NAME : ls_max = 9 : GOSUB appkey_read
        GOSUB appkey_close
        IF fn_len >= 2 THEN
            nm_ok = 1
            FOR nm_j = 0 TO fn_len - 1
                nm_char = PEEK(SC_NAME + nm_j) AND 255
                IF nm_char >= 97 AND nm_char <= 122 THEN nm_char = nm_char - 32
                IF (nm_char < 65 OR nm_char > 90) AND (nm_char < 48 OR nm_char > 57) THEN nm_ok = 0
            NEXT nm_j
        END IF
    END IF

    IF nm_ok = 0 THEN
        POKE (SC_NAME), 0
        GOSUB lb_edit_name
    END IF
END

' ---------------------------------------------------------------------------
' lb_edit_name: grid_entry on a scratch copy (SC_EDIT), pre-loaded with
' whatever's already in SC_NAME so re-editing edits rather than retypes.
' Only copies back to SC_NAME and writes the appkey on accept (fn_ok=1);
' a cancel (ESC) leaves the stored name untouched.
' ---------------------------------------------------------------------------
lb_edit_name: PROCEDURE
    FOR nm_i = 0 TO 8
        POKE (SC_EDIT + nm_i), PEEK(SC_NAME + nm_i) AND 255
    NEXT nm_i

    GOSUB scr_clear
    PRINT AT screenpos(0,0) COLOR COL_NORMAL,"ENTER YOUR NAME"
    #ge_dst = SC_EDIT : g_max = 9
    GOSUB grid_entry

    IF fn_ok = 1 AND g_len >= 2 THEN
        FOR nm_i = 0 TO 8
            POKE (SC_NAME + nm_i), PEEK(SC_EDIT + nm_i) AND 255
        NEXT nm_i

        ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1 : ak_key = 0 : ak_mode = 1
        GOSUB appkey_open
        IF fn_ok THEN
            #fn_src = SC_NAME : fn_len = g_len
            GOSUB appkey_write
            GOSUB appkey_close
        END IF
    ELSEIF (PEEK(SC_NAME) AND 255) = 0 THEN
        ' No stored name and the user cancelled out with nothing typed --
        ' fall back to a fixed placeholder so the rest of the program
        ' always has *something* NUL-terminated at SC_NAME to draw and to
        ' put in the URL's query string.
        POKE (SC_NAME + 0), 73  ' 'I'
        POKE (SC_NAME + 1), 78  ' 'N'
        POKE (SC_NAME + 2), 84  ' 'T'
        POKE (SC_NAME + 3), 86  ' 'V'
        POKE (SC_NAME + 4), 0
    END IF
END
