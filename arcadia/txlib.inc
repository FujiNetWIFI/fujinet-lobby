; txlib.inc -- the page-0 half of the transport (block 1): TX payload
; streaming from strings, and the pointer words page-0 code reaches the
; mailbox through. From fujinet-config/arcadia.
;
; FSTRP names its string with INDIRECT loads (LODA,R0 *FSTRP,R3,+), and
; an indirect operand must sit on the same page as the code -- FSTRP is
; page-0 RAM, so everything that dereferences it lives here. The string
; itself can be anywhere: RAM, block-2 ROM, or the reply window (the
; window holds still until the next commit, so streaming a reply back
; into TX is legal).

; ---- pointer words: the page-1 bytes page-0 code touches ---------------
FRDP:   DWBE    FNRDATA
FTXP:   DWBE    FNDATA
FMAGFP: DWBE    FNMAGF

SETFS   MACRO   adr             ; FSTRP = adr (clobbers R0)
        LODI,R0 (adr)>>8
        STRA,R0 FSTRP
        LODI,R0 (adr)&$FF
        STRA,R0 FSTRP+1
        ENDM

; CTXSTR: stream the 0-terminated string at (FSTRP) -- not its NUL --
; and add its length to V_TXN. Leaf. Clobbers R0,R1,R3.
CTXSTR: LODI,R3 $FF
CTS1:   LODA,R0 *FSTRP,R3,+
        BCTR,EQ CTS2
        STRZ    R1
        LODA,R0 *FTXP,R1        ; TX append: the INDEX is the byte
        BCTR,UN CTS1
CTS2:   LODZ    R3
        ADDA,R0 V_TXN
        STRA,R0 V_TXN
        RETC,UN

; CTXFIX: stream exactly R2 (>0) bytes from (FSTRP); past its NUL the
; rest are zeros. Leaf. Clobbers R0-R3.
CTXFIX: LODI,R3 $FF
CTF1:   LODA,R0 *FSTRP,R3,+
        BCTR,EQ CTF2
        STRZ    R1
        LODA,R0 *FTXP,R1
        BDRR,R2 CTF1
        RETC,UN
CTF2:   LODA,R0 *FTXP           ; append 0
        BDRR,R2 CTF2
        RETC,UN

; CPAD: zeros until V_TXN payload bytes reach 256 -- SET_DEVICE_FULLPATH
; fails a short payload on the ESP32. Leaf. Clobbers R0,R3.
CPAD:   LODA,R3 V_TXN
CPD1:   LODA,R0 *FTXP
        BIRR,R3 CPD1
        RETC,UN

; CMOUNT: MOUNT_HOST V_HOST. Results as CGO.
CMOUNT: LODA,R1 V_HOST
        LODI,R3 FCMHOST
        BCTA,UN CDO1

; PFOLD: fold a-z in R0 to A-Z. Leaf.
PFOLD:  COMI,R0 'a'
        BCTR,LT PF9
        COMI,R0 'z'+1
        BCFR,LT PF9
        SUBI,R0 $20
PF9:    RETC,UN
