; join.inc -- joining the selected room (block 1).
;
; Nothing goes to the lobby server: a join is a local handoff, the same
; one every console lobby does.
;   1. The record's server and client URLs are copied to RAM (every
;      commit after this repaints the reply window).
;   2. The client URL scheme://host/path is split; the host goes into
;      host slot 8 (READ_HOST_SLOTS, then WRITE_HOST_SLOTS streamed back
;      out of the window with slot 8 replaced), and is mounted.
;   3. The server URL goes to appkey 1/1/<game type>, best effort: the
;      game that boots next reads it from there.
;   4. boot.inc mounts the path and swaps it in (JBOOT).
; Any failure says why and goes back to the list on the same page.

HSLOT   EQU     7               ; host slot 8, as CONFIG's lobby claims

LJOIN:  LODA,R0 V_NROOM
        BCTA,EQ LKLOOP
        LODA,R1 V_CUR
        LODA,R0 V_ROWREC,R1
        STRA,R0 V_RI
        STRZ    R1
        LODI,R0 RTYPE
        ZBSR    *ZRXREC
        ZBSR    *ZRXNEXT
        STRA,R0 V_GTYPE
        BCFR,EQ LJ1
        LODI,R0 ENGAME>>8       ; type 0: not a game the lobby can hand off
        LODI,R1 ENGAME&$FF
LJBAD:  ZBSR    *ZPANEL
        ZBSR    *ZSNDERR
        BCTA,UN LKLOOP
LJ1:    LODA,R1 V_RI
        LODI,R0 RSURL
        ZBSR    *ZRXREC
        LODI,R2 V_SURL&$FF
        LODI,R3 64
        BSTA,UN RXCOPY
        LODA,R1 V_RI
        LODI,R0 RCURL
        ZBSR    *ZRXREC
        LODI,R2 V_CURL&$FF
        LODI,R3 64
        BSTA,UN RXCOPY
        BSTA,UN USPLIT
        BCTR,EQ LJ2
        LODI,R0 EBURL>>8
        LODI,R1 EBURL&$FF
        BCTR,UN LJBAD

LJ2:    ZBSR    *ZSNDSEL
        LODA,R0 V_CUR           ; a failed join comes back to this row
        STRA,R0 V_WANT
        LODI,R0 TJOIN>>8
        LODI,R1 TJOIN&$FF
        ZBSR    *ZUTITLE
        LODA,R1 V_RI            ; the game, yellow
        LODI,R0 RGAME
        ZBSR    *ZRXREC
        DCOL    AACC
        DAT     2,0
        LODI,R0 16
        ZBSR    *ZRXFLD
        LODA,R1 V_RI            ; the room, two rows
        LODI,R0 RSERV
        ZBSR    *ZRXREC
        DCOL    ANORM
        DAT     3,0
        LODI,R0 32
        ZBSR    *ZRXFLD
        PRINT   TNOPOW

        LODI,R0 HSLOT
        STRA,R0 V_HOST
        BSTA,UN HWRITE
        BCTR,EQ LJ3
        STRZ    R2
        LODI,R0 EHOSTS>>8
        LODI,R1 EHOSTS&$FF
        BCTR,UN LJFAIL
LJ3:    BSTA,UN CMOUNT
        BCTR,EQ LJ4
        STRZ    R2
        LODI,R0 EMHOST>>8
        LODI,R1 EMHOST&$FF
LJFAIL: ZBSR    *ZUFAIL
        ZBRR    *ZLSTLD
LJ4:    SETFS   V_SURL
        LODA,R1 V_GTYPE
        BSTA,UN AKWRITE         ; best effort
        LODI,R0 V_CURL>>8       ; FSTRP = the path, for JBOOT
        STRA,R0 FSTRP
        LODA,R0 V_PATHO
        ADDI,R0 V_CURL&$FF
        STRA,R0 FSTRP+1
        BCTA,UN JBOOT

; USPLIT: V_CURL = scheme://host/path. V_HNAME = the host (1-32
; characters), V_PATHO = where the path (with its '/') starts. R0 = 0 /
; CC EQ when it splits. Leaf. Clobbers R0,R1,R3.
USPLIT: LODI,R3 0
US1:    LODA,R0 V_CURL,R3
        BCTR,EQ USBAD           ; no "://"
        COMI,R0 ':'
        BCFR,EQ US2
        LODA,R0 V_CURL+1,R3
        COMI,R0 '/'
        BCFR,EQ US2
        LODA,R0 V_CURL+2,R3
        COMI,R0 '/'
        BCTR,EQ US3
US2:    ADDI,R3 1
        BCTR,UN US1
US3:    ADDI,R3 3
        LODI,R1 0
US4:    LODA,R0 V_CURL,R3
        BCTR,EQ USBAD           ; no path
        COMI,R0 '/'
        BCTR,EQ US5
        COMI,R1 32
        BCTR,EQ USBAD           ; a host longer than a slot
        STRA,R0 V_HNAME,R1
        ADDI,R1 1
        ADDI,R3 1
        BCTR,UN US4
US5:    COMI,R1 0
        BCTR,EQ USBAD           ; no host
        EORZ    R0
        STRA,R0 V_HNAME,R1
        STRA,R3 V_PATHO
        RETC,UN
USBAD:  LODI,R0 1
        RETC,UN

; HWRITE: replace slot R0 with V_HNAME and write all 8 slots back: a
; fresh READ_HOST_SLOTS, then WRITE_HOST_SLOTS streamed from the window
; with that slot substituted (NUL-padded to 32). Results as CGO.
; From fujinet-config/arcadia's hosts.inc. Calls four down.
; Clobbers R0-R3, V_T2, V_T3.
HWRITE: STRA,R0 V_T2
        LODI,R3 FCRHSL
        ZBSR    *ZCDO0
        BCFA,EQ HWR9
        LODI,R3 FCWHSL
        LODI,R2 0
        ZBSR    *ZMSTART
        EORZ    R0
HWR1:   STRA,R0 V_T3            ; the slot being streamed
        COMA,R0 V_T2
        BCTR,EQ HWRED
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R1
        LODI,R3 32
HWRC:   LODA,R0 *FRDP,R1
        STRZ    R2
        LODA,R0 *FTXP,R2
        ADDI,R1 1
        BDRR,R3 HWRC
        BCTR,UN HWRN
HWRED:  SETFS   V_HNAME
        LODI,R2 32
        BSTA,UN CTXFIX
HWRN:   LODA,R0 V_T3
        ADDI,R0 1
        COMI,R0 8
        BCTR,LT HWR1
        ZBRR    *ZCGO
HWR9:   RETC,UN
