; p7.inc -- page 7: the help (the O2 CONFIG's), and the list's screen when
; it has nothing to list (E_NOLIST). A screen's keys (HELPCTX), a page of eight
; rows, keys in tan; the editor's has a second page, the shifts' chart. A
; key or the button turns the page, after the last one goes back (HRET:
; entry << 4 | page); the editor's closed with ? types a ? there (FL_HELPQ).
;
; Each context's rows sit in one 256-byte page with their table and a copy
; of the drawer (HBANK): MOVP reads its own page only.

; HBANK: the drawer for this page's help, suffix SFX: hdraw_SFX draws the
; help page whose table (title, then 8 rows: string offsets, 0 blank) is at
; offset A of this page.
HBANK   macro SFX,{GLOBALSYMBOLS}
    STRPAGE SFX
hdraw_SFX:
    mov r0,#TMP0
    movx @r0,a
    movp a,@a
    mov r4,#L_TOP
    call sayk_SFX
    mov r4,#0
SFX_hrow:
    mov r0,#TMP1
    mov a,r4
    movx @r0,a
    mov r0,#TMP0
    movx a,@r0
    inc a
    movx @r0,a
    movp a,@a
    call rowk_SFX
    mov r0,#TMP1
    movx a,@r0
    inc a
    mov r4,a
    xrl a,#NROWS
    jnz SFX_hrow
    ret
; sayk_SFX, rowk_SFX: the string at offset A (0: blank) on line / row r4,
; white, its keys (bit 7) tan.
sayk_SFX:
    call SFX_hlb
    jmp say_line_c
rowk_SFX:
    call SFX_hlb
    mov r0,#RICON
    clr a
    movx @r0,a
    jmp row_line
SFX_hlb:
    mov r6,a
    call lb_clear
    mov a,r6
    jz SFX_hcol
    call lbs_SFX
SFX_hcol:
    mov a,#C_WHITE
    mov r0,#TXCOL
    movx @r0,a
    inc r0
    mov a,#C_TAN
    movx @r0,a
    ret
    endm

; KY: a key's name, tan (bit 7).
KY      function c,c|80h

    PAGE_HEAD
    db e_help&0FFh              ; E_RUN
    db e_nolist&0FFh            ; E_NOLIST
e_help:
    jmp hp_run
e_nolist:
    jmp nl_run

; hp_run: the context's first page.
hp_run:
    mov r0,#HPAGE
    clr a
    movx @r0,a
hp_draw:
    call scr_text
    mov r0,#HPAGE               ; the page: context x 2 + its page
    movx a,@r0
    mov r6,a
    mov r0,#HELPCTX
    movx a,@r0
    rl a
    add a,r6
    add a,#hp_tab&0FFh
    jmpp @a
hp_tab:                         ; by context x 2 + page
    db hp_list&0FFh, hp_last&0FFh
    db hp_edit1&0FFh, hp_edit2&0FFh
hp_list:
    mov a,#ht_list&0FFh
    call hdraw_a
    jmp hp_wait
hp_edit1:
    mov a,#ht_edit1&0FFh
    call hdraw_e
    jmp hp_wait
hp_edit2:
    mov a,#ht_edit2&0FFh
    call hdraw_d
    jmp hp_wait
hp_last:
    jmp hp_back

; hp_wait: a key or the button: the next page (the last: back). ? in the
; editor's: back, and a ? typed there.
hp_wait:
    call in_poll
    mov r0,#JOYEV
    movx a,@r0
    jb4 hp_next
    mov r0,#KEY
    movx a,@r0
    mov r5,a
    cpl a
    jz hp_wait
    mov a,r5
    xrl a,#K_QUEST
    jnz hp_next
    mov r0,#HELPCTX
    movx a,@r0
    xrl a,#HC_EDIT
    jnz hp_back
    mov r0,#FLAGS
    movx a,@r0
    orl a,#FL_HELPQ
    movx @r0,a
    jmp hp_back
hp_next:
    mov a,#SND_MOVE
    call beep
    mov r0,#HPAGE
    movx a,@r0
    jnz hp_back
    inc a
    movx @r0,a
    jmp hp_draw
hp_back:
    call spr_hide
    mov r0,#HRET
    movx a,@r0
    swap a
    anl a,#0Fh
    mov r7,a
    movx a,@r0
    anl a,#0Fh
    jmp far_go

    include "local.inc"
    include "beep.inc"
    if $ > 900h
    error "the help's page $8 has run out"
    endif

; ===========================================================================
    org 0900h
    HBANK a
ht_list:
    db hl_t&0FFh, hl_1&0FFh, hl_2&0FFh, hl_3&0FFh, hl_4&0FFh
    db hl_5&0FFh, hl_6&0FFh, hl_7&0FFh, hl_8&0FFh
hl_t:   db "  HELP: LOBBY",0
hl_1:   db KY('S'),KY('T'),KY('I'),KY('C'),KY('K'),"  ROOM",0
hl_2:   db KY(CH_ARROW),"     PAGE",0
hl_3:   db KY('F'),KY('I'),KY('R'),KY('E'),"   JOIN",0
hl_4:   db KY('E'),KY('N'),KY('T'),"    JOIN",0
hl_5:   db KY('R'),"    RELOAD",0
hl_6:   db KY('N'),"      NAME",0
hl_7:   db "NAME: 2 TO 8",0
hl_8:   db "OF A-Z 0-9",0
    if $ > 0A00h
    error "the help's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h
    STRPAGE n
; sayk_n: line r4 = the string at offset A of page $A, white, its bit-7
; keys tan.
sayk_n:
    call lbs0_n
    mov a,#C_WHITE
    mov r0,#TXCOL
    movx @r0,a
    inc r0
    mov a,#C_TAN
    movx @r0,a
    jmp say_line_c


; nl_run: the list has nothing, and why (NRES) on rows 3 and 4. R or the
; button: the list from its start; N: the name; ?: the help.
nl_run:
    call scr_text
    call nl_title
    mov r4,#L_BOT
    mov a,#s_nlkeys&0FFh
    call sayk_n
    mov r0,#NRES
    movx a,@r0
    mov r5,a
    xrl a,#NS_404
    jz nl_empty
    mov a,r5
    xrl a,#NR_BAD
    jz nl_bad
    mov a,r5
    xrl a,#NR_NONE
    jz nl_noans
    mov r4,#3                   ; an nDevStatus: NETWORK ERROR xx
    mov a,#s_net1&0FFh
    call row_n
    mov a,#s_net2&0FFh
    call lbs0_n
    mov r0,#NRES
    movx a,@r0
    mov r6,a
    swap a
    call nh_dig
    mov r0,#LINEBUF+7
    movx @r0,a
    mov a,r6
    call nh_dig
    mov r0,#LINEBUF+8
    movx @r0,a
    mov r4,#4
    call row_str
    jmp nl_beep
nl_empty:
    mov r4,#3
    mov a,#s_none1&0FFh
    call row_n
    mov r4,#4
    mov a,#s_none2&0FFh
    call row_n
    jmp nl_beep
nl_bad:
    mov r4,#3
    mov a,#s_bad&0FFh
    call row_n
    jmp nl_beep
nl_noans:
    mov r4,#3
    mov a,#s_net1&0FFh
    call row_n
    mov r4,#4
    mov a,#s_noans&0FFh
    call row_n
nl_beep:
    mov a,#SND_ERROR
    call beep
    jmp nl_loop

s_nlkeys:   db 'R'|80h," RELOAD ",'?'|80h," HELP",0
s_none1:    db "  NO ROOMS",0
s_none2:    db "   OPEN",0
s_bad:      db " BAD REPLY",0
s_net1:     db "  NETWORK",0
s_net2:     db " ERROR",0
s_noans:    db " NO ANSWER",0
    if $ > 0B00h
    error "the help's page $A has run out"
    endif

; ===========================================================================
    org 0B00h
    STRPAGE t
; nl_title: T0: the name (the list's T0, without its page). Clobbers A,
; r0-r7.
nl_title:
    call lb_clear
    mov r0,#NAMEBUF
    movx a,@r0
    jz nt_anon
    mov a,#s_lobby&0FFh
    call lbs_t
    mov r1,#NAMEBUF
    mov r2,#NAMEMAX
nt_nm:
    movx a,@r1
    jz nt_say
    movx @r0,a
    inc r0
    inc r1
    djnz r2,nt_nm
    jmp nt_say
nt_anon:
    mov a,#s_title&0FFh
    call lbs_t
nt_say:
    mov r4,#L_TOP
    jmp say_line

s_title:    db "   GAME LOBBY",0
s_lobby:    db "LOBBY ",0

nl_loop:
    call in_poll
    mov r0,#JOYEV
    movx a,@r0
    jb4 nl_reload
    mov r0,#KEY
    movx a,@r0
    mov r5,a
    xrl a,#K_R
    jz nl_reload
    mov a,r5
    xrl a,#K_NO                 ; N
    jz nl_name
    mov a,r5
    xrl a,#K_QUEST
    jz nl_help
    mov a,r5
    xrl a,#K_H
    jz nl_help
    jmp nl_loop
nl_reload:
    GOTO PG_LIST,E_RUN
nl_name:
    GOTO PG_NAME,E_EDIT
nl_help:
    HELP HC_LIST,PG_HELP,E_NOLIST

; nh_dig: A's low nibble as a hex digit.
nh_dig:
    anl a,#0Fh
    add a,#256-10
    jc nh_af
    add a,#10+'0'
    ret
nh_af:
    add a,#'A'
    ret

    if $ > 0C00h
    error "the help's page $B has run out"
    endif

; ===========================================================================
    org 0C00h
    HBANK d
ht_edit2:
    db hc_t&0FFh, hc_1&0FFh, hc_2&0FFh, hc_3&0FFh, hc_4&0FFh
    db hc_5&0FFh, hc_6&0FFh, hc_7&0FFh, hc_8&0FFh
hc_t:   db KY(CH_TIMES),"SHIFT ",KY(CH_DIV),"SYMBOL",0
hc_1:   db KY(CH_TIMES),": ",KY('1'),"! ",KY('2'),"@ ",KY('3'),"#",0
hc_2:   db KY('4'),"$ ",KY('5'),"% ",KY('6'),"^ ",KY('7'),"&",0
hc_3:   db KY('8'),"* ",KY('9'),"( ",KY('0'),") ",KY('?'),22h,0
hc_4:   db KY('+'),"; ",KY('.'),", ",KY('-'),"_ ",KY('='),":",0
hc_5:   db KY(CH_DIV),": ",KY('?'),"' ",KY('+'),"] ",KY('='),"[",0
hc_6:   db KY('.'),"> ",KY('-'),"< ",KY(CH_TIMES),"| ",KY(CH_DIV),"/",0
hc_7:   db KY('1'),"` ",KY('2'),"~ ",KY('9'),"{ ",KY('0'),"}",0
hc_8:   db "EXACT: ",KY(CH_ARROW),"TOP",0
    if $ > 0D00h
    error "the help's page $C has run out"
    endif

; ===========================================================================
    org 0D00h
    HBANK e
ht_edit1:
    db he_t&0FFh, he_1&0FFh, he_2&0FFh, he_3&0FFh, he_4&0FFh
    db he_5&0FFh, he_6&0FFh, he_7&0FFh, he_8&0FFh
he_t:   db "  HELP: TYPING",0
he_1:   db KY(CH_TIMES),"    SHIFT",0
he_2:   db KY(CH_TIMES),KY(CH_TIMES),"    LOCK",0
he_3:   db KY(CH_DIV),"  SYMBOLS",0
he_4:   db KY('C'),KY('L'),KY('R'),"  RUB OUT",0
he_5:   db KY(CH_TIMES),KY('C'),KY('L'),KY('R'),"  DELETE",0
he_6:   db KY(CH_DIV),KY('C'),KY('L'),KY('R'),"  CLEAR",0
he_7:   db KY('E'),KY('N'),KY('T'),"      OK",0
he_8:   db KY(CH_TIMES),KY('E'),KY('N'),KY('T'),"  CANCEL",0
    if $ > 0E00h
    error "the help's page $D has run out"
    endif

; ===========================================================================
    org 0E00h
    include "input.inc"
    include "snd.inc"

    PAGE_TAIL
