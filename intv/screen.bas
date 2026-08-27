' screen.bas -- low-level text drawing helpers shared by every screen.
'
' Character encoding follows the standard IntyBASIC card formula used
' throughout the FujiNet Intellivision tree (fujitest.bas, 5cardstud):
'     card = ascii - 32   (clamped to a printable placeholder if out of range)
'     screen word = card*8 + color
' Nothing here ever buffers a whole string in an IntyBASIC variable -- text
' is always drawn straight from a source address (ROM DATA or scratch RAM)
' via PEEK, one character at a time, per the RAM budget rule in constants.bas.

    CONST COL_NORMAL   = CS_WHITE
    CONST COL_HILIGHT  = CS_YELLOW
    CONST COL_DIM      = CS_BLUE
    CONST COL_ERROR    = CS_RED
    CONST COL_VALUE    = CS_TAN
    CONST COL_CURSOR   = CS_GREEN
    CONST COL_HEADER   = CS_YELLOW  ' game-group header rows, on dark green
    CONST COL_SELECT   = CS_BLACK   ' selected room row, on the tan bar

    DIM s_row, s_col, s_i, s_c, s_len, s_max, s_col_color
    DIM #s_src, #s_val

' ---------------------------------------------------------------------------
' scr_clear: blank the whole 20x12 screen.
' ---------------------------------------------------------------------------
scr_clear: PROCEDURE
    CLS
END

' ---------------------------------------------------------------------------
' scr_row_clear: blank row s_row (all 20 columns).
' ---------------------------------------------------------------------------
scr_row_clear: PROCEDURE
    FOR s_i = 0 TO SCREEN_COLS - 1
        #BACKTAB(s_row * SCREEN_COLS + s_i) = CS_BLACK
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_puts: draw bytes from #s_src onto row s_row starting at column s_col,
' in color s_col_color, stopping at the first NUL or after s_max characters,
' then space-padding the remainder of the s_max-wide field. Sets s_len to
' the number of real (non-pad) characters drawn. Caller ensures
' s_col + s_max <= SCREEN_COLS.
' ---------------------------------------------------------------------------
scr_puts: PROCEDURE
    s_len = 0
    WHILE (s_len < s_max) AND ((PEEK(#s_src + s_len) AND 255) <> 0)
        s_len = s_len + 1
    WEND
    FOR s_i = 0 TO s_max - 1
        IF s_i < s_len THEN
            s_c = PEEK(#s_src + s_i) AND 255
        ELSE
            s_c = 32
        END IF
        IF s_c < 32 OR s_c > 126 THEN s_c = 32
        #BACKTAB(s_row * SCREEN_COLS + s_col + s_i) = (s_c - 32) * 8 + s_col_color
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_dec: print unsigned #s_val at (s_col,s_row) as decimal, in s_col_color.
' ---------------------------------------------------------------------------
scr_dec: PROCEDURE
    PRINT AT screenpos(s_col, s_row) COLOR s_col_color, <>#s_val
END

' ---------------------------------------------------------------------------
' scr_recolor: change only the COLOR of s_max already-drawn characters on
' row s_row starting at column s_col, to s_col_color -- the card (glyph)
' underneath is left untouched. Used to re-highlight a cursor row without
' redrawing its text, which for a directory listing would mean re-fetching
' the filename from the mailbox (never cached, per the RAM budget rule).
' ---------------------------------------------------------------------------
scr_recolor: PROCEDURE
    FOR s_i = 0 TO s_max - 1
        #s_val = (#BACKTAB(s_row * SCREEN_COLS + s_col + s_i) AND $FFF8) + s_col_color
        #BACKTAB(s_row * SCREEN_COLS + s_col + s_i) = #s_val
    NEXT s_i
END

' ---------------------------------------------------------------------------
' scr_video_list: program the color stack for the list screen (st_list.bas's
' lb_apply_stack explains the run-by-run reasoning). Position 0 is BLUE on
' purpose: every other screen starts with scr_clear, which zeroes every BACKTAB
' word including the advance bit, so with no advances anywhere they sit
' entirely on p0.
'
' Shared by two callers rather than inlined, so they can't drift: lobby.bas at
' boot, and st_name.bas's lb_edit_name restoring this palette after
' input.bas's grid_video swapped in the character grid's own.
'
' The WAIT is load-bearing. MODE packs its four colours into IntyBASIC's
' _color variable and flags _mode_select; the ISR consumes that on the next
' frame and resets _color to 7. Any PRINT ... COLOR in between would overwrite
' the packed word and bring the stack up as garbage.
' ---------------------------------------------------------------------------
scr_video_list: PROCEDURE
    MODE 0, CS_BLUE, CS_DARKGREEN, CS_TAN, CS_DARKGREEN
    BORDER CS_BLUE
    WAIT
END

' ---------------------------------------------------------------------------
' The program's only GRAM card. Intellivision GRAM is 8x8, one byte per row,
' MSB leftmost.
'
' Every pixel is "on", which is what makes it useful twice over: input.bas
' parks it behind the character grid's selected cell as a MOB, where it fills
' the whole card except where the glyph's own foreground pixels win (real
' inverse video), and st_boot.bas draws it into BACKTAB directly so the
' progress bar reads as one unbroken run with no gaps between cells.
'
' Safe to sit here as raw data: lobby.bas's GOTO lb_boot_start jumps clear of
' every INCLUDE, so straight-line execution never falls into it.
' ---------------------------------------------------------------------------
lit_glyphs:
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"
    BITMAP "########"

' ---------------------------------------------------------------------------
' scr_define_glyphs: upload the card to GRAM. Called once from lb_boot_start
' before anything draws -- until the DEFINE lands, GRAM holds whatever the
' EXEC left there. DEFINE takes effect on the NEXT frame, hence the WAIT.
' ---------------------------------------------------------------------------
scr_define_glyphs: PROCEDURE
    DEFINE GLYPH_BLOCK, 1, lit_glyphs
    WAIT
END
