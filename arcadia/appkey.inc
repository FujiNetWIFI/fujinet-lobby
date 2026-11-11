; appkey.inc -- the username (creator 1 / app 1 / key 0) and the room
; handoff (key <game type>) (block 2).
;
; The Astrocade lobby's shape: OPEN_APPKEY's payload is six RAW TX
; bytes -- creator lo, creator hi, app, key, mode, and a reserved 0 the
; firmware waits for -- not size-prefixed parameters. A read reply is a
; little-endian u16 length ahead of the data. Appkeys live on the
; FujiNet's SD card.

AKCRLO  EQU     1
AKCRHI  EQU     0
AKAPP   EQU     1
AKKNAME EQU     0               ; the username
AKMRD   EQU     0               ; open modes
AKMWR   EQU     1

; AKOPR / AKOPW: open key R1 to read / write. Results as CGO.
; Calls three down. Clobbers R0-R3.
AKOPW:  LODI,R0 AKMWR
        BCTR,UN AKOP
AKOPR:  LODI,R0 AKMRD
AKOP:   STRA,R0 *PVAKM
        LODI,R3 FCOKEY
        LODI,R2 0
        ZBSR    *ZMSTART
        LODA,R0 FNDATA+AKCRLO
        LODA,R0 FNDATA+AKCRHI
        LODA,R0 FNDATA+AKAPP
        LODA,R0 FNDATA,R1       ; key
        LODA,R2 *PVAKM
        LODA,R0 FNDATA,R2       ; mode
        LODA,R0 FNDATA          ; reserved
        ZBRR    *ZCGO

; AKCLOSE: CLOSE_APPKEY. Results as CGO. Calls two down.
AKCLOSE: LODI,R3 FCCKEY
        ZBRR    *ZCDO0

; AKWRITE: key R1 = the string at (FSTRP), sent without its NUL. Best
; effort: the result is the close's. Calls four down. Clobbers R0-R3.
AKWRITE: BSTR,UN AKOPW
        BCFR,EQ AKCLOSE
        LODI,R3 FCWKEY
        LODI,R2 0
        ZBSR    *ZMSTART
        ZBSR    *ZCTXSTR
        ZBSR    *ZCGO
        BCTR,UN AKCLOSE

; NAMGET: V_NEDT = the stored username, raw, at most 8 characters
; ("" when there is none). The open gets a second try: the first
; command after power-on can meet a cold link. Calls four down.
; Clobbers R0-R3, V_TMP.
NAMGET: EORZ    R0
        STRA,R0 *PVNEDT
        LODI,R0 2
        STRA,R0 *PVTMP
NG1:    LODI,R1 AKKNAME
        BSTA,UN AKOPR
        BCTR,EQ NG2
        LODA,R0 *PVTMP
        SUBI,R0 1
        STRA,R0 *PVTMP
        BCFR,EQ NG1
        RETC,UN
NG2:    LODI,R3 FCRKEY
        ZBSR    *ZCDO0
        BCFR,EQ AKCLOSE
        LODA,R0 FNRDATA+1
        BCFR,EQ AKCLOSE         ; 256 bytes or more: not a name
        LODA,R0 FNRDATA
        COMI,R0 NAMEMAX+1
        BCTR,LT NG3
        LODI,R0 NAMEMAX
NG3:    STRZ    R3
        LODI,R1 0
        COMI,R3 0
        BCTR,EQ NG5
NG4:    LODA,R0 FNRDATA+2,R1
        STRA,R0 *PVNEDT,R1
        ADDI,R1 1
        BDRR,R3 NG4
NG5:    EORZ    R0
        STRA,R0 *PVNEDT,R1
        BCTA,UN AKCLOSE
