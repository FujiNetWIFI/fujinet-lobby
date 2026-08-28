' st_qr.bas -- ST_QR: draw the selected room's chat url as a QR code, wait for
' a button, then hand off to ST_BOOT.
'
' Ordering is forced. do_boot never returns on success -- boot_mount_with_progress
' resets the console once the image is mounted -- so there is no "after the
' boot" to draw anything in. ST_LIST therefore hands to ST_QR, and ST_QR sets
' ST_BOOT itself once the player has had a chance to scan.
'
' Rendering technique: STIC Colored Squares. In colour-stack mode a BACKTAB
' card with bit 12 set is not a character at all -- it is four independently
' coloured 4x4 pixel quadrants. That turns the 20x12 card grid into a 40x24
' grid of coloured squares, which is exactly enough for a 21x21 version 1
' symbol at one module per square. It costs no GRAM: the card definition IS
' the pixel data.
'
' Nothing smaller is possible. A version 2 symbol is 25x25 and does not fit,
' and drawing modules from GRAM at 3x3 pixels would need 121 unique cards
' against the 64 GRAM slots the hardware has. This is the whole reason the
' URL budget is 25 characters everywhere else in the system.
'
' Proven against a real decoder first: /home/thomc/Workspace/intv-qr generated
' the same packing at build time, rendered it at the STIC's pixel aspect, and
' round-tripped it through zbarimg.

    ' Colored Squares packing. Each quadrant's colour goes in a different
    ' field of the word, and pixel 3's is split across three separate bits:
    '   word = $1000 | p0 | (p1<<3) | (p2<<6) | pix3(p3)
    ' Only two colours are ever used here, so both cases collapse to constants
    ' rather than needing the bit-shuffling at runtime.
    CONST QR_CS_ENABLE = $1000   ' bit 12: this card is coloured squares
    CONST QR_SQ_LIGHT0 = 7       ' colour 7 in quadrant 0 (bits 0-2)
    CONST QR_SQ_LIGHT1 = $38     '                      1 (bits 3-5)
    CONST QR_SQ_LIGHT2 = $1C0    '                      2 (bits 6-8)
    CONST QR_SQ_LIGHT3 = $2600   '                      3 (bits 9,10,13)
    ' All four light = $37FF, all four dark = $1000. Both appear verbatim in
    ' intv-qr's generated DATA, which is the cross-check that these are right.

    CONST QR_MODULES = 21
    CONST QR_GRID_W  = SCREEN_COLS * 2   ' 40 squares
    CONST QR_GRID_H  = SCREEN_ROWS * 2   ' 24 squares

    ' Symbol origin in the square grid. Horizontally centred, giving 9 squares
    ' of quiet zone to the left and 10 to the right. Vertically there is only
    ' 1 square above and 2 below -- 21 modules plus the 4-module margin a
    ' scanner wants is 29, against 24 rows. The border is set to the same white
    ' so the margin continues into overscan, which is what intv-qr relied on
    ' and what zbar decoded. It is the weakest part of this and worth checking
    ' on a real television.
    CONST QR_QX = 9
    CONST QR_QY = 1

    ' Expected size of a version 1 binary reply: 1 size byte + 441 bits.
    CONST QR_V1_BYTES = 57

    DIM qr_cx, qr_cy, qr_x, qr_y, qr_b, qr_i
    DIM #qr_w, #qr_src, #qr_n   ' #qr_len is DIMmed in fujinet.bas, which sets it
    ' #qr_n counts modules, which runs to 440 -- an 8-bit loop variable would
    ' silently never reach its limit.

' ---------------------------------------------------------------------------
' do_qr: the ST_QR state body.
'
' Any failure -- no chat url on this record, the FujiNet refusing to encode,
' a short reply -- falls straight through to ST_BOOT. A missing QR code must
' never stop a game from booting.
' ---------------------------------------------------------------------------
do_qr: PROCEDURE
    state = ST_BOOT

    GOSUB qr_load_url
    IF fn_len = 0 THEN RETURN

    GOSUB scr_clear
    PRINT AT screenpos(0,5) COLOR COL_NORMAL,"  GETTING CHAT CODE "

    GOSUB qr_build
    IF fn_ok = 0 THEN RETURN

    GOSUB qr_expand
    GOSUB qr_instructions
    GOSUB qr_paint
    GOSUB qr_wait_button

    ' Put the list palette back before do_boot draws its progress bar.
    GOSUB scr_video_list
    GOSUB scr_clear
END

' ---------------------------------------------------------------------------
' qr_instructions: say what the code is for, before it fills the screen.
'
' This has to happen here rather than alongside the code, because the QR screen
' has no room for text -- see qr_wait_button.
' ---------------------------------------------------------------------------
qr_instructions: PROCEDURE
    GOSUB scr_clear
    PRINT AT screenpos(0,3)  COLOR COL_NORMAL,"  SCAN THE CODE TO  "
    PRINT AT screenpos(0,4)  COLOR COL_NORMAL,"  CHAT WITH PLAYERS  "
    PRINT AT screenpos(0,7)  COLOR COL_HILIGHT,"  PRESS ANY BUTTON  "
    PRINT AT screenpos(0,8)  COLOR COL_HILIGHT,"     TO CONTINUE    "

    FOR qr_i = 0 TO 149
        WAIT
    NEXT qr_i
END

' ---------------------------------------------------------------------------
' qr_load_url: copy the selected record's REC_CHATURL into SC_QRURL, and set
' fn_len to its length. fn_len = 0 means this room has no chat.
' ---------------------------------------------------------------------------
qr_load_url: PROCEDURE
    #qr_src = SC_RECS + sel_rec * REC_STRIDE + REC_CHATURL

    fn_len = 0
    FOR qr_i = 0 TO 24
        qr_b = PEEK(#qr_src + qr_i) AND 255
        IF qr_b = 0 THEN EXIT FOR
        POKE (SC_QRURL + qr_i), qr_b
        fn_len = fn_len + 1
    NEXT qr_i
    POKE (SC_QRURL + fn_len), 0
END

' ---------------------------------------------------------------------------
' qr_build: INPUT, ENCODE, LENGTH, OUTPUT. Leaves fn_ok clear on any failure.
' ---------------------------------------------------------------------------
qr_build: PROCEDURE
    #fn_src = SC_QRURL
    GOSUB qr_input
    IF fn_ok = 0 THEN RETURN

    GOSUB qr_encode_v1
    IF fn_ok = 0 THEN RETURN

    GOSUB qr_length
    IF fn_ok = 0 THEN RETURN

    ' Anything other than a version 1 reply means the FujiNet chose a different
    ' size, which this renderer cannot draw. Refuse rather than paint a symbol
    ' that will not scan.
    IF #qr_len <> QR_V1_BYTES THEN
        fn_ok = 0
        RETURN
    END IF

    GOSUB qr_output
    IF fn_ok = 0 THEN RETURN

    IF (PEEK(SC_QRRAW) AND 255) <> QR_MODULES THEN fn_ok = 0
END

' ---------------------------------------------------------------------------
' qr_expand: SC_QRRAW[1..56] holds the matrix packed one bit per module,
' row-major, least significant bit first. Spread it to one byte per module in
' SC_QRBITS.
'
' Done once, 441 times, rather than bit-twiddling inside qr_paint: IntyBASIC
' has no variable-count shift, so testing bit (i AND 7) would need an inner
' loop or a mask table on every one of the 960 quadrant lookups the paint does.
' Walking the bits in raster order here makes it a running mask and a carry.
' ---------------------------------------------------------------------------
qr_expand: PROCEDURE
    #qr_src = SC_QRRAW + 1
    qr_b = PEEK(#qr_src) AND 255
    #qr_w = 1

    FOR #qr_n = 0 TO QR_MODULES * QR_MODULES - 1
        IF (qr_b AND #qr_w) <> 0 THEN
            POKE (SC_QRBITS + #qr_n), 1
        ELSE
            POKE (SC_QRBITS + #qr_n), 0
        END IF

        #qr_w = #qr_w * 2
        IF #qr_w = 256 THEN
            #qr_w = 1
            #qr_src = #qr_src + 1
            qr_b = PEEK(#qr_src) AND 255
        END IF
    NEXT #qr_n
END

' ---------------------------------------------------------------------------
' qr_dark: is the square at (qr_x, qr_y) a dark module? Squares outside the
' symbol are light, which is what makes the quiet zone fall out for free.
' ---------------------------------------------------------------------------
qr_dark: PROCEDURE
    qr_b = 0
    IF qr_x < QR_QX OR qr_y < QR_QY THEN RETURN
    IF qr_x >= QR_QX + QR_MODULES OR qr_y >= QR_QY + QR_MODULES THEN RETURN
    qr_b = PEEK(SC_QRBITS + (qr_y - QR_QY) * QR_MODULES + (qr_x - QR_QX)) AND 255
END

' ---------------------------------------------------------------------------
' qr_paint: switch to an all-white colour stack and fill every BACKTAB card
' with its four quadrants.
'
' All four stack registers have to be white: colour 7 in a Colored Squares
' quadrant means "whatever the colour stack currently holds", and which entry
' is current depends on how many CS_ADVANCE cards preceded this one -- not
' something worth tracking across a full-screen repaint.
' ---------------------------------------------------------------------------
qr_paint: PROCEDURE
    MODE 0, CS_WHITE, CS_WHITE, CS_WHITE, CS_WHITE
    BORDER CS_WHITE
    WAIT

    FOR qr_cy = 0 TO SCREEN_ROWS - 1
        FOR qr_cx = 0 TO SCREEN_COLS - 1
            #qr_w = QR_CS_ENABLE

            qr_x = qr_cx * 2     : qr_y = qr_cy * 2
            GOSUB qr_dark : IF qr_b = 0 THEN #qr_w = #qr_w + QR_SQ_LIGHT0

            qr_x = qr_cx * 2 + 1
            GOSUB qr_dark : IF qr_b = 0 THEN #qr_w = #qr_w + QR_SQ_LIGHT1

            qr_x = qr_cx * 2     : qr_y = qr_cy * 2 + 1
            GOSUB qr_dark : IF qr_b = 0 THEN #qr_w = #qr_w + QR_SQ_LIGHT2

            qr_x = qr_cx * 2 + 1
            GOSUB qr_dark : IF qr_b = 0 THEN #qr_w = #qr_w + QR_SQ_LIGHT3

            #BACKTAB(qr_cy * SCREEN_COLS + qr_cx) = #qr_w
        NEXT qr_cx
    NEXT qr_cy
END

' ---------------------------------------------------------------------------
' qr_wait_button: hold the code on screen until the player presses something.
'
' No prompt is drawn over the code. There is no card row to spare -- the
' symbol already runs to within one square of the top and two of the bottom,
' and a row of text would eat the quiet zone the border is standing in for.
' The instruction goes on the screen before this one instead.
' ---------------------------------------------------------------------------
qr_wait_button: PROCEDURE
    ' Wait for the button that chose the room to be released first. in_btn is
    ' edge-triggered and so is already 0 by now; in_braw is the level, which is
    ' what "still held" actually needs.
    DO
        WAIT
        GOSUB in_poll
    LOOP WHILE in_braw <> 0

    DO
        WAIT
        GOSUB in_poll
    LOOP WHILE in_btn = 0
END
