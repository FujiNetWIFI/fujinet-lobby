; appkey.inc -- the username (creator 1 / app 1 / key 0) and the room
; handoff (key <game_type>).
;
; Google Calendar's shape (fujinet-google-calendar/astrocade/appkey.inc):
; the open payload is six RAW tx bytes -- not the size-prefixed FNPARB
; the N: device wants -- and the reserved sixth byte must be sent or the
; firmware blocks waiting for it. The read reply is a 2-byte little-endian
; length ahead of the payload, small enough to sit in slice 0.

; AKOPEN: A = mode (AKMRD / AKMWR), C = key. Carry/err from FNCOMMIT.
AKOPEN: PUSH    BC
        PUSH    AF
        LD      A,FNDFUJI
        LD      E,FCOKEY
        LD      L,0
        CALL    FNBEGIN
        LD      A,AKCRLO
        CALL    FNTXBYT
        LD      A,AKCRHI
        CALL    FNTXBYT
        LD      A,AKAPP
        CALL    FNTXBYT
        POP     AF
        POP     BC
        PUSH    AF
        LD      A,C
        CALL    FNTXBYT         ; key
        POP     AF
        CALL    FNTXBYT         ; mode
        XOR     A
        CALL    FNTXBYT         ; reserved
        LD      B,3
        JP      FNCOMMIT

AKCLOSE: LD     A,FNDFUJI
        LD      E,FCCKEY
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        JP      FNCOMMIT

; AKWRITE: key C = the NUL-terminated string at HL, sent without its NUL
; (the C clients' write_appkey(..., strlen)). Carry set on failure.
AKWRITE: PUSH   HL
        LD      A,AKMWR
        CALL    AKOPEN
        POP     HL
        RET     C
        JR      NZ,AKWF
        PUSH    HL
        LD      A,FNDFUJI
        LD      E,FCWKEY
        LD      L,0
        CALL    FNBEGIN
        POP     HL
        CALL    FNTXSTR
        LD      B,3
        CALL    FNCOMMIT
        PUSH    AF
        CALL    AKCLOSE
        POP     AF
        RET     C
        OR      A
        RET     Z
AKWF:   SCF
        RET

; NAMGET: the shared username into PLNBUF, folded to the name wheel's
; alphabet (A-Z, 0-9) and cut at the first character outside it. Z set
; if there is no usable name.
NAMGET: XOR     A
        LD      (PLNBUF),A
        LD      C,AKKNAME
        LD      A,AKMRD
        CALL    AKOPEN
        JR      C,NAMG9
        JR      NZ,NAMG9
        LD      A,FNDFUJI
        LD      E,FCRKEY
        LD      L,0
        CALL    FNBEGIN
        LD      B,3
        CALL    FNCOMMIT
        JR      C,NAMG8
        JR      NZ,NAMG8
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,NAMG8
        LD      A,(FNRDATA+1)   ; the LE length prefix; names are short
        OR      A
        LD      A,(FNRDATA)
        JR      Z,NAMG1
        LD      A,8
NAMG1:  CP      9
        JR      C,NAMG2
        LD      A,8
NAMG2:  OR      A
        JR      Z,NAMG8
        LD      B,A
        LD      HL,FNRDATA+2
        LD      DE,PLNBUF
NAMG3:  LD      A,(HL)
        CP      'a'             ; fold lowercase
        JR      C,NAMG4
        CP      'z'+1
        JR      NC,NAMG4
        SUB     20H
NAMG4:  CP      '0'
        JR      C,NAMG7
        CP      '9'+1
        JR      C,NAMG5
        CP      'A'
        JR      C,NAMG7
        CP      'Z'+1
        JR      NC,NAMG7
NAMG5:  LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    NAMG3
NAMG7:  XOR     A
        LD      (DE),A
NAMG8:  CALL    AKCLOSE
NAMG9:  LD      A,(PLNBUF)
        OR      A
        RET

; NAMPUT: PLNBUF back to the shared key. Best effort; the demo keeps it.
NAMPUT:
        IF      DEMO
        RET
        ENDIF
        LD      C,AKKNAME
        LD      HL,PLNBUF
        JP      AKWRITE
