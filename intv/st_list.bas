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
    DIM lb_full
    DIM lb_total, lb_pages, lb_qsize
    DIM #lb_qoff
    DIM cp_i, cp_j, cp_row, cp_drawn, cp_same, cp_hdr, cp_fits

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
' platform=intv&pagesize=<lb_qsize>&offset=<#lb_qoff>" into FN_TX. Both are
' set by the caller -- lb_fetch_page asks for one screenful, lb_fetch_total
' asks for everything and reads only the count byte.
' ---------------------------------------------------------------------------
compose_lobby_url: PROCEDURE
    #fn_txlen = 0
    #fn_src = VARPTR lit_n_https(0) : fn_len = LEN_N_HTTPS : GOSUB fn_putstr
    #fn_src = VARPTR lit_host(0) : fn_len = LEN_HOST : GOSUB fn_putstr
    #fn_src = VARPTR lit_path(0) : fn_len = LEN_PATH : GOSUB fn_putstr
    #fn_src = VARPTR lit_pagesize(0) : fn_len = LEN_PAGESIZE : GOSUB fn_putstr
    pn_val = lb_qsize : GOSUB fn_putnum
    #fn_src = VARPTR lit_offset(0) : fn_len = LEN_OFFSET : GOSUB fn_putstr
    pn_val = #lb_qoff : GOSUB fn_putnum
END

' ---------------------------------------------------------------------------
' lb_count_pages: work out the exact number of pages by walking the whole
' list once and replaying lb_render_list's row arithmetic over it.
'
' ceil(total / ENTRIES_PER_PAGE) does NOT work here, which is why this
' procedure is the shape it is. A page is bounded by SCREEN ROWS, not by
' record count: every change of game costs an extra row for its header, and
' a header with no room under it is deferred to the next page. So a page can
' hold anywhere from 4 to 9 records and the page count is a property of the
' game-name sequence, not of the total. Dividing undercounts -- a 4-page
' lobby reports 3.
'
' The reply carries no help either: server/actions.go's
' SerializeToBinaryFormat writes [count_in_this_page, 0, 0] and the two zeros
' are marked "reserved for future use".
'
' Cost is one extra HTTP request per full reload (not per page turn) and one
' 189-byte mailbox read per record within it. Nothing is cached -- only the
' previous record's game field is kept, in SC_PREVGAME, which is all the
' header test needs. A short read mid-walk just stops the count early; the
' running total is kept and lb_render_list's floor still applies.
' ---------------------------------------------------------------------------
lb_count_pages: PROCEDURE
    lb_total = 0 : lb_pages = 1

    lb_qsize = 255 : #lb_qoff = 0
    GOSUB compose_lobby_url
    GOSUB net_open_settled
    IF fn_ok = 0 THEN RETURN

    #net_readlen = 3
    GOSUB net_read
    IF fn_ok = 0 OR #net_gotlen < 3 THEN
        GOSUB net_close
        RETURN
    END IF

    lb_total = PEEK(FN_RX) AND 255
    IF lb_total = 0 THEN
        GOSUB net_close
        RETURN
    END IF

    cp_row = LIST_START_ROW
    cp_drawn = 0

    FOR cp_i = 0 TO lb_total - 1
        #net_readlen = REC_STRIDE
        GOSUB net_read
        IF fn_ok = 0 OR #net_gotlen < REC_STRIDE THEN EXIT FOR

        ' Header test, same as lb_same_game: the first record of a page always
        ' gets one (lb_render_list compares within the page, so a game split
        ' across a page break is re-headed), otherwise only when the game
        ' differs from the record before it.
        cp_hdr = 1
        IF cp_drawn > 0 THEN
            cp_same = 1
            FOR cp_j = 0 TO 16
                IF (PEEK(FN_RX + REC_GAME + cp_j) AND 255) <> (PEEK(SC_PREVGAME + cp_j) AND 255) THEN cp_same = 0
            NEXT cp_j
            IF cp_same THEN cp_hdr = 0
        END IF

        ' Does it still fit? These are lb_render_list's two guards verbatim: a
        ' header needs its own row AND a room row under it (>=), a bare room
        ' row needs one (>). Plus the server's own ceiling on a page.
        cp_fits = 1
        IF cp_hdr = 1 AND cp_row >= LIST_LAST_ROW THEN cp_fits = 0
        IF cp_hdr = 0 AND cp_row > LIST_LAST_ROW THEN cp_fits = 0
        IF cp_drawn >= ENTRIES_PER_PAGE THEN cp_fits = 0

        IF cp_fits = 0 THEN
            lb_pages = lb_pages + 1
            cp_row = LIST_START_ROW
            cp_drawn = 0
            cp_hdr = 1
        END IF

        IF cp_hdr = 1 THEN cp_row = cp_row + 1
        cp_row = cp_row + 1
        cp_drawn = cp_drawn + 1

        FOR cp_j = 0 TO 16
            POKE (SC_PREVGAME + cp_j), PEEK(FN_RX + REC_GAME + cp_j) AND 255
        NEXT cp_j
    NEXT cp_i

    GOSUB net_close
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
        IF lb_full = 1 THEN
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
    PRINT AT screenpos(0,0) COLOR COL_NORMAL,"LOADING..."

    ' Depth 0 means the list is being loaded from scratch -- boot, keypad 9
    ' refresh, a name edit, or paging all the way back -- so that is when the
    ' pages get re-counted, rather than on every page turn.
    IF pstk_depth = 0 THEN GOSUB lb_count_pages

    lb_qsize = ENTRIES_PER_PAGE : #lb_qoff = #dp_start
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

    ' lb_render_list truncates num_rows further to whatever fits on screen
    ' (headers eat rows too), so it can no longer tell "more pages exist"
    ' apart from "this page didn't all fit". Latch that signal here, off
    ' the server's actual reply, before render gets a chance to shrink it.
    lb_full = 0
    IF num_rows >= ENTRIES_PER_PAGE THEN lb_full = 1

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
' Rows 1-9 hold game headers and room rows; a header is emitted whenever
' record i's 17-byte game field differs from record i-1's (the server
' pre-sorts by game, so one back-comparison suffices). If the next row
' would exceed LIST_LAST_ROW, rendering stops and num_rows is truncated to
' the count of records actually drawn -- the same in-place truncation the C
' client's display_servers() does, and what makes the next-page offset
' arithmetic (#dp_start + num_rows) correct afterward. Row 10 is never
' drawn on: it is the colour stack's spacer (see lb_apply_stack).
'
' Column 0 is a dark green gutter on every row, selected or not. It carries
' no cursor glyph -- the tan bar is the selection marker -- but it is not
' decorative either: it is the run that keeps the stack legal when the top
' entry is selected, so nothing may ever be drawn into it.
' ---------------------------------------------------------------------------
lb_render_list: PROCEDURE
    GOSUB scr_clear

    s_row = 0 : s_col = 0 : s_max = 14 : s_col_color = COL_NORMAL
    #s_src = VARPTR lit_title(0) : GOSUB scr_puts
    s_row = 0 : s_col = 14 : s_max = 6 : s_col_color = COL_VALUE
    #s_src = SC_NAME : GOSUB scr_puts

    ' Page counter, over the title field's padding at columns 9-13 (the
    ' username starts at 14, and "12/29" is the widest this can get).
    ' pstk_depth counts the pages pushed to get here, so the page number is
    ' one more than it. Digits yellow, separator white, matching the command
    ' row. No zero padding needed -- the screen was just cleared, so a
    ' shrinking number can never leave a stale digit behind.
    '
    ' lb_count_pages walks the real list, so this should already be exact.
    ' The floor is here for the case where the count request failed or its
    ' walk was cut short by a truncated reply: believe the paging over a
    ' stale count rather than render "4/3".
    IF pstk_depth + 1 > lb_pages THEN lb_pages = pstk_depth + 1
    PRINT AT screenpos(9,0) COLOR COL_HILIGHT,<>(pstk_depth + 1)
    PRINT COLOR COL_NORMAL,"/"
    PRINT COLOR COL_HILIGHT,<>lb_pages

    lb_row = LIST_START_ROW

    IF num_rows = 0 THEN
        PRINT AT screenpos(0,LIST_START_ROW) COLOR COL_NORMAL,"NO SERVERS ONLINE"
    ELSE
        FOR lb_i = 0 TO num_rows - 1
            #lb_addr = SC_RECS + lb_i * REC_STRIDE

            lb_same = 0
            IF lb_i > 0 THEN GOSUB lb_same_game
            IF lb_i = 0 OR lb_same = 0 THEN
                ' A header needs its own row AND at least one room row under
                ' it. With only its own row left it would be a widow, so stop
                ' here instead and let this record open the next page, where
                ' the header gets redrawn with its rooms.
                IF lb_row >= LIST_LAST_ROW THEN
                    num_rows = lb_i
                    EXIT FOR
                END IF
                s_row = lb_row : s_col = 0 : s_max = SCREEN_COLS : s_col_color = COL_HEADER
                #s_src = #lb_addr + REC_GAME : GOSUB scr_puts
                lb_row = lb_row + 1
            END IF

            IF lb_row > LIST_LAST_ROW THEN
                num_rows = lb_i
                EXIT FOR
            END IF

            POKE (SC_EROW + lb_i), lb_row
            lb_color = COL_NORMAL
            IF lb_i = sel_rec THEN lb_color = COL_SELECT
            ' Column 0 is left exactly as scr_clear left it (card 0, a
            ' space): it is the dark green gutter, not a cursor column.
            s_row = lb_row : s_col = 1 : s_max = 14 : s_col_color = lb_color
            #s_src = #lb_addr + REC_SERVER : GOSUB scr_puts
            GOSUB lb_draw_players
            lb_row = lb_row + 1
        NEXT lb_i

        IF sel_rec >= num_rows THEN sel_rec = 0
        IF num_rows > 0 THEN
            sc_row = PEEK(SC_EROW + sel_rec) AND 255
            ' The clamp above can move sel_rec after the loop has already
            ' painted every row COL_NORMAL -- rendering truncates num_rows
            ' when headers eat rows, and paging leaves sel_rec wherever the
            ' previous page's cursor was. Recolour the selected row here
            ' unconditionally rather than tracking whether it moved; it's 19
            ' cells once per page, and lb_apply_stack is about to put the bar
            ' under it either way. Without this the bar comes up tan with
            ' white text on it.
            s_row = sc_row : s_col = 1 : s_max = 19 : s_col_color = COL_SELECT
            GOSUB scr_recolor
            sc_col = 1 : sc_max = 14 : sc_color = COL_SELECT
            sc_active = 0 : sc_idle = 0
        END IF
    END IF

    ' Command bar: keypad digits in yellow, their labels in white, 20
    ' columns exactly (2+4+1+4+1+8). PRINT keeps its cursor in _screen
    ' across statements, so only the first one needs an AT.
    PRINT AT screenpos(0,LEGEND_ROW) COLOR COL_HILIGHT,"23"
    PRINT COLOR COL_NORMAL,"PAGE"
    PRINT COLOR COL_HILIGHT,"1"
    PRINT COLOR COL_NORMAL,"NAME"
    PRINT COLOR COL_HILIGHT,"9"
    PRINT COLOR COL_NORMAL,"REFRESH "

    ' Last, always: PRINT and scr_puts both write a bare card*8+colour word
    ' and wipe bit 13, so the advance bits only survive if they go down
    ' after every other draw on this screen is finished.
    GOSUB lb_apply_stack
END

lit_title: DATA 76,79,66,66,89,0

' ---------------------------------------------------------------------------
' Colour stack management, the same method as fujinet-config/intv/csbar.bas.
'
' The stack holds four background entries, resets to position 0 at the top of
' every frame, and only advances forward, wrapping 3 -> 0. A cell with bit 13
' (CS_ADVANCE) set advances the stack and then draws with the NEW entry, so
' the cell carrying the bit is the first cell of the new run.
'
' In raster order this screen is five runs, and the fifth wraps back onto
' position 0, which is what lobby.bas programs:
'
'     MODE 0, CS_BLUE, CS_DARKGREEN, CS_TAN, CS_DARKGREEN
'              p0        p1            p2      p3
'
'     p0  row 0                    title + username
'     p1  rows 1..bar-1, and       the list above the bar
'         column 0 of the bar row
'     p2  bar row, columns 1-19    the selection bar
'     p3  rows bar+1..10           the list below the bar
'     p0  row 11 (wrapped)         command bar
'
' That only holds while BOTH dark green runs are non-empty, which is where
' the two layout rules come from: the bar spans columns 1-19 only (column 0
' is the run when the TOP entry is selected) and row 10 is a permanent blank
' spacer (the run when the BOTTOM entry is selected).
'
' lb_apply_stack stamps all four bits in one pass, which is only safe because
' lb_render_list's scr_clear zeroed every word first and lb_apply_stack runs
' after the last draw. scr_recolor masks with AND $FFF8 (screen.bas) and
' leaves bit 13 alone, so re-highlighting a row never disturbs them.
' ---------------------------------------------------------------------------
lb_apply_stack: PROCEDURE
    #BACKTAB(screenpos(0, LIST_START_ROW)) = #BACKTAB(screenpos(0, LIST_START_ROW)) OR CS_ADVANCE
    IF num_rows > 0 THEN
        GOSUB lb_bar_set
    ELSE
        ' Nothing to highlight, but the COUNT of advances matters as much as
        ' their positions: drop the bar's pair and row 11 sits on p2 -- tan --
        ' instead of wrapping onto p0. Park them on the last two cells of the
        ' blank spacer row, which costs one tan cell at (18,10) and hides the
        ' other on column 19, already dark green.
        #BACKTAB(screenpos(18, LIST_SPACER_ROW)) = #BACKTAB(screenpos(18, LIST_SPACER_ROW)) OR CS_ADVANCE
        #BACKTAB(screenpos(19, LIST_SPACER_ROW)) = #BACKTAB(screenpos(19, LIST_SPACER_ROW)) OR CS_ADVANCE
    END IF
    #BACKTAB(screenpos(0, LEGEND_ROW)) = #BACKTAB(screenpos(0, LEGEND_ROW)) OR CS_ADVANCE
END

' lb_bar_set / lb_bar_clr: add or remove the selection bar's two advance bits
' for the row SC_EROW records for the CURRENT sel_rec -- so lb_bar_clr has to
' run before sel_rec moves. The text underneath is untouched; only bit 13
' moves, so the cursor walks without anything being redrawn.
'
' lb_row + 1 is always <= LIST_SPACER_ROW because LIST_LAST_ROW is 9, which
' is the entire reason the spacer row exists.
lb_bar_set: PROCEDURE
    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(1, lb_row)) = #BACKTAB(screenpos(1, lb_row)) OR CS_ADVANCE
    #BACKTAB(screenpos(0, lb_row + 1)) = #BACKTAB(screenpos(0, lb_row + 1)) OR CS_ADVANCE
END

lb_bar_clr: PROCEDURE
    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    #BACKTAB(screenpos(1, lb_row)) = #BACKTAB(screenpos(1, lb_row)) AND $DFFF
    #BACKTAB(screenpos(0, lb_row + 1)) = #BACKTAB(screenpos(0, lb_row + 1)) AND $DFFF
END

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
' fujinet-config's central cursor idiom. Moving the bar is two advance bits
' plus a foreground swap over columns 1-19; scr_recolor preserves bit 13, so
' the two operations don't interfere.
'
' Order is load-bearing. The STIC halts the CPU for the whole active display
' and hands it back at vertical blank, so a procedure that outruns one vblank
' gets frozen mid-update and the partial screen is what gets scanned out --
' for a FULL frame, not a sliver. Between lb_bar_clr and lb_bar_set the screen
' carries two advances instead of four, which puts the command row on p2 (tan)
' and the whole list on p1, and there is no way to move the pair atomically.
' So the pair goes first, back to back, where a vblank is least likely to
' expire between them; the ~40 cells of scr_recolor that follow are safe to be
' interrupted because scr_recolor masks with AND $FFF8 and never touches bit 13.
'
' scroll_reset still has to precede lb_bar_clr: it repaints columns 1-14 of the
' row being left and scroll_draw re-arms that row's advance bit as it goes, so
' running it afterwards would leave a fifth advance behind. It is a no-op
' unless a scroll was actually running, which is never the case while the disc
' is held down -- the repeat path reaches the bit swap immediately.
' ---------------------------------------------------------------------------
lb_move_up: PROCEDURE
    GOSUB scroll_reset

    GOSUB lb_bar_clr
    sel_rec = sel_rec - 1
    GOSUB lb_bar_set

    lb_row = PEEK(SC_EROW + sel_rec + 1) AND 255
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_NORMAL
    GOSUB scr_recolor

    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_SELECT
    GOSUB scr_recolor
    sc_row = lb_row : sc_col = 1 : sc_max = 14 : sc_color = COL_SELECT
END

lb_move_down: PROCEDURE
    GOSUB scroll_reset

    GOSUB lb_bar_clr
    sel_rec = sel_rec + 1
    GOSUB lb_bar_set

    lb_row = PEEK(SC_EROW + sel_rec - 1) AND 255
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_NORMAL
    GOSUB scr_recolor

    lb_row = PEEK(SC_EROW + sel_rec) AND 255
    s_row = lb_row : s_col = 1 : s_max = 19 : s_col_color = COL_SELECT
    GOSUB scr_recolor
    sc_row = lb_row : sc_col = 1 : sc_max = 14 : sc_color = COL_SELECT
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
