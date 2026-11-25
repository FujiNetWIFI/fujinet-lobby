; maria.s -- the 7800 lobby's 40 x 24 text screen in MARIA 320D; see
; include/atari7800/a78.h. Adapted from fujinet-config atari7800/maria.s.
;
; Nothing here stores to $00-$1F: INPTCTRL is left unlocked.

        .include "zeropage.inc"

        .export _a78_init, _a78_flush, _a78_vsync, _a78_background
        .export _a78_map, _a78_dirty, _a78_rowlo, _a78_rowhi, _a78_pal
        .import _a78_font, _fuji_a7800_nmi

BACKGRND = $20
P0C1    = $21
P0C2    = $22
P0C3    = $23
MWSYNC  = $24
MSTAT   = $28                   ; bit 7: vertical blank
DPPH    = $2C
DPPL    = $30
P4C1    = $31
P4C2    = $32
P4C3    = $33
CHARBASE = $34
OFFSET  = $38
CTRL    = $3C

COLS    = 40
ROWS    = 24
MAXRUNS = 12                    ; 16 + 12 * 10 + 40 * 6 = 376 MARIA clocks a line
WIDTHMAX = 32                   ; the most cells one header can draw
SLOT    = 64                    ; bytes per display list slot
NSLOTS  = 3                     ; slots per row, in the row's own page
HDR     = $60                   ; extended header, indirect, write mode 0: 320D
DMA_ON  = $46                   ; DMA on, 1-byte characters, kangaroo, 320B/D
DMA_OFF = $7F
PAL_LINES = 270                 ; NTSC MARIA draws 242 lines, PAL 292
TOP     = 4                     ; blank DLL entries above the rows
BOTTOM  = 4
DLI     = $80                   ; a DLL entry's display-list interrupt bit
BANDS   = 4
ATTR    = $400                  ; the palette values are 4 pages after the map

        .segment "VIDEO"        ; page aligned, in the cart's RAM
dls:    .res    ROWS * $100     ; a page per row: NSLOTS lists of SLOT bytes
_a78_map:                       ; 6 rows of 40 characters a page, so no
        .res    $400            ; header's run of map bytes crosses a page
attr:   .res    $400            ; the palette value (0-7) of every cell
dll:    .res    (TOP + ROWS + BOTTOM) * 3
nulldl: .res    2
_a78_dirty:
        .res    ROWS            ; the row's list must be rebuilt
slot:   .res    ROWS            ; the slot MARIA reads, 0 to NSLOTS - 1
bandcol: .res   BANDS * 6       ; P0C1-P0C3, P4C1-P4C3 of each band

        .segment "BSS"
_a78_pal:  .res 1
dli_idx: .res   1
brow:   .res    1
rpal:   .res    1
rstart: .res    1
rend:   .res    1
rmax:   .res    1
ink:    .res    1
runs:   .res    1
oidx:   .res    1
syncrow: .res   1

        .code

; void a78_init (void)
; Measure the TV standard, set up the colours, the DLL and a blank screen,
; hook the display-list interrupt and turn the display on at the next
; vertical blank.
.proc _a78_init
        lda     #DMA_OFF
        sta     CTRL
        jsr     measure

        ldx     #BANDS * 6 - 1  ; the band colours; PAL's hues are one on
@col:   lda     bandrom,x
        ldy     _a78_pal
        beq     @ntsc
        cmp     #$10            ; greys stay
        bcc     @ntsc
        clc
        adc     #$10
@ntsc:  sta     bandcol,x
        dex
        bpl     @col

        lda     #0
        sta     nulldl
        sta     nulldl+1
        sta     dli_idx
        sta     BACKGRND

        ldx     #0              ; the blank lines above, NTSC or PAL
        ldy     _a78_pal
        beq     @top
        ldy     #TOP
@top:   lda     toplines,y
        sta     dll,x
        lda     #>nulldl
        sta     dll+1,x
        lda     #<nulldl
        sta     dll+2,x
        inx
        inx
        inx
        iny
        cpx     #TOP * 3
        bne     @top

        ldy     #0              ; the rows, each 8 lines
@row:   lda     rowdll,y
        sta     dll,x
        tya
        clc
        adc     #>dls
        sta     dll+1,x
        lda     #0
        sta     dll+2,x
        sta     slot,y
        inx
        inx
        inx
        iny
        cpy     #ROWS
        bne     @row

        lda     #15 | DLI       ; enough below to reach the blank; the
@bot:   sta     dll,x           ; interrupt after row 23 resyncs the bands
        lda     #>nulldl
        sta     dll+1,x
        lda     #<nulldl
        sta     dll+2,x
        lda     #15
        inx
        inx
        inx
        cpx     #(TOP + ROWS + BOTTOM) * 3
        bne     @bot

        ldx     #0              ; every cell a space, palette 0
@clr:   lda     #' '
        sta     _a78_map,x
        sta     _a78_map+$100,x
        sta     _a78_map+$200,x
        sta     _a78_map+$300,x
        lda     #0
        sta     attr,x
        sta     attr+$100,x
        sta     attr+$200,x
        sta     attr+$300,x
        inx
        bne     @clr
        lda     #1
        ldx     #ROWS - 1
@d:     sta     _a78_dirty,x
        dex
        bpl     @d
        jsr     _a78_flush

        lda     #<dli           ; the crt0's NMI vector jumps through here
        sta     _fuji_a7800_nmi+1
        lda     #>dli
        sta     _fuji_a7800_nmi+2

        lda     #>_a78_font
        sta     CHARBASE
        lda     #0
        sta     OFFSET
        lda     #>dll
        sta     DPPH
        lda     #<dll
        sta     DPPL
@w:     bit     MSTAT           ; DMA may only start in a vertical blank
        bpl     @w
        lda     #DMA_ON
        sta     CTRL
        rts
.endproc

; Count MARIA's lines from the end of one vertical blank to the next.
.proc measure
@in:    bit     MSTAT
        bpl     @in
@out:   bit     MSTAT
        bmi     @out
        ldx     #0
        ldy     #0
@line:  sta     MWSYNC
        inx
        bne     @same
        iny
@same:  bit     MSTAT
        bpl     @line
        lda     #0
        cpy     #>PAL_LINES
        bcc     @set
        bne     @pal
        cpx     #<PAL_LINES
        bcc     @set
@pal:   lda     #1
@set:   sta     _a78_pal
        rts
.endproc

; void __fastcall__ a78_background (unsigned char c)
.proc _a78_background
        sta     BACKGRND
        rts
.endproc

; void a78_vsync (void)
; Wait for the start of the next vertical blank, then rebuild what changed.
.proc _a78_vsync
@in:    bit     MSTAT           ; let a blank already under way end
        bmi     @in
@out:   bit     MSTAT
        bpl     @out
        ; fall through
.endproc

; void a78_flush (void)
; Rebuild every row marked dirty.
.proc _a78_flush
        ldx     #0
@r:     lda     _a78_dirty,x
        beq     @next
        lda     #0
        sta     _a78_dirty,x
        stx     syncrow
        jsr     build
        ldx     syncrow
@next:  inx
        cpx     #ROWS
        bne     @r
        rts
.endproc

; Row X's list into its next slot, then publish it with one store to the
; row's DLL entry (the slots share a page, so only the low byte changes).
; MARIA latches a zone's list when the zone starts; with three slots the one
; being written is never the one a zone on screen still reads, since a
; rebuild takes longer than a zone.
;
; One header per run of cells sharing a palette value, at most WIDTHMAX
; cells; a run of spaces in a plain style (P1 P0 = 0) needs none, as the
; background shows there anyway. The last of MAXRUNS takes the rest.
.proc build
        stx     brow
        lda     _a78_rowlo,x
        sta     ptr1            ; palette values
        sta     ptr3            ; map
        sta     tmp1
        lda     _a78_rowhi,x
        sta     ptr3+1
        sta     tmp2
        clc
        adc     #>ATTR
        sta     ptr1+1
        ldy     slot,x          ; the next slot
        iny
        cpy     #NSLOTS
        bcc     @s
        ldy     #0
@s:     tya
        sta     slot,x
        lda     slotlo,y
        sta     ptr2
        txa
        clc
        adc     #>dls
        sta     ptr2+1

        ldy     #0
        sty     runs
        sty     oidx
@run:   cpy     #COLS
        bcs     @end
        lda     runs
        cmp     #MAXRUNS
        bcc     @more
@end:   jmp     @term
@more:  sty     rstart
        lda     (ptr1),y
        sta     rpal
        lda     (ptr3),y
        eor     #' '
        sta     ink             ; non-zero: not a space
        tya
        clc
        adc     #WIDTHMAX
        sta     rmax
        lda     runs
        cmp     #MAXRUNS - 1
        beq     @last
@scan:  iny
        cpy     #COLS
        bcs     @emit
        cpy     rmax
        bcs     @emit
        lda     (ptr1),y
        cmp     rpal
        bne     @emit
        lda     (ptr3),y
        eor     #' '
        ora     ink
        sta     ink
        jmp     @scan
@last:  lda     rmax            ; the rest of the row, up to WIDTHMAX
        cmp     #COLS
        bcc     @l
        lda     #COLS
@l:     tay
        lda     #1
        sta     ink

@emit:  sty     rend
        lda     ink
        bne     @put
        lda     rpal
        and     #3
        beq     @skip
@put:   ldy     oidx
        lda     tmp1
        clc
        adc     rstart
        sta     (ptr2),y        ; +0 map, low
        iny
        lda     #HDR
        sta     (ptr2),y        ; +1
        iny
        lda     tmp2
        sta     (ptr2),y        ; +2 map, high
        iny
        lda     rpal
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     tmp3
        lda     rstart
        sec
        sbc     rend            ; -width; the field is 32 - width
        and     #$1F
        ora     tmp3
        sta     (ptr2),y        ; +3 palette, width
        iny
        lda     rstart
        asl     a
        asl     a
        sta     (ptr2),y        ; +4 hpos: 4 per character
        iny
        sty     oidx
        inc     runs
@skip:  ldy     rend
        jmp     @run

@term:  ldy     oidx
        lda     #0
        sta     (ptr2),y
        iny
        sta     (ptr2),y
        ldx     brow
        ldy     dllpos,x
        lda     ptr2
        sta     dll,y
        rts
.endproc

; The display-list interrupt. A DLL entry's DLI bit interrupts at the end
; of the zone above it, so the bits on rows 0, 2, 4 and 21 load those rows'
; band colours into palettes 0 and 4, which are all 320D shows, while the
; blank lines above, row 1, row 3 or row 20 are drawn: blank, or a dashed
; rule, in their last lines, so the change can't show early. The bit on the
; first blank entry below starts the count again.
.proc dli
        pha
        txa
        pha
        ldx     dli_idx
        cpx     #BANDS * 6
        bcs     @sync
        lda     bandcol,x
        sta     P0C1
        lda     bandcol+1,x
        sta     P0C2
        lda     bandcol+2,x
        sta     P0C3
        lda     bandcol+3,x
        sta     P4C1
        lda     bandcol+4,x
        sta     P4C2
        lda     bandcol+5,x
        sta     P4C3
        txa
        clc
        adc     #6
        sta     dli_idx
        pla
        tax
        pla
        rti
@sync:  lda     #0
        sta     dli_idx
        pla
        tax
        pla
        rti
.endproc

        .rodata
; The bands' colours (NTSC: hue << 4 | luminance); see conio.c for the
; roles drawn in them. Per band: P0C1 P0C2 P0C3 P4C1 P4C2 P4C3, where a
; plain style shows C2 on the background and a bar style C3 on C1.
bandrom:
        .byte   $64, $0F, $0F, $64, $1E, $1E    ; rows 0-1: purple title bar,
                                                ; white title, gold name
        .byte   $54, $AC, $0F, $54, $C8, $1E    ; rows 2-3: cyan headings,
                                                ; green rule
        .byte   $84, $0E, $0F, $84, $CA, $DE    ; rows 4-20: white names,
                                                ; green counts, blue bar
        .byte   $1C, $0A, $00, $42, $1E, $0F    ; rows 21-23: grey hints, gold
                                                ; keys, yellow status, red errors
toplines:
        .byte   15, 6, 0, 0     ; NTSC: 25 lines
        .byte   15, 15, 15, 1   ; PAL: 50
rowdll:                         ; offset 7; a band starts at rows 0, 2, 4, 21
        .repeat ROWS, r
        .if r = 0 || r = 2 || r = 4 || r = 21
        .byte   7 | DLI
        .else
        .byte   7
        .endif
        .endrepeat
_a78_rowlo:
        .repeat ROWS, r
        .byte   <(_a78_map + (r / 6) * $100 + (r .mod 6) * COLS)
        .endrepeat
_a78_rowhi:
        .repeat ROWS, r
        .byte   >(_a78_map + (r / 6) * $100 + (r .mod 6) * COLS)
        .endrepeat
slotlo:
        .byte   0, SLOT, 2 * SLOT
dllpos:
        .repeat ROWS, r
        .byte   (TOP + r) * 3 + 2
        .endrepeat
