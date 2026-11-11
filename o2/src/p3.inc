; p3.inc -- page 3: one GET from the lobby server (NREQ), then back to
; RETPG:RETEN with NRES: 0 (the reply is in: FN_R_RXLEN its length), NS_404
; (no rooms there), NR_NONE (no answer), or another nDevStatus.
;   NQ_PAGE   LB_ASK records from 4 x LPAGE on
;   NQ_REC    the record at JABS
;   NQ_NEAR   LB_ASK records from JABS - 2 (0 at least)
; CLOSE (the last one: a NAK is fine), OPEN, STATUS until the answer is all
; in -- EOF, or the same count waiting twice running (fujinet-pc says OK,
; not EOF, while bytes wait) -- then one READ of it. Each transaction waits
; (fn_go); the caller has its screen up.

NPOLLS  equ 200                 ; STATUS polls, two frames apart

    PAGE_HEAD
    db e_get&0FFh               ; E_RUN
e_get:
    jmp ng_run

ng_run:
    mov r3,#NC_CLOSE
    mov r4,#0
    call np_begin
    call fn_go
    mov r3,#NC_OPEN
    mov r4,#2
    call np_begin
    mov a,#NM_GET
    call fn_par1
    clr a
    call fn_par1
    call url_tx
    call fn_go
    jnz ng_why
    mov r0,#TMP0                ; the count waiting last time: none
    clr a
    movx @r0,a
    inc r0
    movx @r0,a
    mov r0,#TMP2
    mov a,#NPOLLS
    movx @r0,a
ng_poll:
    call np_stat
    jnz ng_none
    STB 3                       ; nDevStatus
    mov r4,a
    xrl a,#NS_404
    jz ng_res
    STB 0                       ; the count waiting
    mov r5,a
    STB 1
    mov r1,a
    orl a,r5
    jnz ng_some
    mov a,r4                    ; none yet: still coming, or an error
    xrl a,#NS_OK
    jnz ng_res
    jmp ng_wait
ng_some:
    mov a,r4                    ; the server is done: read it all
    xrl a,#NS_EOF
    jz ng_read
    mov r0,#TMP0                ; the same as last time: it has settled
    movx a,@r0
    xrl a,r5
    jnz ng_new
    inc r0
    movx a,@r0
    xrl a,r1
    jz ng_read
ng_new:
    mov r0,#TMP0
    mov a,r5
    movx @r0,a
    inc r0
    mov a,r1
    movx @r0,a
ng_wait:
    call frame_wait
    call frame_wait
    mov r0,#TMP2
    movx a,@r0
    dec a
    movx @r0,a
    jnz ng_poll
ng_none:
    mov r4,#NR_NONE
ng_res:
    mov r0,#NRES
    mov a,r4
    movx @r0,a
    mov r0,#RETEN
    movx a,@r0
    mov r7,a
    mov r0,#RETPG
    movx a,@r0
    jmp far_go

; ng_read: READ what waits, the cart's 1K at most.
ng_read:
    mov a,r1
    add a,#256-4
    jnc ng_rd
    mov r5,#0
    mov r1,#4
ng_rd:
    mov r3,#NC_READ
    mov r4,#2
    call np_begin
    mov a,#2                    ; a two-byte parameter: the length
    call fn_tx
    mov a,r5
    call fn_tx
    mov a,r1
    call fn_tx
    clr a
    call fn_par1
    call fn_go
    jnz ng_none
    mov r4,#0
    jmp ng_res

; ng_why: the OPEN failed: STATUS's nDevStatus says why, if it says.
ng_why:
    call np_stat
    jnz ng_none
    STB 3
    mov r4,a
    add a,#256-2                ; 0 and 1 say nothing
    jnc ng_none
    jmp ng_res

; np_stat: STATUS. A = 0 if it worked. Clobbers A, r0-r2, r5-r7.
np_stat:
    mov r3,#NC_STATUS
    mov r4,#2
    call np_begin
    clr a
    call fn_par1
    clr a
    call fn_par1
    jmp fn_go

; np_begin: a network device command r3 with r4 parameters.
np_begin:
    mov r2,#NET_DEVICEID
    jmp fn_begin

    if $ > 900h
    error "the net page's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; url_tx: the request's URL into the TX stream, NUL-ended: u_base (the
; endpoint, ?bin=1&platform=..&pagesize=), the page size, &offset=, the
; offset in decimal. Clobbers A, r0, r1, r5-r7.
url_tx:
    mov r1,#u_base&0FFh
    call ut_str
    mov r0,#NREQ
    movx a,@r0
    mov r5,a
    mov a,#'0'+LB_ASK
    mov r6,a
    mov a,r5
    xrl a,#NQ_REC
    jnz ut_ps
    mov r6,#'1'
ut_ps:
    mov a,r6
    call fn_tx
    mov r1,#u_off&0FFh
    call ut_str
    mov a,r5
    jz ut_page
    mov r0,#JABS
    movx a,@r0
    mov r6,a
    mov a,r5
    xrl a,#NQ_REC
    jz ut_abs
    mov a,r6                    ; NQ_NEAR: two before, 0 at least
    add a,#256-2
    jc ut_num
    clr a
    jmp ut_num
ut_abs:
    mov a,r6
    jmp ut_num
ut_page:
    mov r0,#LPAGE
    movx a,@r0
    rl a
    rl a
    anl a,#0FCh
    ; fall into ut_num

; ut_num: A in decimal, then the NUL. Clobbers A, r0, r5-r7.
ut_num:
    mov r7,#'0'-1               ; hundreds
un_h:
    inc r7
    add a,#256-100
    jc un_h
    add a,#100
    mov r5,#'0'-1               ; tens
un_t:
    inc r5
    add a,#256-10
    jc un_t
    add a,#10+'0'
    mov r6,a
    mov a,r7
    xrl a,#'0'
    jz un_noh
    mov a,r7
    call fn_tx
    jmp un_10
un_noh:
    mov a,r5
    xrl a,#'0'
    jz un_1
un_10:
    mov a,r5
    call fn_tx
un_1:
    mov a,r6
    call fn_tx
    clr a
    jmp fn_tx

; ut_str: the string at offset r1 of this page into the TX stream, its NUL
; left out. Clobbers A, r0, r1.
ut_str:
    mov a,r1
    movp a,@a
    jz us_x
    call fn_tx
    inc r1
    jmp ut_str
us_x:
    ret

u_off:  db "&offset=",0
    include "endpoint.inc"
    if $ > 0A00h
    error "the net page's page $9 has run out (ENDPOINT too long?)"
    endif

; ===========================================================================
    org 0D00h
    include "mail.inc"

; ===========================================================================
    org 0E00h
    include "input.inc"
    include "snd.inc"

    PAGE_TAIL
