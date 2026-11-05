; p9.inc -- page 9: the player's name, key 0 (creator 1, app 1), which the
; games read too; NAMEBUF keeps it for the list's T0 (FL_NAME). The first
; run without one asks for one; N on the list asks again. A name is 2-8 of
; A-Z and 0-9, the editor's lower case folded up: the strictest rule any O2
; game has (M.U.L.E.'s).

    PAGE_HEAD
    db e_read&0FFh              ; E_RUN: read it, then the list
    db e_edit&0FFh              ; E_EDIT: the editor on it
    db e_named&0FFh             ; E_NAMED: the editor is back
e_read:
    jmp nm_run
e_edit:
    jmp nm_editn
e_named:
    jmp nm_named

; nm_run: key 0 into NAMEBUF; none (the first time): the editor, on what
; key 0 held.
nm_run:
    call nm_read
    jz nm_list
    mov r0,#FLAGS
    movx a,@r0
    jb1 nm_list
    orl a,#FL_ASKED
    movx @r0,a
    jmp nm_edit
nm_list:
    GOTO PG_LIST,E_FETCH

; nm_editn: the editor on NAMEBUF. nm_edit: on EDBUF as it is.
nm_editn:
    mov r0,#NAMEBUF
    mov r1,#EDBUF
    mov r2,#NAMEMAX
    mov r3,#0
ne_l:
    movx a,@r0
    jz ne_len
    movx @r1,a
    inc r0
    inc r1
    inc r3
    djnz r2,ne_l
ne_len:
    mov r0,#EDLEN
    mov a,r3
    movx @r0,a
nm_edit:
    mov r0,#EDKIND
    mov a,#ED_NAME
    movx @r0,a
    inc r0                      ; EDMAX
    mov a,#NAMEMAX
    movx @r0,a
    mov r0,#RETPG
    mov a,#PG_NAME
    movx @r0,a
    inc r0
    mov a,#E_NAMED
    movx @r0,a
    GOTO PG_EDIT,E_RUN

; nm_named: the editor is back. Accepted and good: NAMEBUF, and key 0
; written; not good: the editor again, folded. Cancelled: as it was. Then
; the list, the bar where it was.
nm_named:
    mov r0,#EDRES
    movx a,@r0
    jz nn_back
    mov r0,#EDLEN
    movx a,@r0
    mov r4,a
    mov r1,#EDBUF
    call nm_valid
    jz nn_ok
    mov a,#SND_ERROR
    call beep
    jmp nm_edit
nn_ok:
    mov r0,#EDLEN
    movx a,@r0
    mov r4,a
    mov r0,#EDBUF
    call nm_take
    call nm_save
    jnz nn_back                 ; (not saved: this session has it anyway)
    mov a,#SND_OK
    call beep
nn_back:
    mov r0,#LCUR
    mov a,#LC_KEEP
    movx @r0,a
    jmp nm_list

; nm_take: NAMEBUF = r4 bytes of external RAM from r0, NUL-ended if short;
; FL_NAME. Clobbers A, r0, r1, r4.
nm_take:
    mov r1,#NAMEBUF
nt_l:
    movx a,@r0
    movx @r1,a
    inc r0
    inc r1
    djnz r4,nt_l
    mov a,r1                    ; (NAMEBUF ends external RAM: no NUL past it)
    xrl a,#NAMEBUF+NAMEMAX
    jz nt_flag
    clr a
    movx @r1,a
nt_flag:
    mov r0,#FLAGS
    movx a,@r0
    orl a,#FL_NAME
    movx @r0,a
    ret

; nm_read: key 0. EDBUF gets its first NAMEMAX bytes as they are (EDLEN:
; the editor's), NAMEBUF the name if it's good, else none. FL_NAME either
; way. A = 0 for a name. Two tries: the first command after a power-on can
; meet a cold link. Clobbers A, r0-r7.
nm_read:
    mov r5,#AK_NAME
    call ak_read
    jnz nr_have
    mov r5,#AK_NAME
    call ak_read
nr_have:
    mov r4,a                    ; its length (0FFh: too long)
    mov r0,#EDLEN
    clr a
    movx @r0,a
    mov r0,#NAMEBUF
    movx @r0,a
    mov r0,#FLAGS
    movx a,@r0
    orl a,#FL_NAME
    movx @r0,a
    mov a,r4
    jz nr_bad
    mov r7,#NAMEMAX             ; the editor's: NAMEMAX at most
    add a,#256-NAMEMAX-1
    jc nr_cp
    mov a,r4
    mov r7,a
nr_cp:
    mov r3,#0
    mov r2,#2
    mov r1,#EDBUF
    call rx_lb
    mov a,r1
    add a,#256-EDBUF
    mov r0,#EDLEN
    movx @r0,a
    mov a,r4                    ; longer than a name: no good
    add a,#256-NAMEMAX-1
    jc nr_bad
    mov r0,#EDLEN
    movx a,@r0
    jz nr_bad
    mov r4,a
    mov r5,a
    mov r0,#EDBUF
    call nm_take
    mov a,r5
    mov r4,a
    mov r1,#NAMEBUF
    call nm_valid
    jz nr_x
nr_bad:
    mov r0,#NAMEBUF
    clr a
    movx @r0,a
    inc a
nr_x:
    ret

    if $ > 900h
    error "the name's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; nm_save: key 0 = NAMEBUF. A = 0 if it was written. Clobbers A, r0-r7.
nm_save:
    mov r5,#AK_NAME
    mov r1,#1
    call ak_open
    jnz ns_x
    mov r3,#FC_AKWRITE
    mov r4,#0
    call fuji_begin
    mov r1,#NAMEBUF             ; its length
    mov r5,#0
ns_len:
    movx a,@r1
    jz ns_tx
    inc r1
    inc r5
    mov a,r5
    xrl a,#NAMEMAX
    jnz ns_len
ns_tx:
    mov a,r5
    mov r7,a
    mov r1,#NAMEBUF
    call xr_tx
    jmp fn_go
ns_x:
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

; ak_read: key r5 into the reply: its length (2 bytes), then the key. A =
; the length; 0: none; 0FFh: 256 or more. Clobbers A, r0-r7.
ak_read:
    mov r1,#0
    call ak_open
    jnz akr_none
    mov r3,#FC_AKREAD
    mov r4,#0
    call fuji_begin
    call fn_go
    jnz akr_none
    STB 1
    jnz akr_long
    STB 0
    ret
akr_long:
    mov a,#0FFh
    ret
akr_none:
    clr a
    ret

; nm_valid: A = 0 if the r4 bytes of external RAM from r1 are a name, 2-8
; of A-Z and 0-9; a-z are folded to A-Z where they lie. Clobbers A, r1, r4,
; r6.
nm_valid:
    mov a,r4
    add a,#256-2
    jnc nv_bad
    mov a,r4
    add a,#256-NAMEMAX-1
    jc nv_bad
nv_l:
    movx a,@r1
    mov r6,a
    add a,#256-'a'
    jnc nv_up
    mov a,r6
    add a,#256-'z'-1
    jc nv_bad
    mov a,r6
    add a,#256-20h
    movx @r1,a
    mov r6,a
nv_up:
    mov a,r6
    add a,#256-'0'
    jnc nv_bad
    mov a,r6
    add a,#256-'9'-1
    jnc nv_ok
    mov a,r6
    add a,#256-'A'
    jnc nv_bad
    mov a,r6
    add a,#256-'Z'-1
    jc nv_bad
nv_ok:
    inc r1
    djnz r4,nv_l
    clr a
    ret
nv_bad:
    mov a,#1
    ret

    include "local.inc"
    if $ > 0A00h
    error "the name's page $9 has run out"
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
