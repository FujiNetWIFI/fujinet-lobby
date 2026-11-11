; p2.inc -- page 2: the rooms. A GET (page 3) of LB_ASK records from 4 x
; LPAGE on -- one past the page, so a next page is known -- drawn a row
; each: a record's game (tan) when it isn't the one above's, then its room:
; the name (grey if offline), the players. The reply stays while the
; screen is up: the bar's long name scrolls straight out of it. The stick
; picks a room and turns the pages; the button or ENT joins it (page 4).
; Key 0's name (page 9) shows in T0; N edits it.

    PAGE_HEAD
    db e_run&0FFh               ; E_RUN: from the boot
    db e_fetch&0FFh             ; E_FETCH
    db e_show&0FFh              ; E_SHOW
e_run:
    jmp lp_run
e_fetch:
    jmp lp_fetch
e_show:
    jmp lp_show

; lp_loop: a frame: the stick and the keys; the bar's long name scrolls.
lp_loop:
    call in_poll
    call lp_marq
    mov r0,#JOYEV
    movx a,@r0
    jb0 ll_up
    jb1 ll_down
    jb2 ll_left
    jb3 ll_right
    jb4 ll_join
    mov r0,#KEY
    movx a,@r0
    xrl a,#K_ENT
    jz ll_join
    mov r0,#KEY
    movx a,@r0
    mov r5,a
    xrl a,#K_R
    jz ll_reload
    mov a,r5
    xrl a,#K_NO                 ; N
    jz ll_name
    mov a,r5
    xrl a,#K_QUEST
    jz ll_help
    mov a,r5
    xrl a,#K_H
    jz ll_help
    jmp lp_loop
ll_up:
    jmp lm_up
ll_down:
    jmp lm_down
ll_reload:
    jmp lp_run
ll_name:
    GOTO PG_NAME,E_EDIT
ll_help:
    mov r0,#LCUR
    mov a,#LC_KEEP
    movx @r0,a
    HELP HC_LIST,PG_LIST,E_SHOW

ll_left:
    mov r0,#LPAGE
    movx a,@r0
    jz lp_loop
    dec a
    movx @r0,a
    mov a,#LC_FIRST
    jmp lp_turn
ll_right:
    mov r0,#LMORE
    movx a,@r0
    jz lp_loop
    mov r0,#LPAGE               ; (offsets are a byte: 63 pages)
    movx a,@r0
    xrl a,#63
    jz lp_loop
    movx a,@r0
    inc a
    movx @r0,a
    mov a,#LC_FIRST
    ; fall into lp_turn

; lp_turn: the page LPAGE, the bar on its room LCUR = A.
lp_turn:
    mov r0,#LCUR
    movx @r0,a
    mov a,#SND_MOVE
    call beep
    jmp lf_named

; ll_join: the bar's room: the join (page 4) takes it from the reply. A
; failure comes back to it.
ll_join:
    mov r0,#LCUR
    mov a,#LC_KEEP
    movx @r0,a
    mov r0,#SEL
    movx a,@r0
    mov r4,a
    call rm_get
    mov r0,#LREC
    movx @r0,a
    mov a,#SND_SELECT
    call beep
    GOTO PG_JOIN,E_RUN

; lp_run: from the first page. lp_fetch: the page LPAGE, after the name
; (page 9) if it isn't known yet.
lp_run:
    mov r0,#LPAGE
    clr a
    movx @r0,a
    mov r0,#LCUR
    movx @r0,a                  ; LC_FIRST
lp_fetch:
    mov r0,#FLAGS
    movx a,@r0
    jb0 lf_named
    GOTO PG_NAME,E_RUN
lf_named:
    mov r0,#LTRY
    mov a,#2
    movx @r0,a
lf_get:
    call lp_wait
    mov r0,#NREQ
    mov a,#NQ_PAGE
    movx @r0,a
    mov r0,#RETPG
    mov a,#PG_LIST
    movx @r0,a
    inc r0
    mov a,#E_SHOW
    movx @r0,a
    GOTO PG_NET,E_RUN

; lb_fold: LINEBUF's a-z to A-Z (bit 7 left alone). Clobbers A, r0, r6,
; r7.
lb_fold:
    mov r0,#LINEBUF
    mov r7,#TXT_LEN
lf_l:
    movx a,@r0
    mov r6,a
    add a,#256-'a'
    jnc lf_n
    mov a,r6
    add a,#256-'z'-1
    jc lf_n
    mov a,r6
    add a,#256-20h
    movx @r0,a
lf_n:
    inc r0
    djnz r7,lf_l
    ret

    if $ > 900h
    error "the list's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; lp_show: the GET is back. A reply whose length isn't its count's is got
; again (LTRY); a 404 past the first page is the page before. Nothing to
; list, and why (NRES): page 7.
lp_show:
    mov r0,#NRES
    movx a,@r0
    jz ls_reply
    xrl a,#NS_404
    jnz ls_nolist
ls_none:
    mov r0,#LPAGE
    movx a,@r0
    jz ls_empty
    dec a
    movx @r0,a
    mov r0,#LCUR
    mov a,#LC_LAST
    movx @r0,a
    jmp lf_named
ls_empty:
    mov r0,#NRES
    mov a,#NS_404
    movx @r0,a
ls_nolist:
    GOTO PG_HELP,E_NOLIST
ls_reply:
    STB 1                       ; [count][0][0]
    jnz ls_bad
    STB 2
    jnz ls_bad
    STB 0
    mov r5,a
    add a,#256-LB_ASK-1
    jc ls_bad
    mov a,r5
    jz ls_none
    call len_chk
    jz ls_ok
ls_bad:
    mov r0,#LTRY
    movx a,@r0
    dec a
    movx @r0,a
    jz ls_give
    jmp lf_get
ls_give:
    mov r0,#NRES
    mov a,#NR_BAD
    movx @r0,a
    jmp ls_nolist
ls_ok:
    call lp_layout
    ; fall into lp_draw

; lp_draw: the list: T0, T1, the rows, the bar (LCUR).
lp_draw:
    call scr_text
    call lp_title
    mov r4,#L_BOT
    mov a,#s_legend&0FFh
    call sayk_l
    call lp_cursor
    call mq_reset
    mov r4,#0
ld_row:
    mov r0,#TMP0
    mov a,r4
    movx @r0,a
    call lr_row
    mov r0,#TMP0
    movx a,@r0
    inc a
    mov r4,a
    xrl a,#NROWS
    jnz ld_row
    mov r0,#SEL
    movx a,@r0
    call bar_row
    jmp lp_loop

; lp_layout: LMORE, LCNT, ROWMAP from the reply: a record's game on a row
; of its own when it isn't the one above's (always the page's first), then
; its room. LB_PAGE records in 2 rows each at most: the 8 rows always hold
; them. Clobbers A, r0-r7.
lp_layout:
    STB 0
    mov r5,a
    mov r6,#0
    xrl a,#LB_ASK
    jnz ly_m
    mov r6,#1
ly_m:
    mov r0,#LMORE
    mov a,r6
    movx @r0,a
    mov a,r5
    add a,#256-LB_PAGE-1
    jnc ly_n
    mov r5,#LB_PAGE
ly_n:
    mov r0,#LCNT
    mov a,r5
    movx @r0,a
    mov r1,#ROWMAP              ; every row empty
    mov r2,#NROWS
    mov a,#0FFh
ly_clr:
    movx @r1,a
    inc r1
    djnz r2,ly_clr
    mov r0,#TMP0                ; the row
    mov a,#ROWMAP
    movx @r0,a
    inc r0                      ; TMP1: the record
    clr a
    movx @r0,a
ly_rec:
    mov r0,#TMP1
    movx a,@r0
    jz ly_head
    call lp_same
    jz ly_room
ly_head:
    mov r0,#TMP1
    movx a,@r0
    orl a,#80h
    call ly_put
ly_room:
    mov r0,#TMP1
    movx a,@r0
    call ly_put
    mov r0,#TMP1
    movx a,@r0
    inc a
    movx @r0,a
    mov r6,a
    mov r0,#LCNT
    movx a,@r0
    xrl a,r6
    jnz ly_rec
    ret
; ly_put: ROWMAP's next row = A. Clobbers A, r0, r1.
ly_put:
    mov r1,a
    mov r0,#TMP0
    movx a,@r0
    xch a,r1
    movx @r1,a
    inc r1
    mov a,r1
    movx @r0,a
    ret

    if $ > 0A00h
    error "the list's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h

; lr_row: row r4 from ROWMAP: a game (tan), a room (its name from MQOFF on
; the bar's row; grey if offline; " c/m" its players, tan), or blank.
; Clobbers A, r0-r7, TMP1, TMP2.
lr_row:
    mov r0,#TMP1
    mov a,r4
    movx @r0,a
    call lb_clear
    mov r0,#TMP1
    movx a,@r0
    mov r4,a
    call rm_get
    mov r5,a
    inc a
    jz lr_white                 ; none: blank
    mov a,r5
    jb7 lr_head
    mov r0,#TMP2                ; a room: its record
    movx @r0,a
    call rec_at
    mov a,#LR_ROOM
    call add_r32
    mov r0,#SEL                 ; the bar's: from MQOFF
    movx a,@r0
    mov r6,a
    mov r0,#TMP1
    movx a,@r0
    xrl a,r6
    jnz lr_name
    mov r0,#MQOFF
    movx a,@r0
    call add_r32
lr_name:
    mov r1,#LINEBUF
    mov r7,#ROOMW
    call rx_lb
    mov r0,#TMP2                ; the players: cur/max
    movx a,@r0
    call rec_at
    mov a,#LR_CUR
    call add_r32
    call st_next
    call lr_digit
    mov r0,#LINEBUF+ROOMW+1
    movx @r0,a
    inc r0
    mov a,#'/'|80h
    movx @r0,a
    call st_next
    call lr_digit
    mov r0,#LINEBUF+ROOMW+3
    movx @r0,a
    mov r0,#TMP2                ; online: white, else grey
    movx a,@r0
    call rec_at
    mov a,#LR_ONLINE
    call add_r32
    call st_byte
    mov r6,#C_WHITE
    jnz lr_col
    mov r6,#C_DGREY
lr_col:
    mov r0,#TXCOL
    mov a,r6
    movx @r0,a
    inc r0
    mov a,#C_TAN
    movx @r0,a
    jmp lr_out
lr_head:
    anl a,#7Fh                  ; a game: its name
    call rec_at
    mov a,#LR_GAME
    call add_r32
    mov r1,#LINEBUF
    mov r7,#NCOLS
    call rx_lb
    mov a,#C_TAN
    call col_set
    jmp lr_out
lr_white:
    call col_white
lr_out:
    call lb_fold
    mov r0,#RICON
    clr a
    movx @r0,a
    mov r0,#TMP1
    movx a,@r0
    mov r4,a
    jmp row_line

; lr_digit: A (players) as a digit, tan; 10 or more "+". Clobbers r6.
lr_digit:
    mov r6,a
    add a,#256-10
    jc ld_big
    mov a,r6
    add a,#'0'|80h
    ret
ld_big:
    mov a,#'+'|80h
    ret

; lp_marq: the bar's room name, if it's longer than its place: after
; MQ_WAIT frames still, a character every MQ_STEP, to its end and back.
; Clobbers A, r0-r7.
lp_marq:
    mov r0,#MQCLK
    movx a,@r0
    dec a
    movx @r0,a
    jz lq_step
    ret
lq_step:
    mov a,#MQ_STEP
    movx @r0,a
    mov r0,#SEL                 ; its length: up to the NUL, 32 at most
    movx a,@r0
    mov r4,a
    call rm_get
    call rec_at
    mov a,#LR_ROOM
    call add_r32
    mov r5,#0
lq_len:
    call st_next
    jz lq_have
    inc r5
    mov a,r5
    xrl a,#32
    jnz lq_len
lq_have:
    mov a,r5                    ; r5 - ROOMW: the last offset (C: room)
    add a,#256-ROOMW-1
    jc lq_long
    ret
lq_long:
    inc a
    mov r6,a
    mov r0,#MQDIR
    movx a,@r0
    mov r7,a
    mov r0,#MQOFF
    movx a,@r0
    add a,r7
    movx @r0,a
    jz lq_end                   ; back at the start
    xrl a,r6
    jz lq_end                   ; at the end
    jmp lq_draw
lq_end:
    mov r0,#MQDIR               ; turn round, and rest there
    movx a,@r0
    cpl a
    inc a
    movx @r0,a
    mov r0,#MQCLK
    mov a,#MQ_WAIT
    movx @r0,a
lq_draw:
    mov r0,#SEL
    movx a,@r0
    mov r4,a
    jmp lr_row

; mq_reset: the bar's name from its start, resting. Clobbers A, r0.
mq_reset:
    mov r0,#MQOFF
    clr a
    movx @r0,a
    inc r0                      ; MQCLK
    mov a,#MQ_WAIT
    movx @r0,a
    inc r0                      ; MQDIR
    mov a,#1
    movx @r0,a
    ret

    if $ > 0B00h
    error "the list's page $A has run out"
    endif

; ===========================================================================
    org 0B00h

; lp_cursor: SEL from LCUR: a room row (the list has one at least).
; Clobbers A, r0, r4.
lp_cursor:
    mov r0,#LCUR
    movx a,@r0
    jz lc_first
    inc a
    jz lc_last
    mov r0,#SEL                 ; where it was, if that's a room
    movx a,@r0
    mov r4,a
    add a,#256-NROWS
    jc lc_first
    call rm_get
    jb7 lc_first
    ret
lc_first:
    mov r4,#0
lcf_l:
    call rm_get
    jb7 lcf_n
    jmp lc_set
lcf_n:
    inc r4
    jmp lcf_l
lc_last:
    mov r4,#NROWS-1
lcl_l:
    call rm_get
    jb7 lcl_n
    jmp lc_set
lcl_n:
    dec r4
    jmp lcl_l
lc_set:
    mov r0,#SEL
    mov a,r4
    movx @r0,a
    ret

; rm_get: A = ROWMAP[r4]. Clobbers r0.
rm_get:
    mov a,r4
    add a,#ROWMAP
    mov r0,a
    movx a,@r0
    ret

; lp_same: A = 0 if record A's type and game are record A-1's. Clobbers
; A, r0-r7.
lp_same:
    dec a
    call rec_at
    mov a,r2
    mov r5,a
    mov a,r3
    mov r4,a
    mov r7,#LR_ROOM             ; the type, then the game: NUL-padded
lsm_l:
    mov a,r5
    mov r2,a
    mov a,r4
    mov r3,a
    call st_byte
    mov r1,a
    mov a,r5
    add a,#LB_REC
    mov r2,a
    clr a
    addc a,r4
    mov r3,a
    call st_byte
    xrl a,r1
    jnz lsm_x
    inc r5
    mov a,r5
    jnz lsm_n
    inc r4
lsm_n:
    djnz r7,lsm_l
    clr a
lsm_x:
    ret

; len_chk: A = 0 if the reply's length is a count of A's. Clobbers r0,
; r2, r3, r6.
len_chk:
    call rec_at
    mov a,#FN_R_RXLEN_LO&0FFh
    call mb_get
    xrl a,r2
    jnz lc_x
    mov a,#FN_R_RXLEN_HI&0FFh
    call mb_get
    xrl a,r3
lc_x:
    ret

; rec_at: r3:r2 = record A's offset in the reply (A = LB_ASK: the length
; of LB_ASK records). add_r32: r3:r2 + A. Clobbers r6.
rec_at:
    mov r6,a
    add a,#rec_lo&0FFh
    movp a,@a
    mov r2,a
    mov a,r6
    add a,#rec_hi&0FFh
    movp a,@a
    mov r3,a
    ret
add_r32:
    add a,r2
    mov r2,a
    clr a
    addc a,r3
    mov r3,a
    ret
rec_lo:
K   set 0
    rept LB_ASK+1
    db (LB_HDR+K*LB_REC)&0FFh
K   set K+1
    endm
rec_hi:
K   set 0
    rept LB_ASK+1
    db (LB_HDR+K*LB_REC)>>8
K   set K+1
    endm

    include "local.inc"
    if $ > 0C00h
    error "the list's page $B has run out"
    endif

; ===========================================================================
    org 0C00h
    STRPAGE l
; sayk_l: line r4 = the string at offset A, white, its bit-7 keys tan.
sayk_l:
    call lbs0_l
    mov a,#C_WHITE
    mov r0,#TXCOL
    movx @r0,a
    inc r0
    mov a,#C_TAN
    movx @r0,a
    jmp say_line_c

; lp_title: T0: the name, the page (when there are more than one).
; Clobbers A, r0-r7.
lp_title:
    call lb_clear
    mov r0,#NAMEBUF
    movx a,@r0
    jz lt_anon
    mov a,#s_lobby&0FFh
    call lbs_l
    mov r1,#NAMEBUF
    mov r2,#NAMEMAX
lt_nm:
    movx a,@r1
    jz lt_page
    movx @r0,a
    inc r0
    inc r1
    djnz r2,lt_nm
    jmp lt_page
lt_anon:
    mov a,#s_title&0FFh
    call lbs_l
lt_page:
    mov r0,#LPAGE
    movx a,@r0
    mov r5,a
    jnz lt_num
    mov r0,#LMORE
    movx a,@r0
    jz lt_say
lt_num:
    mov a,r5
    add a,#256-9
    jc lt_plus
    mov a,r5
    add a,#'1'|80h
    jmp lt_put
lt_plus:
    mov a,#'+'|80h
lt_put:
    mov r0,#LINEBUF+TXT_LEN-1
    movx @r0,a
lt_say:
    mov a,#C_WHITE
    mov r0,#TXCOL
    movx @r0,a
    inc r0
    mov a,#C_TAN
    movx @r0,a
    mov r4,#L_TOP
    jmp say_line_c

; lp_wait: the screen while a page comes. Clobbers A, r0-r7.
lp_wait:
    call scr_text
    call lp_title
    mov r4,#3
    mov a,#s_loading&0FFh
    jmp row_l

s_title:    db "   GAME LOBBY",0
s_lobby:    db "LOBBY ",0
s_legend:   db 'F'|80h,'I'|80h,'R'|80h,'E'|80h," JOIN ",'?'|80h," HELP",0
s_loading:  db "  LOADING",0

; lm_up, lm_down: the bar to the room above or below; past the page's
; first or last, the page before or after.
lm_up:
    mov r0,#SEL
    movx a,@r0
    mov r4,a
lmu_l:
    mov a,r4
    jz lmu_page
    dec r4
    call rm_get
    jb7 lmu_l
    jmp lm_sel
lmu_page:
    mov r0,#LPAGE
    movx a,@r0
    jz lm_stay
    dec a
    movx @r0,a
    mov a,#LC_LAST
    jmp lp_turn
lm_down:
    mov r0,#SEL
    movx a,@r0
    mov r4,a
lmd_l:
    inc r4
    mov a,r4
    xrl a,#NROWS
    jz lmd_page
    call rm_get
    jb7 lmd_l
    jmp lm_sel
lmd_page:
    jmp ll_right
lm_stay:
    jmp lp_loop

; lm_sel: the bar to row r4: the row it leaves drawn from its start.
lm_sel:
    mov r0,#TMP0
    mov a,r4
    movx @r0,a
    mov a,#SND_MOVE
    call beep
    mov r0,#MQOFF
    clr a
    movx @r0,a
    mov r0,#SEL
    movx a,@r0
    mov r4,a
    call lr_row
    mov r0,#TMP0
    movx a,@r0
    mov r0,#SEL
    movx @r0,a
    call mq_reset
    mov r0,#SEL
    movx a,@r0
    call bar_row
    jmp lp_loop

    if $ > 0D00h
    error "the list's page $C has run out"
    endif

; ===========================================================================
    org 0D00h
    include "mail.inc"
    include "beep.inc"

; ===========================================================================
    org 0E00h
    include "input.inc"
    include "snd.inc"

    PAGE_TAIL
