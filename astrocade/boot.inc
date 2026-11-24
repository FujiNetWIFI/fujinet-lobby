; boot.inc -- the trigger: hand the room to its client and boot it.
;
; Nothing is sent to the lobby server; joining is local (the 2600 port's
; README, "The handoff"):
;
;  1. Re-fetch the one record (pagesize=1): the list kept only offsets.
;  2. Copy its server and client URLs to RAM while they are still in the
;     window -- every transaction below repaints it.
;  3. READ_HOST_SLOTS, then WRITE_HOST_SLOTS with slot 8 replaced by the
;     client URL's host. Always slot 8: the other seven hold whatever
;     other clients left, and CONFIG's lobby shortcut claims it too.
;  4. MOUNT_HOST.
;  5. Appkey 1/1/<game_type> = the server URL. This is the handoff: the
;     client reads that key under its own game_type. Best effort -- a
;     client that cannot find its server says so better than we can.
;  6. SET_DEVICE_FULLPATH, MOUNT_IMAGE with a progress bar over
;     FN_R_BOOT_PCT, then BOOTLOCK and the swap -- CONFIG's boot.inc
;     (fujinet-config/astrocade/boot.inc), which this follows closely.

BARY    EQU     8*6+1           ; progress bar: first line, text row 8
BARB    EQU     7               ; raw start byte (px 28)
BARW    EQU     25              ; 25 bytes = 100 px = 4 px per 4 percent
STUB    EQU     4FE0H           ; boot swap stub (6 bytes, runs from RAM)

JOIN:   LD      A,(V_SEL)
        CALL    ROWGET
        LD      (V_OFF),A
        LD      A,1
        LD      (V_PSZ),A
        CALL    CLRSCR
        LD      HL,TJOIN
        LD      DE,0*256+0
        LD      C,XCHROM
        CALL    TXTAT
        CALL    APICALL
        LD      HL,ENET
        JP      C,JFAIL
        LD      HL,0            ; exactly the one record, all of it
        CALL    RXGETB
        CP      1
        JP      NZ,JFBAD
        LD      HL,(RXLEN)
        LD      DE,3+RECLEN
        OR      A
        SBC     HL,DE
        JP      C,JFBAD
        LD      HL,3+RGTYPE
        CALL    RXGETB
        LD      (V_GT),A
        OR      A               ; game_type 0 names no appkey slot
        JP      Z,JFBAD
        LD      HL,3+RGAME      ; what we are joining, while it is here
        LD      DE,LINBUF
        LD      B,16
        CALL    RXSTRN
        LD      HL,LINBUF
        LD      DE,2*256+0
        LD      C,XGAME
        CALL    TXTAT
        LD      HL,3+RSERV
        LD      DE,LINBUF
        LD      B,30
        CALL    RXSTRN
        LD      HL,LINBUF
        LD      DE,3*256+2
        LD      C,XROOM
        CALL    TXTAT
        LD      HL,3+RSURL
        LD      DE,SRVURL
        LD      B,64
        CALL    RXSTRN
        LD      HL,3+RCURL
        LD      DE,CLURL
        LD      B,64
        CALL    RXSTRN
        LD      HL,CLURL
        LD      DE,5*256+0
        LD      B,40
        LD      C,XCHROM
        CALL    TXTN

; Split scheme://host/path: host into HOSTBF, V_PATH -> the '/'.
        LD      HL,CLURL
JS1:    LD      A,(HL)          ; find "://"; without one the whole
        OR      A               ; URL is host/path
        JR      Z,JS3
        CP      ':'
        INC     HL
        JR      NZ,JS1
        LD      A,(HL)
        CP      '/'
        JR      NZ,JS1
        INC     HL
        LD      A,(HL)
        CP      '/'
        JR      NZ,JS1
        INC     HL
        JR      JS4
JS3:    LD      HL,CLURL
JS4:    LD      DE,HOSTBF
        LD      B,32
JS5:    LD      A,(HL)
        OR      A
        JP      Z,JFURL         ; no path at all
        CP      '/'
        JR      Z,JS6
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    JS5
        JP      JFURL           ; a host too long for a slot
JS6:    LD      (V_PATH),HL
        XOR     A
        LD      (DE),A
        LD      A,E             ; an empty host
        CP      HOSTBF-4E00H
        JP      Z,JFURL

        IF      DEMO
        LD      HL,TDHOST       ; DEMO: show the split, a bar, stop here
        LD      DE,7*256+0
        LD      C,XCHROM
        CALL    TXTAT
        LD      HL,HOSTBF
        LD      DE,7*256+6
        LD      C,XROOM
        CALL    TXTAT
        CALL    BARBG
        LD      A,60
        CALL    BARUPD
        LD      HL,TDEMO
        JP      JFAIL
        ELSE

        LD      HL,TNOPOW
        LD      DE,14*256+12
        LD      C,XCHROM
        CALL    TXTAT
        LD      E,FCRHSL        ; host slot 8
        CALL    CDO0
        JP      C,JFHOST
        LD      A,FNDFUJI
        LD      E,FCWHSL
        LD      L,0
        CALL    FNBEGIN
        LD      C,0             ; slot being streamed
JW1:    LD      A,C
        CP      HSLOT
        JR      Z,JW3
        RRCA                    ; the old slot, 32 bytes, from the reply
        RRCA
        RRCA
        LD      L,A
        LD      H,FNRDATA/256
        LD      B,32
JW2:    LD      A,(HL)
        CALL    FNTXBYT
        INC     HL
        DJNZ    JW2
        JR      JW4
JW3:    LD      HL,HOSTBF
        LD      B,32
        CALL    CTXFIX
JW4:    INC     C
        LD      A,C
        CP      8
        JR      C,JW1
        CALL    CGO
        JP      C,JFHOST
        LD      A,HSLOT
        LD      E,FCMHOST
        CALL    CDO1
        LD      HL,EMHOST
        JP      C,JFAIL
        LD      A,(V_GT)        ; the handoff; best effort
        LD      C,A
        LD      HL,SRVURL
        CALL    AKWRITE
        LD      A,FNDFUJI       ; the client image
        LD      E,FCSDFP
        LD      L,3
        CALL    FNBEGIN
        LD      A,DEVSLOT
        CALL    FNPARB
        LD      A,HSLOT
        CALL    FNPARB
        LD      A,FMREAD
        CALL    FNPARB
        LD      C,0
        LD      HL,(V_PATH)
        CALL    CTXSTR
        CALL    CPAD
        CALL    CGO
        LD      HL,EPATH
        JP      C,JFAIL
        JP      JMOUNT
        ENDIF

JFBAD:  LD      HL,EBAD
        JP      JFAIL
JFURL:  LD      HL,EURL
        JP      JFAIL
JFHOST: LD      HL,EHOST

; JFAIL: message HL on the status row; any key goes back to the list.
JFAIL:  CALL    STATUS
        CALL    KEYRAW
        LD      (V_KEY),A
JF1:    CALL    KEYSCN
        OR      A
        JR      Z,JF1
        JP      LSTRDR

        IF      DEMO
        ELSE
JMOUNT: CALL    BARBG
        LD      A,FNDFUJI
        LD      E,FCMIMG
        LD      L,2
        CALL    FNBEGIN
        LD      A,DEVSLOT
        CALL    FNPARB
        LD      A,FMREAD
        CALL    FNPARB
        CALL    BCOMMIT
        JR      NC,JRDY
        LD      A,(FNBSTAT)     ; prefer the boot-state verdict: it names
        CP      FBFAIL          ; the cause
        JR      NC,JBERR
        LD      HL,EMOUNT
        JP      JFAIL
JBERR:  CALL    BERRNAM
        JP      JFAIL
JRDY:   LD      BC,3000         ; ~30 s for the stage to settle
JR1:    LD      A,(FNBSTAT)
        CP      FBREADY
        JR      Z,JGO
        CP      FBFAIL
        JR      NC,JBERR
        PUSH    BC
        LD      B,1
        CALL    WAITB
        POP     BC
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,JR1
        LD      HL,ESTAGE
        JP      JFAIL
JGO:    LD      A,100
        CALL    BARUPD
        LD      C,FRBOOTL
        LD      A,FNBLMAG
        CALL    FNREGWR
        LD      HL,BSTUB
        LD      DE,STUB
        LD      BC,BSTUBL
        LDIR
        JP      STUB
BSTUB:  LD      A,(FNSWAP)      ; the cart swaps between this read and
        JP      0               ; the next fetch; nothing touches cart
BSTUBL  EQU     $-BSTUB         ; space in between -- we run from RAM

; BCOMMIT: commit the staged MOUNT_IMAGE, painting FN_R_BOOT_PCT into the
; bar while the transaction is outstanding. ~120 s budget. Carry set on
; timeout / transport error / NAK.
BCOMMIT: LD     A,0FFH
        LD      (V_TMP),A       ; last percent drawn
        LD      A,(FNACKSQ)
        INC     A
        JR      NZ,BC1
        INC     A               ; 255 wraps past the reserved 0
BC1:    LD      H,A
        LD      C,FRSEQ
        CALL    FNREGWR
        LD      B,60            ; 60 x ~2 s
BCO:    LD      DE,0
BCI:    LD      A,(FNACKSQ)
        CP      H
        JR      Z,BCOK
        LD      A,(FNBPCT)
        LD      L,A
        LD      A,(V_TMP)
        CP      L
        JR      Z,BCSKIP
        LD      A,L
        LD      (V_TMP),A
        PUSH    HL
        PUSH    DE
        PUSH    BC
        CALL    BARUPD
        POP     BC
        POP     DE
        POP     HL
BCSKIP: DEC     DE
        LD      A,D
        OR      E
        JR      NZ,BCI
        DJNZ    BCO
        SCF
        RET
BCOK:   LD      A,(FNERR)
        OR      A
        JR      NZ,BCF
        LD      A,(FNREPLY)
        CP      FCACK
        RET     Z
BCF:    SCF
        RET

; BERRNAM: HL = the message for the FN_R_BOOT_ERR code.
BERRNAM: LD     A,(FNBERR)
        LD      HL,EBTOOB
        CP      1
        RET     Z
        LD      HL,EBTRUNC
        CP      2
        RET     Z
        LD      HL,EBNOMAP
        CP      3
        RET     Z
        LD      HL,ESTAGE
        RET

; ---- Fuji-device command helpers (CONFIG's fujicmd.inc) ---------------

; CGO: commit (8 quanta) and validate. Carry set on timeout, transport
; error or NAK. Clobbers everything.
CGO:    LD      B,8
        CALL    FNCOMMIT
        RET     C
        JR      NZ,CGOF
        LD      A,(FNREPLY)
        CP      FCACK
        RET     Z
CGOF:   SCF
        RET

; CDO0: run the 0-param command E. CDO1: with the 1-byte param A.
CDO0:   LD      A,FNDFUJI
        LD      L,0
        CALL    FNBEGIN
        JR      CGO
CDO1:   PUSH    AF
        LD      A,FNDFUJI
        LD      L,1
        CALL    FNBEGIN
        POP     AF
        CALL    FNPARB
        JR      CGO

; CTXSTR: stream the NUL-terminated string at HL, adding its length to C.
CTXSTR: LD      A,(HL)
        OR      A
        RET     Z
        CALL    FNTXBYT
        INC     HL
        INC     C
        JR      CTXSTR

; CTXFIX: stream exactly B bytes from HL, zeros once a NUL is hit.
CTXFIX: LD      A,(HL)
        OR      A
        JR      Z,CTXF0
        CALL    FNTXBYT
        INC     HL
        DJNZ    CTXFIX
        RET
CTXF0:  XOR     A
        CALL    FNTXBYT
        DJNZ    CTXF0
        RET

; CPAD: zeros until C bytes total reach 256 (the firmware fails a short
; SET_DEVICE_FULLPATH read).
CPAD:   LD      B,C
CPAD1:  XOR     A
        CALL    FNTXBYT
        INC     B
        JR      NZ,CPAD1
        RET
        ENDIF

; BARBG: the empty bar, 6 lines of color 1.
BARBG:  LD      HL,NORMEM+BARY*BYTEPL+BARB
        LD      D,55H
        LD      C,BARW
        LD      B,6
        JP      FILLRC

; BARUPD: A = percent 0-100: fill pct/4 bytes of the bar in color 3,
; the percentage under it.
BARUPD: PUSH    AF
        SRL     A
        SRL     A
        JR      Z,BAR2
        CP      BARW+1
        JR      C,BAR1
        LD      A,BARW
BAR1:   LD      C,A
        LD      HL,NORMEM+BARY*BYTEPL+BARB
        LD      D,0FFH
        LD      B,6
        CALL    FILLRC
BAR2:   POP     AF
        LD      HL,LINBUF
        CALL    PUTDEC
        LD      (HL),'%'
        INC     HL
        LD      (HL),0
        LD      HL,LINBUF
        LD      DE,10*256+18
        LD      B,4
        LD      C,XCHROM
        JP      TXTN

TJOIN:  DB      "JOINING",0
TNOPOW: DB      "DO NOT POWER OFF",0
EURL:   DB      "BAD CLIENT URL - ANY KEY",0
EHOST:  DB      "HOST SLOT ERROR - ANY KEY",0
EMHOST: DB      "HOST MOUNT ERROR - ANY KEY",0
EPATH:  DB      "PATH ERROR - ANY KEY",0
EMOUNT: DB      "MOUNT ERROR - ANY KEY",0
ESTAGE: DB      "STAGE ERROR - ANY KEY",0
EBTOOB: DB      "ROM TOO BIG - ANY KEY",0
EBTRUNC: DB     "TRUNCATED XFER - ANY KEY",0
EBNOMAP: DB     "NO MAPPING - ANY KEY",0
        IF      DEMO
TDHOST: DB      "HOST",0
TDEMO:  DB      "DEMO BUILD: NO BOOT - ANY KEY",0
        ENDIF
