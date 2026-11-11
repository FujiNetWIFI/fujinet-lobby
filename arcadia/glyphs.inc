; glyphs.inc -- the user-defined character bitmaps (block 2).
;
; Rows 1-7 of each 8x8 glyph (row 0 is always blank, as in the 2637's own
; font), 5 pixels wide in bits 5-1 to match the built-in letters. Glyphs
; 0-7 are the lobby bank, loaded into $38-$3F by UDCBNK; 8-28 are the
; symbols the charset lacks, which only the editor shows (one at a time,
; in the preview slot $3F). XLAT bit 7 entries index this table.

GLYPHS: DB      $02,$06,$0E,$1E,$0E,$06,$02     ;  0 page back (left)
        DB      $20,$30,$38,$3C,$38,$30,$20     ;  1 page on (right)
        DB      $08,$08,$10,$00,$00,$00,$00     ;  2 '
        DB      $00,$00,$00,$3E,$00,$00,$00     ;  3 -
        DB      $00,$00,$08,$00,$00,$08,$00     ;  4 :
        DB      $08,$08,$08,$08,$08,$00,$08     ;  5 !
        DB      $00,$3E,$22,$22,$22,$3E,$00     ;  6 placeholder box
        DB      $32,$32,$04,$08,$10,$26,$26     ;  7 %
        DB      $04,$08,$10,$10,$10,$08,$04     ;  8 (
        DB      $10,$08,$04,$04,$04,$08,$10     ;  9 )
        DB      $00,$00,$00,$00,$00,$00,$3E     ; 10 _
        DB      $14,$14,$00,$00,$00,$00,$00     ; 11 "
        DB      $14,$14,$3E,$14,$3E,$14,$14     ; 12 #
        DB      $10,$28,$28,$10,$2A,$24,$1A     ; 13 &
        DB      $00,$08,$2A,$1C,$2A,$08,$00     ; 14 *
        DB      $00,$00,$08,$00,$00,$08,$10     ; 15 ;
        DB      $04,$08,$10,$20,$10,$08,$04     ; 16 <
        DB      $00,$00,$3E,$00,$3E,$00,$00     ; 17 =
        DB      $10,$08,$04,$02,$04,$08,$10     ; 18 >
        DB      $1C,$22,$02,$04,$08,$00,$08     ; 19 ?
        DB      $1C,$22,$2E,$2A,$2E,$20,$1E     ; 20 @
        DB      $1C,$10,$10,$10,$10,$10,$1C     ; 21 [
        DB      $1C,$04,$04,$04,$04,$04,$1C     ; 22 ]
        DB      $08,$14,$22,$00,$00,$00,$00     ; 23 ^
        DB      $10,$08,$04,$00,$00,$00,$00     ; 24 `
        DB      $0C,$10,$10,$20,$10,$10,$0C     ; 25 {
        DB      $08,$08,$08,$08,$08,$08,$08     ; 26 |
        DB      $18,$04,$04,$02,$04,$04,$18     ; 27 }
        DB      $00,$00,$10,$2A,$04,$00,$00     ; 28 ~
