; ui.inc -- screen scaffolding (block 1).
;
; The list screen: row 0 the title band, rows 1-8 the list, row 9 the
; page rule, rows 10-11 the detail panel (white on black: the selected
; room's full name, or a status message), row 12 the legend. The editor
; and join screens keep CONFIG's frame: row 0 title, row 10 messages.
; Titles, legends and messages are positioned strings (STRAT), drawn
; with PRS/PRINT; key names inside them are accent-coloured by $01
; toggles.

; UTITLE: clear the screen and print the positioned string R0:R1.
; Calls two down. Clobbers R0-R3.
UTITLE: STRZ    R2
        BSTA,UN DCLS
        LODZ    R2
        ZBRR    *ZPRS

; UMSG: the positioned string R0:R1 (a row-10 message) on a cleared
; row 10, then a ~2 s pause that any key cuts short. UFAIL: the same
; with the error code R2 in hex at the row's end and the error buzz.
; Calls three down. Clobbers R0-R3, V_T0, V_ERRC.
UFAIL:  STRA,R2 V_ERRC
        BSTR,UN UMSG0
        DAT     10,14
        LODA,R0 V_ERRC
        ZBSR    *ZDHEX
        ZBSR    *ZSNDERR
        BCTR,UN UMSG1
UMSG:   BSTR,UN UMSG0
UMSG1:  LODI,R3 120
        BCTR,UN UPAUSE
UMSG0:  STRA,R0 V_T0
        DAT     10,0
        LODI,R3 16
        ZBSR    *ZDROWS
        LODA,R0 V_T0
        ZBRR    *ZPRS

; UPAUSE: wait R3 frames or until a fresh key; R0 = the key or KNONE.
; Calls three down. Clobbers R0,R1,R3.
UPAUSE: ZBSR    *ZKEYSCN
        COMI,R0 KNONE
        BCFR,EQ UPS9
        BDRR,R3 UPAUSE
UPS9:   RETC,UN

; PANEL: the detail panel (rows 10-11) blanked to black, then the
; positioned string R0:R1 in it. PANCLR: just the blanking.
; Calls three down. Clobbers R0-R3, V_T0, V_T1.
PANEL:  STRA,R0 V_T0
        STRA,R1 V_T1
        BSTR,UN PANCLR
        DCOL    ASEL
        LODA,R0 V_T0
        LODA,R1 V_T1
        BCTA,UN PRSC
PANCLR: LODI,R0 ASEL
        DAT     10,0
        LODI,R3 32
        ZBRR    *ZDFILL
