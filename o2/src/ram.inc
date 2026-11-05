; ram.inc -- where everything lives.
;
; Internal RAM (64 bytes):
;   $00-$07  register bank 0: the main program
;   $08-$17  the stack, 8 levels: the interrupts take up to 2
;   $18-$1F  register bank 1: the interrupts' own (r5: the next band, r4:
;            the staged row's Y, from the vblank to the last row)
;   $20-$3C  below
;   $3D-$3F  the BIOS's (its keyboard and IRQ routines)

ROWBUF  equ 020h                ; ROW_TAB: the next row, staged
FRAMES  equ 039h                ; vblanks, for the main loop's clock
QUIET   equ 03Ah                ; frames whose last band has run
MODE    equ 03Bh                ; MODE_*
SNDCNT  equ 03Ch                ; the sound: frames left (beep.inc)

    if ROWBUF+ROW_TAB > FRAMES
    error "ROWBUF has grown into FRAMES"
    endif

MODE_SETUP equ 002h             ; a new screen's objects (fixed.inc obj_setup)
MODE_PAL   equ 080h             ; a PAL console (the boot measures it)

; External RAM (128 bytes, $00-$7F, MOVX with P1_MAIN):
LINEBUF equ 000h                ; 16: a text line being built (disp.inc)
TXCOL   equ 010h                ; its colour, and for characters with bit 7 set
TXCOL2  equ 011h
OLDPG   equ 012h                ; farcall's caller's page
CURPG   equ 013h                ; the page mapped at $800
TMP0    equ 014h                ; a loop's index across calls that keep nothing
TMP1    equ 015h
TMP2    equ 016h
; the input (input.inc)
KLAST   equ 017h                ; BIOS readkey's memory (its r7)
KEY     equ 018h                ; the key pressed this frame (K_NONE: none)
JOY     equ 019h                ; the stick now: J_* bits
JOYP    equ 01Ah                ; last frame's
JOYEV   equ 01Bh                ; pressed since last frame
REP     equ 01Ch                ; auto-repeat countdown
; the transaction (mail.inc)
NSEQ    equ 01Dh                ; the sequence number posted
LTMP    equ 01Eh                ; the library's own scratch (p1.inc)
HRET    equ 01Fh                ; the help's way back: entry << 4 | page
RICON   equ 020h                ; row_line: sprite 1 (the folder) beside the row
; the screens' shared state
RETPG   equ 021h                ; the editor's and the net page's way back: page,
RETEN   equ 022h                ; ... and entry
HELPCTX equ 023h                ; HC_*: whose help
EDKIND  equ 024h                ; ED_*: what the editor edits
EDMAX   equ 025h                ; its longest
EDLEN   equ 026h                ; EDBUF's length
EDRES   equ 027h                ; 1: accepted, 0: cancelled
EDCUR   equ 028h                ; the editor's cursor
SHIFT   equ 029h                ; the editor's shift state (p6.inc)
HOST    equ 02Ah                ; the join's host slot, 0-7
SEL     equ 02Bh                ; a list's cursor row
FLAGS   equ 02Ch                ; FL_*
; a screen's own (each screen's names for the same bytes)
SCR0    equ 02Dh                ; 16 bytes, $2D-$3C
; the list (p2.inc)
MQOFF   equ SCR0                ; the bar's room name: its first character
MQCLK   equ SCR0+1              ; frames to its next step
MQDIR   equ SCR0+2              ; 1 or -1
LCNT    equ SCR0+3              ; records in the reply (0-LB_ASK)
LMORE   equ SCR0+4              ; non-zero: a next page
LREC    equ SCR0+11             ; the bar's record (0-3), for the join
LCUR    equ SCR0+14             ; where the bar goes: 0 the first room, 0FFh
                                ; the last, else where it was
LTRY    equ SCR0+15             ; GETs left for a reply that came short
; the join (p4.inc): taken from the list's reply before it goes
JT      equ SCR0+5              ; the game's appkey id
JABS    equ SCR0+6              ; the record's place in the whole list
JHL     equ SCR0+7              ; a checksum of JT and its server's URL
JHH     equ SCR0+8
JHLEN   equ SCR0+9              ; EDBUF: the host, then the path
JPLEN   equ SCR0+10
JREC    equ LREC
; the editor (p6.inc)
EBLINK  equ SCR0+3              ; the cursor's blink, frames
EMAGC   equ SCR0+4              ; the character the magnifier shows
EDIRTY  equ SCR0+5              ; lines due (ed_lazy): bit 0 the status, 1-6
EDIRTY2 equ SCR0+6              ; the field's rows, 7 the chart's row 7; and
                                ; bit 0 the chart's T1
; the help (p7.inc)
HPAGE   equ SCR0+3              ; its page
; the boot (p5.inc)
BLAST   equ SCR0+3              ; the percent drawn
BCELLS  equ SCR0+4              ; the bar's cells filled
BSTATE  equ SCR0+5              ; BOOT_STATE this frame
BWAITL  equ SCR0+6              ; frames left without progress (2)

; the lobby's, across the screens
LPAGE   equ 03Dh                ; the list's page: records 4 x LPAGE on
NREQ    equ 03Eh                ; NQ_*: the GET the net page makes
NRES    equ 03Fh                ; ... its result: 0, NR_NONE or the nDevStatus

EDBUF   equ 040h                ; 64: the text being edited (p6.inc); the
EDBUF_LEN equ 64                ; join's URLs
ROWMAP  equ EDBUF+48            ; the list's 8 rows: a record (0-3), 80h| a
                                ; record's game, 0FFh none
NAMEBUF equ EDBUF+56            ; the player's name (FL_NAME), NUL-ended if
                                ; short; the join spends it

FL_NAME  equ 01h                ; NAMEBUF holds key 0's name (maybe none)
FL_ASKED equ 02h                ; the first run has asked for a name
FL_HELPQ equ 04h                ; the help was closed with ? (the editor types it)
