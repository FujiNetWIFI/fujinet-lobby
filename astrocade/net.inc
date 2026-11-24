; net.inc -- the N: device round trip.
;
; 5 Card Stud's (fujinet-5cardstud/astrocade/net.inc), itself a Z80
; transcription of o2/net.inc's proven sequence, over fujilib's mailbox.
; The lobby changes: one request shape (url.inc), a 1K read clamp, and
; V_HTTPE telling an HTTP error status (the lobby's empty-page 404) apart
; from a dead network. The rules it carries forward (learned the hard way there and on
; the Intellivision):
;
;  * The CLOSE belongs at the START of the next request, never after the
;    READ: every transaction repaints the whole reply window, a CLOSE
;    reply is empty, and every screen renders straight out of the window
;    between polls.
;  * FNRXLL/FNRXLH belong to the most recent transaction, so the reply
;    length is captured immediately after the READ.
;  * The ESP32 reports STATUS as soon as SOME of the response has
;    arrived; poll until two consecutive readings agree (bounded) rather
;    than guessing a delay.
;  * The 4th STATUS reply byte is nDevStatus_t and checking it is not
;    optional: the HTTP GET is deferred until the first STATUS, and an
;    HTTP error page still has a readable body.
;  * Validate the reply length against what the count implies before
;    believing a byte of it (list.inc does, per chunk).
;
; All routines return carry SET on failure, clear on success.

; NCMD: begin a network-device transaction. E = command, L = nparam.
NCMD:   LD      A,NETDEV
        JP      FNBEGIN

; NCLOSE: close the previous request's connection. A first call has
; nothing to close and earns a NAK; both results are ignored.
NCLOSE: LD      E,NCCLOSE
        LD      L,0
        CALL    NCMD
        LD      B,3             ; ~6 s
        JP      FNCOMMIT

; NOPEN: OPEN the /view devicespec (url.inc) as HTTP GET, no translation.
NOPEN:  LD      E,NCOPEN
        LD      L,2
        CALL    NCMD
        LD      A,NMHTTPG
        CALL    FNPARB
        LD      A,NTRNONE
        CALL    FNPARB
        CALL    BLDURL
        LD      B,8             ; HTTPS handshakes take a while
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NOBAD
        LD      A,(FNREPLY)
        CP      FCACK
        RET     Z               ; equal: carry already clear
NOBAD:  SCF
        RET

; NSTATUS: one STATUS poll. On success AVAIL = bytes waiting.
NSTATUS: LD     E,NCSTAT
        LD      L,2
        CALL    NCMD
        XOR     A
        CALL    FNPARB
        XOR     A
        CALL    FNPARB
        LD      B,4
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NSBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,NSBAD
        LD      A,(FNRDATA+NSDEVST)
        CP      1               ; SUCCESS -- anything else is an HTTP error
        JR      NZ,NSHTTP
        LD      A,(FNRDATA+NSAVLO)
        LD      (AVAIL),A
        LD      A,(FNRDATA+NSAVHI)
        LD      (AVAIL+1),A
        OR      A               ; clear carry
        RET
NSHTTP: LD      A,1             ; the transport worked; the server said no
        LD      (V_HTTPE),A
NSBAD:  SCF
        RET

; NREAD: READ min(AVAIL, RXMAX) bytes into the reply window, then
; capture the reply length and re-home the slice cursor.
NREAD:  LD      HL,(AVAIL)
        LD      DE,RXMAX
        OR      A
        SBC     HL,DE
        JR      C,NRD1          ; avail < clamp: take avail
        LD      H,D
        LD      L,E
        JR      NRD2
NRD1:   ADD     HL,DE
NRD2:   PUSH    HL
        LD      E,NCREAD
        LD      L,1
        CALL    NCMD
        LD      A,2             ; one 2-byte parameter: the length, LE
        CALL    FNTXBYT
        POP     HL
        PUSH    HL
        LD      A,L
        CALL    FNTXBYT
        POP     HL
        LD      A,H
        CALL    FNTXBYT
        LD      B,8
        CALL    FNCOMMIT
        RET     C
        JR      NZ,NRBAD
        LD      A,(FNREPLY)
        CP      FCACK
        JR      NZ,NRBAD
        LD      A,(FNRXLL)      ; capture NOW: the next transaction's
        LD      (RXLEN),A       ; reply length would replace it
        LD      A,(FNRXLH)
        LD      (RXLEN+1),A
        XOR     A
        LD      (CURSLC),A      ; the cart republished slice 0
        RET                     ; carry clear (XOR A)
NRBAD:  SCF
        RET

; ----------------------------------------------------------------------
; APICALL: one /view round trip for V_PSZ records from V_OFF. On success
; (carry clear) the reply is in the window with the slice cursor homed,
; and stays valid until the next transaction of any kind. On failure
; V_HTTPE is nonzero if the server answered with an error status.
APICALL: XOR    A
        LD      (V_HTTPE),A
        CALL    NCLOSE          ; the PREVIOUS request's connection
        CALL    NOPEN
        RET     C
        XOR     A
        LD      (PRVAVL),A
        LD      (PRVAVL+1),A
        LD      B,20            ; settle-loop bound
APSET:  PUSH    BC
        CALL    NSTATUS
        POP     BC
        JR      C,APCFL
        LD      HL,(AVAIL)
        LD      A,H
        OR      L
        JR      Z,APAGN         ; nothing has arrived at all yet
        LD      DE,(PRVAVL)
        OR      A
        SBC     HL,DE
        JR      Z,APRD          ; two readings agree: it has landed
APAGN:  LD      HL,(AVAIL)
        LD      (PRVAVL),HL
        LD      DE,4000         ; ~3 frames between polls
APDLY:  DEC     DE
        LD      A,D
        OR      E
        JR      NZ,APDLY
        DJNZ    APSET
APCFL:  CALL    NCLOSE          ; failed before the READ: nothing to keep
        SCF
        RET
APRD:   CALL    NREAD
        JR      C,APCFL
        LD      HL,(RXLEN)      ; at least the 3-byte header; list.inc
        LD      DE,3            ; checks the records the count promises
        OR      A
        SBC     HL,DE
        RET                     ; carry = shorter than the header
