; bar.inc -- the join screen's progress bar (block 1), from
; fujinet-config/arcadia's block-graphics widgets.
;
; The 2637 switches the rest of a row to block graphics at a cell of
; EXACTLY $C0 (the switch cell draws blank), and each row starts over in
; text mode. A mosaic cell's low 6 bits light a 3x2 grid -- top half
; bit2 left 3 px, bit1 middle 3 px, bit0 right 2 px; bottom half bits
; 5/4/3 the same columns -- and its attribute bits still pick ink and
; background (MAME arcadia_v.cpp). Rules kept here and in list.inc:
;   - a mosaic run always goes to the end of its row: an exact $40 would
;     switch back to text and draw a stray bg-A cell;
;   - no data cell is ever an exact $C0 or $40.
;
;   bar     $00 empty (black trough), $64 1/3, $76 2/3, $7F full (yellow)

BARROW  EQU     6               ; the progress bar's screen row
BARCEL  EQU     BARROW*16

; BARSTP: the progress bar at R0 steps (0-45: 15 cells x 3) on BARROW.
; Leaf. Clobbers R0-R3, V_T0.
BARSTP: STRA,R0 V_T0
        LODI,R0 $C0
        STRA,R0 SCREEN+BARCEL
        LODI,R2 BARCEL
        LODI,R3 15
BS1:    LODA,R0 V_T0
        COMI,R0 3
        BCTR,LT BS2
        SUBI,R0 3
        STRA,R0 V_T0
        LODI,R1 3
        BCTR,UN BS3
BS2:    STRZ    R1              ; the partial cell, then nothing left
        EORZ    R0
        STRA,R0 V_T0
BS3:    LODA,R0 BARCLS,R1
        STRA,R0 SCREEN,R2,+
        BDRR,R3 BS1
        RETC,UN
BARCLS: DB      $00,$64,$76,$7F

; BARPCT: the bar at R0 percent (0-100) plus "NN%" on the row below.
; steps = pct/2 - pct/20 = pct*45/100 within half a step.
; Calls two down. Clobbers R0-R3, V_T0, V_T2, V_FLD.
BARPCT: STRA,R0 V_T2
        ANDI,R0 $FE
        RRR,R0
        STRZ    R1              ; pct/2
        LODA,R0 V_T2
BP1:    SUBI,R0 20
        BCTR,LT BP2
        SUBI,R1 1
        BCTR,UN BP1
BP2:    LODZ    R1
        BSTA,UN BARSTP
        DAT     BARROW+2,5
        LODI,R3 5
        ZBSR    *ZDROWS
        DAT     BARROW+2,6
        DCOL    ANORM
        LODA,R0 V_T2
        ZBSR    *ZDDEC
        LODI,R0 GPCT
        ZBRR    *ZDPUTC
