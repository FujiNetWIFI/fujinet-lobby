; dl.s -- the display list and the font, in ROM where ANTIC reads them
; (CONFIG's atari5200/antic_dl.s).

        .export _a52_dlist, _a52_font

SCREEN = $3C00

        .segment "DLIST"
_a52_dlist:
        .byte   $70, $70, $70                   ; 24 blank lines
        .byte   $42, <SCREEN, >SCREEN           ; mode 2, from the screen
        .repeat 23
        .byte   $02
        .endrepeat
        .byte   $41, <_a52_dlist, >_a52_dlist   ; wait for the next frame

        .segment "FONT"
_a52_font:
        .include "font.inc"
