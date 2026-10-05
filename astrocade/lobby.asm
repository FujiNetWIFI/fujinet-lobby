; lobby.asm -- the FujiNet Game Lobby for the Bally Astrocade.
;
; A standalone-assembly client like the other Astrocade FujiNet apps: the
; shared C client cannot fit this machine, and there is no fujinet-lib
; for it. The cartridge is the FujiNet RP2040 mailbox cart (fujinet-
; firmware pico/astrocade); the framework -- mailbox transport, 4x6 text
; through the magic expander, input, sound -- is 5 Card Stud's
; (fujinet-5cardstud/astrocade), the boot handoff CONFIG's.
;
; ROM budget: 0000H-1AFFH of the 8K window (6,912 bytes); 1B00H+ belongs to
; the mailbox and build.sh stamps the "FUJI" claim at 1CFCH so the mailbox
; stays alive when this image is booted over the network (CONFIG's keypad
; 0 boots it from ec.tnfs.io:/astrocade/lobby.bin). RAM is screen RAM
; alone: 90 visible lines use 4000H-4E0FH and everything above is ours.
;
; Interrupts stay off for the program's whole life (fujilib.inc's contract:
; with I = 0, refresh strays land in OS ROM and never hit the hotspots).

        INCLUDE "HVGLIB.H"
        INCLUDE "fujinet.inc"
        INCLUDE "build/flags.inc"

; ---- RAM map ----------------------------------------------------------
LINES   EQU     90              ; 15 rows of 4x6 text

V_KEY   EQU     4E10H           ; last keypad/handle scan state (edges)
CURSLC  EQU     4E11H           ; reply slice the cart is publishing
AVAIL   EQU     4E12H           ; NET_STATUS bytes waiting, u16
PRVAVL  EQU     4E14H           ; previous reading, for the settle loop
RXLEN   EQU     4E16H           ; reply length, captured after the READ
V_OFF   EQU     4E18H           ; request: first record
V_PSZ   EQU     4E19H           ; request: record count
V_HTTPE EQU     4E1AH           ; nonzero: the last failure was an HTTP
                                ; error status (the empty-page 404)
V_TMP   EQU     4E1BH           ; name-entry loop index, boot percent
V_NCUR  EQU     4E1CH           ; name-entry cursor
V_SEL   EQU     4E1DH           ; list row the bar is on / searches from
V_BAR   EQU     4E1EH           ; list row the bar is drawn on; 0FFH none
V_ROWS  EQU     4E1FH           ; list rows used on this page
V_MORE  EQU     4E20H           ; nonzero: another page follows
V_NXT   EQU     4E21H           ; ...and its first record
V_PG    EQU     4E22H           ; page number = PGSTK index
V_CUR   EQU     4E23H           ; first record of the chunk in the window
V_CNT   EQU     4E24H           ; records in that chunk
V_I     EQU     4E25H           ; record being drawn, within the chunk
V_REC   EQU     4E26H           ; its reply offset, u16
V_GT    EQU     4E28H           ; joining: game_type
V_PATH  EQU     4E29H           ; joining: -> the client URL's path, u16
V_VOL   EQU     4E2BH           ; cue volume (VOLAB)
V_DCNT  EQU     4E2CH           ; DEMO: records in the canned reply
V_DPTR  EQU     4E2DH           ; DEMO: its first record, u16
NUMBUF  EQU     4E2FH           ; TXDEC digits, 4
PREVGM  EQU     4E33H           ; game of the last header drawn, 17
PLNBUF  EQU     4E44H           ; player name, 8 + NUL
ROWREC  EQU     4E4DH           ; per list row: record offset, 0FFH header
PGSTK   EQU     4E58H           ; first record of each page visited, 32
NAMEED  EQU     4E78H           ; name-entry edit buffer, 8
SRVURL  EQU     4E80H           ; joining: server URL, 64 + NUL
HOSTBF  EQU     4EC1H           ; joining: client URL host, 32 + NUL
LINBUF  EQU     4F00H           ; display line being built (page aligned:
                                ; L is the length)
CLURL   EQU     4F30H           ; joining: client URL, 64 + NUL
STACK   EQU     4FC0H           ; grows down to CLURL's end, 4F71H;
                                ; 4FC0H+ left to the BIOS cells and the
                                ; boot stub

OPTFB   EQU     0CH             ; BIOS STRDIS: fg color 3, bg color 0

; MB_* labels are module fences for tools/checksize.py's budget table.
        ORG     FIRSTC
MB_MAIN:
        DB      55H
        DW      MENUST
        DW      PRGNAM
        DW      PRGSTR
PRGNAM: DB      "FUJINET LOBBY"
        DB      0

PRGSTR: DI
        LD      SP,STACK
        SYSTEM  INTPC
        DO      SETOUT
        DB      LINES*2
        DB      0               ; HORCB 0: the whole line is the right palette
        DB      8
        DO      COLSET
        DW      PALET
        DO      FILL
        DW      NORMEM
        DW      LINES*BYTEPL
        DB      0
        DO      STRDIS
        DB      24
        DB      36
        DB      OPTFB
        DW      TSPLSH
        EXIT

        CALL    FNCHECK
        JP      NZ,NOCARD
        XOR     A
        LD      (V_KEY),A

        IF      DEMO
        LD      HL,DEMNAM       ; no appkeys in the demo
        LD      DE,PLNBUF
        LD      BC,6
        LDIR
        ELSE
        CALL    NAMGET          ; the shared name, or ask for one
        JR      NZ,MAIN1
        CALL    NAMESCR
        CALL    NAMPUT
MAIN1:
        ENDIF
        JP      LSTSCR

; ---- Errors -----------------------------------------------------------
NOCARD: SYSSUK  STRDIS
        DB      20
        DB      56
        DB      OPTFB
        DW      ENOCART
HALTE:  JR      HALTE

; ---- Data -------------------------------------------------------------
MB_DATA:
TSPLSH: DB      "FUJINET LOBBY",0
ENOCART: DB     "NO FUJINET CART",0

; COLSET stores descending, ports 7 down to 0; both halves identical since
; HORCB is 0. Byte = (hue << 3) | luminance (MAME astrocde_v.cpp; blue is
; hue 31, 7AH is olive). 3 = white rooms, 2 = yellow game headers,
; 1 = cyan chrome, 0 = deep blue background.
PALET:  DB      07H,7FH,0D5H,0F8H
        DB      07H,7FH,0D5H,0F8H

        INCLUDE "build/endpoint.inc"

MB_LIST:
        INCLUDE "list.inc"
MB_BOOT:
        INCLUDE "boot.inc"
MB_NAMENT:
        INCLUDE "nament.inc"
MB_APPKEY:
        INCLUDE "appkey.inc"
MB_SOUND:
        INCLUDE "sound.inc"
MB_INPUT:
        INCLUDE "input.inc"
MB_NET:
        IF      DEMO
        INCLUDE "demo.inc"
        ELSE
        INCLUDE "net.inc"
        ENDIF
        IF      DEMO
        ELSE
MB_URL:
        INCLUDE "url.inc"
        ENDIF
MB_STATE:
        INCLUDE "state.inc"
MB_GFX:
        INCLUDE "gfx.inc"
MB_FONT:
        INCLUDE "assets/font.inc"
MB_FUJILIB:
        INCLUDE "fujilib.inc"
MB_END:
