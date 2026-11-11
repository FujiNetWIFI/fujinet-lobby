; disp.inc -- 2637 UVI display layer for the lobby (block 1).
;
; A snapshot of fujinet-config/arcadia's, with the lobby's UDC bank.
;
; One mode for every screen: 13-line doublescan (16 x 13 cells of 8x16)
; in MULTICOLOR, because 13-line mode is what leaves $1A00-$1AFF free as
; RAM. In multicolor, cell bit 6 picks the ink and bit 7 the background
; from two register pairs:
;
;   $19F8 = $07  13-line, ink A white(0), bg A BLACK(7)
;   $19F9 = $0E  8x16 cells, pot mux clear, ink B yellow(1), bg B BLUE(6)
;
;   attr  looks like          used for
;   $80   white  on blue      rooms, the page (MAME paints the border in
;                             bg B, so the page must be bg B)
;   $C0   yellow on blue      game headers, their rules and brackets
;   $00   white  on black     the selected room, the title band, the
;                             detail panel
;   $40   yellow on black     accents inside those
;
; Two exact bytes are traps. $C0 (a space with attrs 11) flips the rest
; of its row into block graphics, so DPUTC and RHILIT rewrite it to $80
; (the same blank blue cell). $40 ends a block-graphics run; in a text
; row it is a harmless black space. The game headers and bar.inc own
; the mosaic runs.
;
; $19F9 bit 6 is also the pot mux: input.inc rewrites the register every
; frame from BGCB, so nothing else may change the palette after DINI.
;
; Writes are direct indexed stores, STRA,R0 SCREEN,R2,+ with R2 = the
; cell before the target (the DAT macro), so this layer is page-0 only.

; ---- hardware ---------------------------------------------------------
SCREEN  EQU     $1800           ; 16 cols x 13 rows
UDC     EQU     $1980           ; 8 user-defined characters, 8 bytes each
GFXMOD  EQU     $19F8           ; b7 gfx, b6 26-line, b5-3 ink A, b2-0 bg A
BGCOL   EQU     $19F9           ; b7 8x8 cells, b6 pot mux, ink B, bg B
SPRPOS  EQU     $18F0           ; 4 x (y,x) sprite positions
VPOS    EQU     $18FC           ; vertical screen position
PITCH   EQU     $18FD           ; b7 MULTICOLOR, b0-6 tone
VOLUME  EQU     $18FE           ; b0-2 volume, b3 tone, b4 noise, b5-7 shift

GFXM    EQU     $07             ; colours are inverted: 0 white .. 7 black
BGCB    EQU     $0E

ANORM   EQU     $80
AACC    EQU     $C0
ASEL    EQU     $00
ASELA   EQU     $40

; ---- glyphs -----------------------------------------------------------
GSPACE  EQU     $00
GDIAG   EQU     $01             ; built-in '/'
GBSLSH  EQU     $02             ; built-in '\'
GFTOP   EQU     $04             ; built-in box edges and corners
GFLFT   EQU     $07
GFBL    EQU     $0A
GDOT    EQU     $34
; the lobby UDC bank (glyphs.inc order; the editor reuses slot $3F)
GLARR   EQU     $38             ; page arrows
GRARR   EQU     $39
GAPOS   EQU     $3A             ; '
GDASH   EQU     $3B             ; -
GCOLON  EQU     $3C             ; :
GBANG   EQU     $3D             ; !
GPLACE  EQU     $3E             ; any symbol the charset lacks
GPCT    EQU     $3F             ; '%' -- the editor's live preview slot
NGLYPH  EQU     7               ; bitmap rows stored per glyph (rows 1-7)

; ---- mode setup -------------------------------------------------------
; DINI: palette, multicolor, silence, screen position, sprites parked
; off screen (their images are UDCs $38-$3B), the UDC bank, clear.
; Calls two down. Clobbers R0-R3.
DINI:   LODI,R0 GFXM
        STRA,R0 GFXMOD
        LODI,R0 BGCB
        STRA,R0 BGCOL
        LODI,R0 $80             ; multicolor on, tone 0 = silent
        STRA,R0 PITCH
        LODI,R0 $FF             ; screen from the top line (MAME's default)
        STRA,R0 VPOS
        EORZ    R0
        STRA,R0 VOLUME
        LODI,R3 8
DIN1:   STRA,R0 SPRPOS-1,R3     ; y = 0 means line 256: off screen
        BDRR,R3 DIN1
        BSTR,UN UDCBNK
        BCTR,UN DCLS

; UDCBNK: load the bank (glyphs 0-7 into $38-$3F). Calls one down.
; Clobbers R0-R3.
UDCBNK: LODI,R1 7
UDCB1:  LODZ    R1
        STRZ    R2
        BSTR,UN UDCLD
        BDRR,R1 UDCB1
        LODI,R2 0
        BCTR,UN UDCLD

; UDCLD: UDC slot R1 (0-7) := glyph R2 (glyphs.inc index). Row 0 of
; every glyph is blank, as in the 2637's own font. Leaf. Clobbers R0,R2,
; R3, V_T1.
UDCLD:  LODZ    R2              ; source = glyph * 7, less one for ,+
        RRL,R0
        RRL,R0
        RRL,R0
        SUBZ    R2
        SUBI,R0 1
        STRZ    R2
        LODZ    R1              ; destination = slot * 8
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        EORZ    R0
        STRA,R0 UDC,R3          ; row 0
        LODI,R0 NGLYPH
        STRA,R0 V_T1
UDCL1:  LODA,R0 *PGLYPH,R2,+
        STRA,R0 UDC,R3,+
        LODA,R0 V_T1
        SUBI,R0 1
        STRA,R0 V_T1
        BCFR,EQ UDCL1
        RETC,UN

; DCLS: the whole screen to blank page cells. Leaf. Clobbers R0,R3.
DCLS:   LODI,R3 208
        LODI,R0 ANORM
DCLS1:  STRA,R0 SCREEN-1,R3
        BDRR,R3 DCLS1
        RETC,UN

; DROWS: blank R3 cells from the cell after R2 (the DAT convention).
; DFILL: the same with the cell value R0. Leaf. Clobbers R0,R2,R3.
DROWS:  LODI,R0 ANORM
DFILL:  STRA,R0 SCREEN,R2,+
        BDRR,R3 DFILL
        RETC,UN

; ---- writing ----------------------------------------------------------
; DPUTC: glyph R0 at the next cell with the DCOLOR attribute. The $C0
; guard lives here. Leaf. Clobbers R0, R2 (+1).
DPUTC:  IORA,R0 DCOLOR
        COMI,R0 $C0
        BCFR,EQ DPUT1
        LODI,R0 ANORM
DPUT1:  STRA,R0 SCREEN,R2,+
        RETC,UN

; DXLAT: ASCII R0 -> glyph R0. Lowercase folds to uppercase; symbols
; the font lacks become GPLACE. Leaf. Clobbers R0,R1.
DXLAT:  ANDI,R0 $7F
        SUBI,R0 $20
        BCTR,LT DXLBAD          ; control character
        STRZ    R1
        LODA,R0 *PXLAT,R1
        BCTR,LT DXLBAD          ; bit 7: a symbol the font lacks
        RETC,UN
DXLBAD: LODI,R0 GPLACE
        RETC,UN

; PRS: print the positioned string at R0:R1 (hi:lo; first byte is the
; cell before the start) in the normal colour. PRSC: the same in the
; DCOLOR already set. Calls two down. Clobbers R0-R3.
PRS:    LODI,R2 ANORM
        STRA,R2 DCOLOR
PRSC:   STRA,R0 DSTRP
        STRA,R1 DSTRP+1
        LODA,R0 *DSTRP
        STRZ    R2
        LODI,R3 0
        BCTR,UN DPRT1

; DPRINT: print the 0-terminated ASCII string at (DSTRP) from the next
; cell. A $01 byte toggles the accent ink. Calls one down.
; Clobbers R0-R3.
DPRINT: LODI,R3 $FF
DPRT1:  LODA,R0 *DSTRP,R3,+    ; str[++R3]; a load sets CC
        BCTR,EQ DPRT9
        COMI,R0 $20
        BCTR,LT DPRTC
        BSTR,UN DXLAT
        BSTR,UN DPUTC
        BCTR,UN DPRT1
DPRTC:  LODA,R0 DCOLOR
        EORI,R0 $40
        STRA,R0 DCOLOR
        BCTR,UN DPRT1
DPRT9:  RETC,UN

; DHEX: R0 as two hex digits at the next cells ($10+n runs 0-9 then
; A-F in this charset). Calls one down. Clobbers R0,R1,R2.
DHEX:   STRZ    R1
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        BSTR,UN DHEX1
        LODZ    R1
DHEX1:  ANDI,R0 $0F
        IORI,R0 $10
        ZBRR    *ZDPUTC

; DDEC: R0 (0-255) in decimal, no leading zeros, from the next cell.
; Calls one down. Clobbers R0-R3, V_FLD, V_T0.
DDEC:   STRZ    R1              ; R1 = what is left to print
        LODI,R3 0               ; nonzero once a digit is out
        LODI,R0 100
        BSTR,UN DDIG
        LODI,R0 10
        BSTR,UN DDIG
        LODZ    R1
        BCTR,UN DHEX1           ; the ones digit always prints
DDIG:   STRA,R0 V_FLD           ; the power of ten
        EORZ    R0
        STRA,R0 V_T0            ; the digit
DDG1:   LODZ    R1
        COMA,R0 V_FLD
        BCTR,LT DDG2
        SUBA,R0 V_FLD
        STRZ    R1
        LODA,R0 V_T0
        ADDI,R0 1
        STRA,R0 V_T0
        BCTR,UN DDG1
DDG2:   LODA,R0 V_T0
        BCFR,EQ DDG3            ; a nonzero digit prints
        COMI,R3 0
        BCFR,EQ DDG3            ; so does a zero after one
        RETC,UN
DDG3:   LODI,R3 1
        BCTR,UN DHEX1

; NDIG: R0 = how many digits DDEC prints for R0. Leaf.
NDIG:   COMI,R0 10
        BCTR,LT NDG1
        COMI,R0 100
        BCTR,LT NDG2
        LODI,R0 3
        RETC,UN
NDG2:   LODI,R0 2
        RETC,UN
NDG1:   LODI,R0 1
        RETC,UN

; RHILIT: toggle the selection bar on screen row R1, columns 1-15 (the
; bracket in column 0 stays put). Bit 7 is the background. Leaf.
; Clobbers R0,R2,R3.
RHILIT: LODZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R2
        LODI,R3 15
RHL1:   LODA,R0 SCREEN+1,R2
        EORI,R0 $80
        COMI,R0 $C0
        BCFR,EQ RHL2
        LODI,R0 ANORM
RHL2:   STRA,R0 SCREEN+1,R2
        ADDI,R2 1
        BDRR,R3 RHL1
        RETC,UN

; ROWCEL: R2 = the cell before list row R0's first cell (list row 0 is
; screen row 1). Leaf.
ROWCEL: ADDI,R0 1
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        SUBI,R0 1
        STRZ    R2
        RETC,UN

PXLAT:  DWBE    XLAT            ; the tables live in block 2; indirect
PGLYPH: DWBE    GLYPHS          ;   operands reach them, direct ones do not
