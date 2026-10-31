; p4.inc -- page 4: the join. First, from the list's reply while it lasts:
; the room's type (its game's appkey id), its place in the whole list, a
; checksum of its type and server URL, and its client URL's host and path
; (tnfs://host/path, into EDBUF). Then the host in a slot -- the one that
; has it, else the first empty one, else slot 8 -- mounted from its top,
; and slot 0's path. Then the room again (page 3), found by the checksum
; (the list is sorted by players: it can move): its server URL is the
; game's appkey (creator 1, app 1, key = the type), where the game finds
; its table. Then the boot (page 5). A failure goes back to the list.

    PAGE_HEAD
    db e_join&0FFh              ; E_RUN: the list's room LREC
    db e_rec&0FFh               ; E_REC: the room fetched again
    db e_scan&0FFh              ; E_SCAN: its neighbours
e_join:
    jmp jn_run
e_rec:
    jmp jn_rec
e_scan:
    jmp jn_scan

jn_run:
    mov r0,#FLAGS               ; EDBUF is the join's now
    movx a,@r0
    anl a,#~FL_NAME
    movx @r0,a
    call jn_screen
    mov r0,#JREC
    movx a,@r0
    call rec_at
    call st_byte                ; its type: 0 is no game
    jnz jr_game
    mov a,#s_notgame&0FFh
    jmp jn_fail
jr_game:
    mov r0,#JT
    movx @r0,a
    mov r0,#LPAGE               ; its place: 4 x LPAGE + LREC
    movx a,@r0
    rl a
    rl a
    anl a,#0FCh
    mov r5,a
    mov r0,#JREC
    movx a,@r0
    add a,r5
    mov r0,#JABS
    movx @r0,a
    mov r0,#JREC
    movx a,@r0
    call jn_sum
    mov r0,#JHL
    mov a,r4
    movx @r0,a
    inc r0
    mov a,r5
    movx @r0,a
    mov r0,#JREC                ; its client: tnfs://, any case
    movx a,@r0
    call rec_at
    mov a,#LR_CURL
    call add_r32
    mov r7,#u_tnfs&0FFh
jr_sch:
    mov a,r7
    movp a,@a
    jz jr_host
    mov r1,a
    call st_next
    orl a,#20h
    xrl a,r1
    jnz jr_bad
    inc r7
    jmp jr_sch
jr_host:
    mov r1,#EDBUF               ; host/path
    mov r7,#CURLMAX
    call rx_lb
    mov a,r1
    add a,#256-EDBUF
    mov r5,a                    ; its length
    mov r1,#EDBUF               ; the host: up to the first /
    mov r4,#0
jr_sl:
    mov a,r4
    xrl a,r5
    jz jr_bad
    movx a,@r1
    xrl a,#'/'
    jz jr_split
    inc r1
    inc r4
    jmp jr_sl
jr_split:
    mov a,r4                    ; 1-32
    jz jr_bad
    add a,#256-HOSTMAX-1
    jc jr_bad
    mov a,r4                    ; the path: "/" and a name at least
    cpl a
    inc a
    add a,r5
    mov r6,a
    add a,#256-2
    jnc jr_bad
    mov r0,#JHLEN
    mov a,r4
    movx @r0,a
    inc r0
    mov a,r6
    movx @r0,a
    jmp jn_slots
jr_bad:
    mov a,#s_badurl&0FFh
    jmp jn_fail

u_tnfs: db "tnfs://",0

; jn_sum: r4, r5 = record A's checksum: its type, then its server URL (to
; its NUL, SURLMAX at most): a running sum, and the sum of the sums.
; Clobbers A, r0, r2, r3, r6, r7.
jn_sum:
    call rec_at
    call st_byte
    mov r4,a
    mov r5,a
    mov a,#LR_SURL
    call add_r32
    mov r7,#SURLMAX
js_l:
    call st_next
    jz js_x
    add a,r4
    mov r4,a
    add a,r5
    mov r5,a
    djnz r7,js_l
js_x:
    ret

; jn_write: WRITE_HOST_SLOTS: the slots as they are, slot TMP0 the host.
; A = 0 if it worked. Clobbers A, r0-r7.
jn_write:
    mov r3,#FC_WRHOSTS
    mov r4,#0
    call fuji_begin
    mov r5,#0
jw_slot:
    mov a,r5
    mov r0,#TMP2
    movx @r0,a
    mov r0,#TMP0
    movx a,@r0
    xrl a,r5
    jz jw_new
    mov a,r5
    swap a
    rl a
    mov r2,a
    mov r3,#0
    mov r7,#32
jw_old:
    call st_next
    call fn_tx
    djnz r7,jw_old
    jmp jw_next
jw_new:
    mov r0,#JHLEN
    movx a,@r0
    mov r5,a
    mov r1,#EDBUF
    mov r7,#HOSTMAX
    call xr_tx
jw_next:
    mov r0,#TMP2
    movx a,@r0
    inc a
    mov r5,a
    xrl a,#NROWS
    jnz jw_slot
    jmp fn_go

    if $ > 900h
    error "the join's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; jn_slots: READ_HOST_SLOTS; the slot naming the host (any case), else the
; first empty one, else slot 8 named it (WRITE_HOST_SLOTS); MOUNT_HOST, its
; prefix "/"; SET_DEVICE_FULLPATH, slot 0 = the path. Then the room again.
jn_slots:
    mov r3,#FC_RDHOSTS
    mov r4,#0
    call fuji_begin
    call fn_go
    jnz jh_fail
    mov r0,#TMP0                ; the slot it's in
    mov a,#0FFh
    movx @r0,a
    mov r0,#TMP1                ; the first empty one
    movx @r0,a
    mov r5,#0
jh_slot:
    mov a,r5
    swap a
    rl a
    mov r2,a
    mov r3,#0
    call st_byte                ; empty?
    jnz jh_cmp
    mov r0,#TMP1
    movx a,@r0
    inc a
    jnz jh_next
    mov a,r5
    movx @r0,a
    jmp jh_next
jh_cmp:
    call jn_same
    jnz jh_next
    mov r0,#TMP0
    mov a,r5
    movx @r0,a
    jmp jh_have
jh_next:
    inc r5
    mov a,r5
    xrl a,#NROWS
    jnz jh_slot
    mov r0,#TMP1                ; not there: the first empty, or slot 8
    movx a,@r0
    mov r5,a
    inc a
    jnz jh_claim
    mov r5,#NROWS-1
jh_claim:
    mov r0,#TMP0
    mov a,r5
    movx @r0,a
    call jn_write
    jnz jh_fail
jh_have:
    mov r0,#TMP0
    movx a,@r0
    mov r0,#HOST
    movx @r0,a
    mov r3,#FC_MOUNTHOST
    mov r4,#1
    call fuji_begin
    mov r0,#HOST
    movx a,@r0
    call fn_par1
    call fn_go
    jnz jh_nohost
    mov r3,#FC_HPREFIXSET       ; from its top (a host may refuse: fine)
    mov r4,#1
    call fuji_begin
    mov r0,#HOST
    movx a,@r0
    call fn_par1
    mov a,#'/'
    call fn_tx
    mov r6,#255
    call fn_zeros
    call fn_go
    mov r3,#FC_DEVPATH          ; slot 0: the game, read only
    mov r4,#3
    call fuji_begin
    clr a
    call fn_par1
    mov r0,#HOST
    movx a,@r0
    call fn_par1
    mov a,#1
    call fn_par1
    mov r0,#JHLEN               ; the path, NUL-padded to 256
    movx a,@r0
    add a,#EDBUF
    mov r1,a
    mov r0,#JPLEN
    movx a,@r0
    mov r5,a
    mov r7,#0
    call xr_tx
    call fn_go
    jnz jh_nopath
    mov r0,#NREQ                ; the room again
    mov a,#NQ_REC
    movx @r0,a
    mov r5,#E_REC
    ; fall into jn_get

; jn_get: page 3's GET (NREQ), back to entry r5.
jn_get:
    mov r0,#RETPG
    mov a,#PG_JOIN
    movx @r0,a
    inc r0
    mov a,r5
    movx @r0,a
    GOTO PG_NET,E_RUN

jh_fail:
    mov a,#s_hosts&0FFh
    jmp jn_fail
jh_nohost:
    mov a,#s_host&0FFh
    jmp jn_fail
jh_nopath:
    mov a,#s_path&0FFh
    jmp jn_fail

; jn_same: A = 0 if slot r5's name (reply 32 x r5) is the host (EDBUF,
; JHLEN), any case. Clobbers A, r0-r3, r6, r7.
jn_same:
    mov a,r5
    swap a
    rl a
    mov r2,a
    mov r3,#0
    mov r0,#JHLEN
    movx a,@r0
    mov r7,a
    mov r1,#EDBUF
jsm_l:
    call st_next
    orl a,#20h
    mov r6,a
    movx a,@r1
    orl a,#20h
    xrl a,r6
    jnz jsm_x
    inc r1
    djnz r7,jsm_l
    mov r0,#JHLEN               ; the name has to end there too
    movx a,@r0
    xrl a,#HOSTMAX
    jz jsm_x
    call st_byte
jsm_x:
    ret

    if $ > 0A00h
    error "the join's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h

; jn_rec: the room again, by its place. Not there (a 404 too: the list has
; shrunk), or not it: its neighbours.
jn_rec:
    mov r0,#NRES
    movx a,@r0
    jz jc_reply
    xrl a,#NS_404
    jz jc_near
    jmp jn_neterr
jc_reply:
    STB 0
    xrl a,#1
    jnz jc_near
    mov a,#1
    call len_chk
    jnz jc_near
    clr a
    call jn_is
    jnz jc_near
    clr a
    jmp jn_good
jc_near:
    mov r0,#NREQ
    mov a,#NQ_NEAR
    movx @r0,a
    mov r5,#E_SCAN
    jmp jn_get

; jn_scan: the neighbours: the room among them, or it has gone.
jn_scan:
    mov r0,#NRES
    movx a,@r0
    jz jsc_reply
    xrl a,#NS_404
    jz jsc_gone
    jmp jn_neterr
jsc_reply:
    STB 0
    mov r5,a
    jz jsc_gone
    add a,#256-LB_ASK-1
    jc jsc_gone
    mov a,r5
    mov r0,#TMP1
    movx @r0,a
    call len_chk
    jnz jsc_gone
    mov r0,#TMP0                ; each record, from the first
    clr a
    movx @r0,a
jsc_l:
    mov r0,#TMP0
    movx a,@r0
    call jn_is
    jz jsc_have
    mov r0,#TMP0
    movx a,@r0
    inc a
    movx @r0,a
    mov r6,a
    mov r0,#TMP1
    movx a,@r0
    xrl a,r6
    jnz jsc_l
jsc_gone:
    mov a,#s_gone&0FFh
    jmp jn_fail
jsc_have:
    mov r0,#TMP0
    movx a,@r0
    ; fall into jn_good

; jn_good: record A is the room: its server URL into key JT (best effort:
; the game may do without), then the boot.
jn_good:
    call rec_at
    mov a,#LR_SURL
    call add_r32
    mov r1,#EDBUF
    mov r7,#SURLMAX
    call rx_lb
    mov a,r1
    add a,#256-EDBUF
    jz jg_boot
    mov r0,#TMP2
    movx @r0,a
    mov r0,#JT
    movx a,@r0
    mov r5,a
    mov r1,#1
    call ak_open
    jnz jg_boot
    mov r3,#FC_AKWRITE
    mov r4,#0
    call fuji_begin
    mov r0,#TMP2
    movx a,@r0
    mov r5,a
    mov r7,a
    mov r1,#EDBUF
    call xr_tx
    call fn_go
jg_boot:
    GOTO PG_MOUNT,E_RUN

; jn_is: A = 0 if record A's checksum is JHL/JHH. Clobbers A, r0, r2-r7.
jn_is:
    call jn_sum
    mov r0,#JHL
    movx a,@r0
    xrl a,r4
    jnz ji_x
    inc r0
    movx a,@r0
    xrl a,r5
ji_x:
    ret

; ak_open: OPEN_APPKEY key r5, mode r1 (0 read, 1 write): creator 1, app
; 1, no parameters, a 6-byte payload. A = 0 if it worked. Clobbers A,
; r0-r2, r5-r7.
ak_open:
    mov r3,#FC_AKOPEN
    mov r4,#0
    call fuji_begin
    mov a,#1                    ; creator, LE
    call fn_tx
    clr a
    call fn_tx
    mov a,#1                    ; app
    call fn_tx
    mov a,r5                    ; key
    call fn_tx
    mov a,r1                    ; mode
    call fn_tx
    clr a                       ; reserved
    call fn_tx
    jmp fn_go

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

    if $ > 0B00h
    error "the join's page $A has run out"
    endif

; ===========================================================================
    org 0B00h

; jn_screen: the join's: the room's game (tan) and name, from the list's
; reply. Clobbers A, r0-r7.
jn_screen:
    call scr_text
    mov r4,#L_TOP
    mov a,#s_joining&0FFh
    call say_j
    mov r4,#L_BOT
    mov a,#s_wait&0FFh
    call say_j
    mov a,#C_TAN
    mov r5,#LR_GAME
    mov r4,#2
    call js_row
    mov a,#C_WHITE
    mov r5,#LR_ROOM
    mov r4,#3
    ; fall into js_row

; js_row: row r4 = field r5 of the room's record, NCOLS of it, upper
; case, in colour A. Clobbers A, r0-r7.
js_row:
    mov r0,#TXCOL
    movx @r0,a
    inc r0
    movx @r0,a
    mov r0,#TMP1
    mov a,r4
    movx @r0,a
    call lb_clear
    mov r0,#JREC
    movx a,@r0
    call rec_at
    mov a,r5
    call add_r32
    mov r1,#LINEBUF
    mov r7,#NCOLS
    call rx_lb
    call lb_fold
    mov r0,#RICON
    clr a
    movx @r0,a
    mov r0,#TMP1
    movx a,@r0
    mov r4,a
    jmp row_line

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

; lb_fold: LINEBUF's a-z to A-Z. Clobbers A, r0, r6, r7.
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

    include "local.inc"
    if $ > 0C00h
    error "the join's page $B has run out"
    endif

; ===========================================================================
    org 0C00h
    STRPAGE j

; jn_fail: the reason (the string at offset A of this page) on row 5,
; then a key: the list again. jn_neterr: NRES's.
jn_neterr:
    mov r0,#NRES
    movx a,@r0
    inc a
    jnz jne_hex
    mov a,#s_noans&0FFh
    jmp jn_fail
jne_hex:
    mov a,#s_neterr&0FFh
    call lbs0_j
    mov r0,#NRES
    movx a,@r0
    mov r6,a
    swap a
    call hx_dig
    mov r0,#LINEBUF+10
    movx @r0,a
    mov a,r6
    call hx_dig
    mov r0,#LINEBUF+11
    movx @r0,a
    jmp jf_show
jn_fail:
    call lbs0_j
jf_show:
    mov a,#SND_ERROR
    call beep
    mov r4,#5
    call row_str
    mov r4,#L_BOT
    mov a,#s_anykey&0FFh
    call say_j
jf_wait:
    call in_poll
    mov r0,#KEY
    movx a,@r0
    cpl a
    jnz jf_back
    mov r0,#JOYEV
    movx a,@r0
    jb4 jf_back
    jmp jf_wait
jf_back:
    GOTO PG_LIST,E_FETCH

; hx_dig: A's low nibble as a hex digit.
hx_dig:
    anl a,#0Fh
    add a,#256-10
    jc hd_af
    add a,#10+'0'
    ret
hd_af:
    add a,#'A'
    ret

s_joining:  db "    JOINING",0
s_wait:     db "  PLEASE WAIT",0
s_anykey:   db "  PRESS A KEY",0
s_notgame:  db " NOT A GAME",0
s_badurl:   db "BAD GAME URL",0
s_hosts:    db "HOSTS ERROR",0
s_host:     db " HOST ERROR",0
s_path:     db " PATH ERROR",0
s_gone:     db " ROOM GONE",0
s_noans:    db " NO ANSWER",0
s_neterr:   db "NET ERROR",0
    if $ > 0D00h
    error "the join's page $C has run out"
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
