' st_list.bas -- ST_LIST: fetch a page of the lobby's binary server list,
' render it as game-header + room rows (matching the C clients' actual
' layout -- rooms, not games, are what's selectable; see the Atari
' screenshot this was ported from), page with the keypad, and hand off to
' ST_BOOT on selection.
'
' Wire protocol (clients/src/main.c): N:<endpoint>?bin=1&platform=intv&
' pagesize=N&offset=M, opened HTTP GET. Reply is a 3-byte header
' (server_count, 2 reserved) then server_count x 189-byte records
' (constants.bas's REC_* offsets). FN_RX is only 512 bytes, far smaller
' than a 9-record page (1704 bytes), so the fetch below reads one record
' at a time via fujinet.bas's net_open_settled/net_read rather than the
' single-shot api_call every other IntyBASIC FujiNet client uses.

    DIM list_shown, num_rows, sel_rec
    DIM lb_i, lb_j, lb_c, lb_row, lb_color
    DIM lb_same, lb_pv, lb_mv, lb_len
    DIM #lb_addr, #lb_paddr
    DIM pn_val, pn_h, pn_t, pn_o, pn_started
    DIM #dp_start, pstk_depth

' ---------------------------------------------------------------------------
' URL literals (ASCII DATA). IntyBASIC has no string constants; a VARPTR
' onto a DATA label is how fn_putstr gets a ROM source address.
' ---------------------------------------------------------------------------
lit_n_https: DATA 78,58,104,116,116,112,115,58,47,47
lit_host: DATA 108,111,98,98,121,46,102,117,106,105,110,101,116,46,111,110,108,105,110,101
lit_path: DATA 47,118,105,101,119,63,98,105,110,61,49,38,112,108,97,116,102,111,114,109,61,105,110,116,118
lit_pagesize: DATA 38,112,97,103,101,115,105,122,101,61
lit_offset: DATA 38,111,102,102,115,101,116,61

    CONST LEN_N_HTTPS = 10
    CONST LEN_HOST    = 20
    CONST LEN_PATH    = 25
    CONST LEN_PAGESIZE = 10
    CONST LEN_OFFSET   = 8

' ---------------------------------------------------------------------------
' compose_lobby_url: build "N:https://lobby.fujinet.online/view?bin=1&
' platform=intv&pagesize=9&offset=<#dp_start>" into FN_TX.
' ---------------------------------------------------------------------------
compose_lobby_url: PROCEDURE
    #fn_txlen = 0
    #fn_src = VARPTR lit_n_https(0) : fn_len = LEN_N_HTTPS : GOSUB fn_putstr
    #fn_src = VARPTR lit_host(0) : fn_len = LEN_HOST : GOSUB fn_putstr
    #fn_src = VARPTR lit_path(0) : fn_len = LEN_PATH : GOSUB fn_putstr
    #fn_src = VARPTR lit_pagesize(0) : fn_len = LEN_PAGESIZE : GOSUB fn_putstr
    pn_val = ENTRIES_PER_PAGE : GOSUB fn_putnum
    #fn_src = VARPTR lit_offset(0) : fn_len = LEN_OFFSET : GOSUB fn_putstr
    pn_val = #dp_start : GOSUB fn_putnum
END

' ---------------------------------------------------------------------------
' fn_putnum: append the decimal (no leading zeros) representation of pn_val
' (0-999) into FN_TX at the current #fn_txlen. IntyBASIC has no built-in
' itoa; matches fujinet-fujitzee/intv/fujitzee.bas's version. #dp_start can
' exceed 255 (it's a #-prefixed 16-bit offset), so this needs 3 digits,
' same as the game clients' copy.
' ---------------------------------------------------------------------------
fn_putnum: PROCEDURE
    pn_h = pn_val / 100
    pn_t = (pn_val / 10) % 10
    pn_o = pn_val % 10
    pn_started = 0
    IF pn_h > 0 THEN
        POKE (FN_TX + #fn_txlen), pn_h + 48 : #fn_txlen = #fn_txlen + 1
        pn_started = 1
    END IF
    IF pn_t > 0 OR pn_started THEN
        POKE (FN_TX + #fn_txlen), pn_t + 48 : #fn_txlen = #fn_txlen + 1
    END IF
    POKE (FN_TX + #fn_txlen), pn_o + 48 : #fn_txlen = #fn_txlen + 1
END

' ---------------------------------------------------------------------------
' Pagination, config's page-start-stack method (fujinet-config/intv/
' st_file.bas's sf_push_pstk/sf_pop_pstk), with #dp_start standing in for
' the lobby's `offset` query param instead of a directory position.
' ---------------------------------------------------------------------------
lb_push_pstk: PROCEDURE
    IF pstk_depth < 10 THEN
        POKE (SC_PSTK + pstk_depth * 2), #dp_start AND 255
        POKE (SC_PSTK + pstk_depth * 2 + 1), (#dp_start / 256) AND 255
        pstk_depth = pstk_depth + 1
    END IF
END

lb_pop_pstk: PROCEDURE
    pstk_depth = pstk_depth - 1
    #dp_start = (PEEK(SC_PSTK + pstk_depth * 2) AND 255) + (PEEK(SC_PSTK + pstk_depth * 2 + 1) AND 255) * 256
END

' ---------------------------------------------------------------------------
' do_list: entry point, dispatched from the main state machine.
' ---------------------------------------------------------------------------
do_list: PROCEDURE
    IF list_shown = 0 THEN
        GOSUB lb_fetch_page
        GOSUB lb_render_list
        list_shown = 1
    END IF

    GOSUB in_poll

    IF in_disc = DISC_UP AND sel_rec > 0 THEN GOSUB lb_move_up
    IF in_disc = DISC_DOWN AND sel_rec < num_rows - 1 THEN GOSUB lb_move_down

    IF in_key = KEYPAD_1 THEN
        GOSUB lb_edit_name
        list_shown = 0
        RETURN
    END IF
    IF in_key = KEYPAD_2 THEN
        IF pstk_depth > 0 THEN
            GOSUB lb_pop_pstk
            list_shown = 0
        END IF
        RETURN
    END IF
    IF in_key = KEYPAD_3 THEN
        IF num_rows >= ENTRIES_PER_PAGE THEN
            GOSUB lb_push_pstk
            #dp_start = #dp_start + num_rows
            list_shown = 0
        END IF
        RETURN
    END IF
    IF in_key = KEYPAD_9 THEN
        list_shown = 0
        RETURN
    END IF

    IF in_btn <> 0 AND num_rows > 0 THEN
        state = ST_BOOT
        RETURN
    END IF

    IF num_rows > 0 THEN GOSUB scroll_step
END

' ---------------------------------------------------------------------------
' lb_fetch_page: open the lobby URL, settle-poll, then read the page a
' record at a time (one net_read per 189-byte record -- well under FN_RX's
' 512-byte ceiling, so no cursor arithmetic across chunks is needed). A
' short read on any record means the reply was truncated; stop there and
' keep whatever full records already landed, exactly like the C client's
' truncation-in-place in display_servers(). Records get folded to display
' case is NOT done here -- REC_GAME/REC_SERVER stay byte-verbatim (scr_puts
' already clamps to a safe glyph range; folding would touch REC_SERVERURL/
' REC_CLIENTURL too if done blindly, and those are case-sensitive paths).
' ---------------------------------------------------------------------------
lb_fetch_page: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0,0) COLOR COL_DIM,"LOADING..."

    GOSUB compose_lobby_url
    GOSUB net_open_settled
    IF fn_ok = 0 THEN
        num_rows = 0
        RETURN
    END IF

    #net_readlen = 3
    GOSUB net_read
    IF fn_ok = 0 OR #net_gotlen < 3 THEN
        GOSUB net_close
        num_rows = 0
        RETURN
    END IF

    num_rows = PEEK(FN_RX) AND 255
    IF num_rows > ENTRIES_PER_PAGE THEN num_rows = ENTRIES_PER_PAGE

    FOR lb_i = 0 TO num_rows - 1
        #net_readlen = REC_STRIDE
        GOSUB net_read
        IF fn_ok = 0 OR #net_gotlen < REC_STRIDE THEN
            num_rows = lb_i
            EXIT FOR
        END IF
        #lb_addr = SC_RECS + lb_i * REC_STRIDE
        FOR lb_j = 0 TO REC_STRIDE - 1
            POKE (#lb_addr + lb_j), PEEK(FN_RX + lb_j) AND 255
        NEXT lb_j
    NEXT lb_i

    GOSUB net_close

    ' Overshoot: paged past the end via NEXT and got nothing back. Undo by
    ' popping the page-start stack and retrying, exactly like config's
    ' sf_display does for the same situation.
    IF num_rows = 0 AND pstk_depth > 0 THEN
        GOSUB lb_pop_pstk
        GOSUB lb_fetch_page
    END IF
END

' ---------------------------------------------------------------------------
' lb_render_list: full redraw. Row 0 is the title + right-flushed username.
' Rows 1-10 hold game headers and room rows; a header is emitted whenever
' record i's 17-byte game field differs from record i-1's (the server
' pre-sorts by game, so one back-comparison suffices). If the next row
' would exceed row 10, rendering stops and num_rows is truncated to the
' count of records actually drawn -- the same in-place truncation the C
' client's display_servers() does, and what makes the next-page offset
' arithmetic (#dp_start + num_rows) correct afterward.
' ---------------------------------------------------------------------------
lb_render_list: PROCEDURE
    GOSUB scr_clear

    s_row = 0 : s_col = 0 : s_max = 14 : s_col_color = COL_NORMAL
    #s_src = VARPTR lit_title(0) : GOSUB scr_puts
    s_row = 0 : s_col = 14 : s_max = 6 : s_col_color = COL_VALUE
    #s_src = SC_NAME : GOSUB scr_puts

    lb_row = LIST_START_ROW

    IF num_rows = 0 THEN
        PRINT AT screenpos(0,LIST_START_ROW) COLOR COL_DIM,"NO SERVERS ONLINE"
    ELSE
        FOR lb_i = 0 TO num_rows - 1
            #lb_addr = SC_RECS + lb_i * REC_STRIDE

            lb_same = 0
            IF lb_i > 0 THEN GOSUB lb_same_game
            IF lb_i = 0 OR lb_same = 0 THEN
                IF lb_row > LIST_LAST_ROW THEN
                    num_rows = lb_i
                    EXIT FOR
                END IF
                s_row = lb_row : s_col = 0 : s_max = SCREEN_COLS : s_col_color = COL_VALUE
                #s_src = #lb_addr + REC_GAME : GOSUB scr_puts
                lb_row = lb_row + 1
            END IF

            IF lb_row > LIST_LAST_ROW THEN
                num_rows = lb_i
                EXIT FOR
            END IF

            POKE (SC_EROW + lb_i), lb_row
            lb_color = COL_NORMAL
            IF lb_i = sel_rec THEN lb_color = COL_HILIGHT
            #BACKTAB(screenpos(0, lb_row)) = (32 - 32) * 8 + lb_color
            IF lb_i = sel_rec THEN #BACKTAB(screenpos(0, lb_row)) = (62 - 32) * 8 + lb_color
            s_row = lb_row : s_col = 1 : s_max = 14 : s_col_color = lb_color
            #s_src = #lb_addr + REC_SERVER : GOSUB scr_puts
            GOSUB lb_draw_players
            lb_row = lb_row + 1
        NEXT lb_i

        IF sel_rec >= num_rows THEN sel_rec = 0
        IF num_rows > 0 THEN
            sc_row = PEEK(SC_EROW + sel_rec) AND 255
            sc_col = 1 : sc_max = 14 : sc_color = COL_HILIGHT
            sc_active = 0 : sc_idle = 0
        END IF
    END IF

    PRINT AT screenpos(0,LEGEND_ROW) COLOR COL_DIM,"PLAY 2/3=PG 1=N 9=R "
END

lit_title: DATA 76,79,66,66,89

' lb_same_game: sets lb_same = 1 if record lb_i's REC_GAME matches record
' (lb_i-1)'s, byte-for-byte over the full 17-byte field.
lb_same_game: PROCEDURE
    lb_same = 1
    FOR lb_j = 0 TO 16
        IF (PEEK(#lb_addr + REC_GAME + lb_j) AND 255) <> (PEEK(#lb_addr - REC_STRIDE + REC_GAME + lb_j) AND 255) THEN lb_same = 0
    NEXT lb_j
END

' lb_draw_players: "<players>/<max>" right-flushed into cols 15-19 (5 wide).
' Clamped to 0-99 each so the worst case ("99/99", 5 chars) always fits.
lb_draw_players: PROCEDURE
    lb_pv = PEEK(#lb_addr + REC_PLAYERS) AND 255
    IF lb_pv > 99 THEN lb_pv = 99
    lb_mv = PEEK(#lb_addr + REC_MAXPLAYERS) AND 255
    IF lb_mv > 99 THEN lb_mv = 99

    lb_len = 0
    IF lb_pv >= 10 THEN
        POKE (SC_EDIT + lb_len), lb_pv / 10 + 48 : lb_len = lb_len + 1
    END IF
    POKE (SC_EDIT + lb_len), lb_pv % 10 + 48 : lb_len = lb_len + 1
    POKE (SC_EDIT + lb_len), 47 : lb_len = lb_len + 1  ' '/'
    IF lb_mv >= 10 THEN
        POKE (SC_EDIT + lb_len), lb_mv / 10 + 48 : lb_len = lb_len + 1
    END IF
    POKE (SC_EDIT + lb_len), lb_mv % 10 + 48 : lb_len = lb_len + 1
    POKE (SC_EDIT + lb_len), 0

    s_row = lb_row : s_col = SCREEN_COLS - lb_len : s_max = lb_len : s_col_color = lb_color
    #s_src = SC_EDIT : GOSUB scr_puts
END

' ---------------------------------------------------------------------------
' lb_move_up / lb_move_down: recolor in place (never redraw text), matching
' fujinet-config's central cursor idiom. The '>' cursor glyph is a card
' change, not just a color change, so it's poked directly; the rest of the
' row is handled by scr_recolor.
' ---------------------------------------------------------------------------
lb_move_up: PROCEDURE
    GOSUB scroll_reset
    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(0, lb_row)) = (32 - 32) * 8 + COL_NORMAL
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_NORMAL
    GOSUB scr_recolor

    sel_rec = sel_rec - 1

    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(0, lb_row)) = (62 - 32) * 8 + COL_HILIGHT
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_HILIGHT
    GOSUB scr_recolor
    sc_row = lb_row : sc_col = 1 : sc_max = 14 : sc_color = COL_HILIGHT
END

lb_move_down: PROCEDURE
    GOSUB scroll_reset
    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(0, lb_row)) = (32 - 32) * 8 + COL_NORMAL
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_NORMAL
    GOSUB scr_recolor

    sel_rec = sel_rec + 1

    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(0, lb_row)) = (62 - 32) * 8 + COL_HILIGHT
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_HILIGHT
    GOSUB scr_recolor
    sc_row = lb_row : sc_col = 1 : sc_max = 14 : sc_color = COL_HILIGHT
END

' ---------------------------------------------------------------------------
' lb_load_roomname: scroll.bas's hook. Copies the selected record's
' REC_SERVER field (already cached in SC_RECS -- no re-fetch, unlike
' config's directory browser) into SC_ENTRY and sets sc_len.
' ---------------------------------------------------------------------------
lb_load_roomname: PROCEDURE
    #lb_addr = SC_RECS + sel_rec * REC_STRIDE
    ls_max = 33 : #fn_src = #lb_addr + REC_SERVER : GOSUB fn_strlen
    FOR lb_j = 0 TO fn_len - 1
        POKE (SC_ENTRY + lb_j), PEEK(#lb_addr + REC_SERVER + lb_j) AND 255
    NEXT lb_j
    sc_len = fn_len
END
