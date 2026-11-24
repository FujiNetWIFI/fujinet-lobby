; demo.inc -- DEMO=1: APICALL answered from a canned room list.
;
; build/demodata.inc (tools/mkdemo.py) holds DEMON records in the real
; /view?bin=1 layout; this stands in for net.inc and state.inc's DEMO
; RXGETB serves the "reply" out of it, so the list, grouping, bar and
; join parsing run exactly as they do live -- with zero network, for
; screenshots and palette work. An offset past the end answers like the
; server's empty page: an HTTP error status.

APICALL: XOR    A
        LD      (V_HTTPE),A
        LD      A,(V_OFF)
        LD      C,A
        LD      A,DEMON         ; records from V_OFF on
        SUB     C
        JR      Z,DA404
        JR      C,DA404
        LD      HL,V_PSZ        ; ...at most V_PSZ of them
        CP      (HL)
        JR      C,DA1
        LD      A,(HL)
DA1:    LD      (V_DCNT),A
        CALL    RECOFF          ; the length a real reply would have
        LD      (RXLEN),HL
        LD      HL,DEMREC       ; V_DPTR = DEMREC + V_OFF * RECLEN
        LD      DE,RECLEN
        LD      B,C
        INC     B
DA2:    DEC     B
        JR      Z,DA3
        ADD     HL,DE
        JR      DA2
DA3:    LD      (V_DPTR),HL
        OR      A
        RET
DA404:  LD      A,1
        LD      (V_HTTPE),A
        SCF
        RET

DEMNAM: DB      "THOMC",0

        INCLUDE "build/demodata.inc"
