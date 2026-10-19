; nament.inc -- the splash and the username (block 1).
;
; The name is the shared FujiNet username, appkey creator 1 / app 1 /
; key 0, which every lobby client and game reads. It must be 2-8
; letters or digits (the SMS port's rule); a stored name that fails is
; treated as none. The first run, and Clear in the list, open CONFIG's
; grid editor on a working copy (V_NEDT); the key is written only when
; a valid name is accepted. Escaping with nothing stored falls back to
; "ARCADIA" for this session, unwritten.

; SPLASH: the title while the stored name is read, then the list (or
; the editor on a first run).
SPLASH: BSTA,UN DCLS
        DAT     3,0
        BSTR,UN SSTRIP
        DAT     9,0
        BSTR,UN SSTRIP
        PRINT   TSPL1
        PRINT   TSPL2
        BSTA,UN NAMGET
        BSTA,UN NAMVAL
        BCTA,EQ LSTLD
NAMNEW: EORZ    R0
        STRA,R0 V_NEDT
        BCTR,UN NAMED1

; SSTRIP: a full-width yellow checker stripe in block graphics from the
; cell after R2 (the switch cell, then 15 mosaic cells). Leaf.
; Clobbers R0,R2,R3.
SSTRIP: LODI,R0 $C0
        STRA,R0 SCREEN,R2,+
        LODI,R3 15
SST1:   LODZ    R3
        ANDI,R0 1
        BCTR,EQ SST2
        LODI,R0 AACC|$15
        BCTR,UN SST3
SST2:   LODI,R0 AACC|$2A
SST3:   STRA,R0 SCREEN,R2,+
        BDRR,R3 SST1
        RETC,UN

; NAMED: edit the current name; then the list page again.
NAMED:  LODI,R3 $FF
NME1:   LODA,R0 V_NAME,R3,+
        STRA,R0 V_NEDT,R3
        BCFR,EQ NME1
NAMED1: LODI,R0 TYNAME>>8
        LODI,R1 TYNAME&$FF
        LODI,R2 V_NEDT&$FF
        LODI,R3 NAMEMAX
        BSTA,UN EDITB
        BCFR,EQ NMESC
        BSTR,UN NAMVAL
        BCTR,EQ NMOK
        ZBSR    *ZSNDERR
        LODI,R0 ENAME>>8
        LODI,R1 ENAME&$FF
        BSTA,UN UMSG
        BCTR,UN NAMED1
NMOK:   SETFS   V_NAME
        LODI,R1 AKKNAME
        BSTA,UN AKWRITE         ; best effort
        ZBRR    *ZLSTLD
NMESC:  LODA,R0 V_NAME
        BCFR,EQ NME9            ; the old name stands
        LODI,R3 $FF
NME2:   LODA,R0 *PDEFN,R3,+
        STRA,R0 V_NAME,R3
        BCFR,EQ NME2
NME9:   ZBRR    *ZLSTLD

; NAMVAL: fold V_NEDT to upper case; when it is 2-8 letters and digits
; it becomes V_NAME and R0 = 0 / CC EQ. Calls one down. Clobbers R0,R3.
NAMVAL: LODI,R3 0
NV1:    LODA,R0 V_NEDT,R3
        BCTR,EQ NV5
        BSTA,UN PFOLD
        STRA,R0 V_NEDT,R3
        COMI,R0 '0'
        BCTR,LT NVBAD
        COMI,R0 '9'+1
        BCTR,LT NV2
        COMI,R0 'A'
        BCTR,LT NVBAD
        COMI,R0 'Z'+1
        BCFR,LT NVBAD
NV2:    ADDI,R3 1
        COMI,R3 NAMEMAX+1
        BCTR,LT NV1
NVBAD:  LODI,R0 1
        RETC,UN
NV5:    COMI,R3 2
        BCTR,LT NVBAD
        LODI,R3 $FF
NV6:    LODA,R0 V_NEDT,R3,+
        STRA,R0 V_NAME,R3
        BCFR,EQ NV6
        RETC,UN

PDEFN:  DWBE    TDEFN           ; strings.inc, block 2
