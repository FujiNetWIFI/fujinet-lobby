; state.inc -- reading the reply where it lies (5 Card Stud's, plus the
; DEMO build's canned reply).
;
; The reply stays painted in the cart's 256-byte slice window (FNRDATA)
; until the next transaction, so nothing is ever buffered: these helpers
; give the rest of the program a flat 0-1023 offset space over it, and
; switch slices only when a read crosses a 256-byte boundary. CURSLC
; caches the slice the cart is currently publishing; every transaction
; ends with the cart republishing slice 0 and net.inc re-homing the cache.

; RXGETB: A = reply byte at offset HL. Preserves HL, BC, DE.
        IF      DEMO
; DEMO: the reply "window" is demo.inc's canned list -- a synthesized
; header {V_DCNT, 0, 0} and then the records from V_DPTR (demo.inc's
; APICALL sets both), so the real parse and draw code runs unchanged.
RXGETB: PUSH    DE
        PUSH    HL
        LD      DE,3
        OR      A
        SBC     HL,DE
        JR      NC,RXD1
        ADD     HL,DE           ; the header
        LD      A,L
        OR      A
        LD      A,(V_DCNT)
        JR      Z,RXD9
        XOR     A
        JR      RXD9
RXD1:   LD      DE,(V_DPTR)
        ADD     HL,DE
        LD      A,(HL)
RXD9:   POP     HL
        POP     DE
        RET
        ELSE
RXGETB: PUSH    BC
        PUSH    DE
        PUSH    HL
        LD      A,H             ; slice = offset high byte (0-3)
        LD      HL,CURSLC
        CP      (HL)
        JR      Z,RXG1
        LD      (HL),A
        CALL    FNSLICE         ; repaints in microseconds; a timeout here
RXG1:   POP     HL              ; means the cart is gone anyway
        PUSH    HL
        LD      D,3BH           ; FNRDATA page
        LD      E,L
        LD      A,(DE)
        POP     HL
        POP     DE
        POP     BC
        RET

        ENDIF

; RXGETW: DE = little-endian word at offset HL. Preserves HL, BC.
RXGETW: CALL    RXGETB
        LD      E,A
        INC     HL
        CALL    RXGETB
        LD      D,A
        DEC     HL
        RET

; RXSTRN: copy at most B bytes from offset HL into (DE), stopping at a
; NUL; the destination is always NUL-terminated. Preserves HL, DE.
RXSTRN: PUSH    HL
        PUSH    DE
RXS1:   CALL    RXGETB
        OR      A
        JR      Z,RXS9
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    RXS1
RXS9:   XOR     A
        LD      (DE),A
        POP     DE
        POP     HL
        RET
