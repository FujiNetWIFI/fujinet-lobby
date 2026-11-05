; p0.inc -- page 0: the boot. The cartridge, then the list (page 2), one way.

    PAGE_HEAD
    db e_boot&0FFh              ; entry 0: the boot (fixed.inc start)
e_boot:
    jmp boot

boot:
    call vdc_reset
    call ram_clear
    call ints_on
    call scr_text
    call pal_check
    mov a,#FN_R_MAGIC0&0FFh     ; a FujiNet cartridge, speaking protocol 2?
    call mb_get
    xrl a,#'F'
    jnz bt_nocart
    mov a,#FN_R_MAGIC1&0FFh
    call mb_get
    xrl a,#'N'
    jnz bt_nocart
    mov a,#FN_R_PROTO_VER&0FFh
    call mb_get
    add a,#256-FN_PROTO_VER
    jnc bt_nocart
    GOTO PG_LIST,E_RUN

bt_nocart:
    mov r4,#L_TOP
    mov a,#s_title&0FFh
    call say_e
    mov r4,#2
    mov a,#s_nocart1&0FFh
    call row_e
    mov r4,#3
    mov a,#s_nocart2&0FFh
    call row_e
    mov a,#SND_ERROR
    call beep
bt_stop:
    call in_poll
    jmp bt_stop

; ram_clear: external RAM from TMP0 on to 0, KLAST and KEY to none.
; Clobbers A, r0, r2.
ram_clear:
    mov r0,#TMP0
    mov r2,#80h-TMP0
    clr a
rcx_l:
    movx @r0,a
    inc r0
    djnz r2,rcx_l
    mov a,#K_NONE
    mov r0,#KLAST
    movx @r0,a
    mov r0,#KEY
    movx @r0,a
    ret

; pal_check: a PAL console (MODE_PAL): its frame is a third longer, so a
; frame of the main program's loop counts more passes -- with the bands
; running, as they always are. Loops a frame in TMP1:TMP2 (low, high).
; Clobbers A, r0-r5.
pal_check:
    call frame_wait             ; (a screen just set up: a frame to settle)
    call frame_wait
    call frame_wait
    mov r1,#FRAMES
    mov a,@r1
    mov r2,a
    mov r4,#0
    mov r5,#0
pc_l:                           ; 8 cycles a pass
    inc r4
    mov a,r4
    jnz pc_n
    inc r5
pc_n:
    mov a,@r1
    xrl a,r2
    jz pc_l
    mov r0,#TMP1
    mov a,r4
    movx @r0,a
    inc r0
    mov a,r5
    movx @r0,a
    mov a,r4                    ; PALPASS or more: PAL
    add a,#(10000h-PALPASS)&0FFh
    mov a,r5
    addc a,#(10000h-PALPASS)>>8
    jnc pc_x
    mov r1,#MODE
    mov a,@r1
    orl a,#MODE_PAL
    mov @r1,a
pc_x:
    ret

ints_on:
    clr a
    mov t,a
    strt cnt
    en tcnti
    en i
    ret

    if $ > 900h
    error "page 0's page $8 has run out"
    endif

; ===========================================================================
    org 900h
    include "local.inc"

; ===========================================================================
    org 0A00h
    include "input.inc"
    include "vdcreset.inc"

; ===========================================================================
    org 0B00h
    STRPAGE e
s_title:    db "  FUJINET LOBBY",0
s_nocart1:  db " NO FUJINET",0
s_nocart2:  db " CARTRIDGE",0
    if $ > 0C00h
    error "page 0's page $B has run out"
    endif

; ===========================================================================
    org 0D00h
    include "snd.inc"
    include "beep.inc"

    PAGE_TAIL
