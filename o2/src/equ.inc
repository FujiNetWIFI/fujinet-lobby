; equ.inc -- constants: the console, the cartridge, the screen, FujiNet,
; the lobby.

; ---- P1 (all active low but COPY and LUM) ----
; Main code runs with external RAM and the cartridge on the bus; the interrupts
; switch to the VDC and put P1 back. COPY low always, or the cart's write
; strobe is gated off. The bank bits are left high: APPBANK pages are picked
; through the mailbox, not by P10/P11.
P1_MAIN equ 0AFh                ; XRAM + cart selected, VDC parked
P1_VDC  equ 0B7h                ; VDC selected
P1_JOY  equ 0BBh                ; keyboard scanner (joystick) selected

; ---- the mailbox (fuji_mailbox.h, protocol 2) ----
FN_W_REGSEL     equ 0E0h
FN_W_REGDATA    equ 0E1h
FN_W_DATA       equ 0E2h
FN_W_SEQ        equ 0E3h
FN_REG_DEVICE   equ 000h
FN_REG_CMD      equ 001h
FN_REG_NPARAM   equ 002h
FN_REG_DATA_RST equ 005h
FN_REG_CODEBANK equ 008h
FN_REG_POKE_LO  equ 00Ah
FN_REG_POKE_HI  equ 00Bh
FN_REG_POKE     equ 00Ch
FN_REG_RXWIN    equ 00Dh
FN_R_ACKSEQ     equ 0F20h
FN_R_STATUS     equ 0F21h
FN_R_ERR        equ 0F22h
FN_R_REPLY_CMD  equ 0F23h
FN_R_RXLEN_LO   equ 0F24h
FN_R_RXLEN_HI   equ 0F25h
FN_R_BOOT_STATE equ 0F26h
FN_R_BOOT_PCT   equ 0F27h
FN_R_BOOT_ERR   equ 0F28h
FN_R_MAGIC0     equ 0F29h
FN_R_MAGIC1     equ 0F2Ah
FN_R_PROTO_VER  equ 0F2Bh
FN_R_DATA       equ 0F30h       ; the read window: 208 bytes from RXWIN*16
FN_PROTO_VER    equ 2
O2_APP_HDR      equ 0F30h
O2_APP_VERSION  equ 1

FN_BOOT_IDLE    equ 0           ; BOOT_STATE
FN_BOOT_XFER    equ 1
FN_BOOT_READY   equ 2
FN_BOOT_FAILED  equ 80h

; ---- the Fuji device (fujiCmd.h) ----
FUJI_DEVICEID   equ 070h
FC_ACK          equ 006h
FC_NAK          equ 015h
FC_AKOPEN       equ 0DCh        ; OPEN_APPKEY (6: creator, app, key, mode, 0)
FC_AKREAD       equ 0DDh        ; READ_APPKEY: u16 length, then the key
FC_AKWRITE      equ 0DEh        ; WRITE_APPKEY (the key's bytes)
FC_HPREFIXSET   equ 0E1h        ; SET_HOST_PREFIX (host; 256-byte path)
FC_DEVPATH      equ 0E2h        ; SET_DEVICE_FULLPATH (slot, host, mode; 256)
FC_WRHOSTS      equ 0F3h        ; WRITE_HOST_SLOTS (256)
FC_RDHOSTS      equ 0F4h        ; READ_HOST_SLOTS
FC_MOUNTIMG     equ 0F8h        ; MOUNT_IMAGE (slot, mode)
FC_MOUNTHOST    equ 0F9h        ; MOUNT_HOST (host)

; ---- the network device: N:, an HTTP GET ----
NET_DEVICEID    equ 071h
NC_OPEN         equ 04Fh        ; OPEN (mode, translation; "N:url",0)
NC_CLOSE        equ 043h
NC_READ         equ 052h        ; READ (u16 length)
NC_STATUS       equ 053h        ; STATUS: u16 waiting, connected, nDevStatus
NM_GET          equ 00Ch
NS_OK           equ 1           ; nDevStatus
NS_EOF          equ 136
NS_404          equ 170         ; the lobby's "no rooms" (an HTTP 404)

; ---- the lobby (server/model.go): a reply is [count][0][0] + records ----
LB_HDR    equ 3
LB_REC    equ 189               ; bin=1
LB_PAGE   equ 4                 ; records a page shows; one more is asked for
LB_ASK    equ LB_PAGE+1
LR_TYPE   equ 0                 ; a record: the game's appkey id (0: none)
LR_GAME   equ 1                 ; ... the game (16 + NUL)
LR_ROOM   equ 18                ; ... the room (32 + NUL)
LR_SURL   equ 51                ; ... its server's URL (64 + NUL)
LR_CURL   equ 116               ; ... the client's URL (64 + NUL)
LR_ONLINE equ 184
LR_CUR    equ 185
LR_MAX    equ 186
ROOMW     equ 8                 ; a room's name on its row (then " c/m")
SURLMAX   equ 64
CURLMAX   equ 57                ; host/path after "tnfs://"
HOSTMAX   equ 32
AK_NAME   equ 0                 ; key 0: the player's name
NAMEMAX   equ 8

; ---- the VDC's character generator (o2doc appendix C) ----
G_SPACE equ 12
G_DOT   equ 39
G_DASH  equ 40
G_BLOCK equ 47
CH_TIMES equ 09h                ; codes $00-$1F in text: glyph code+32
CH_DIV   equ 0Ah                ; (mkcharmap.py)
CH_BLOCK equ 0Fh
CH_ARROW equ 16h

; Character colours, as MAME's i8244 draws them (control byte bits 1-3).
; A character can't be black: 0 is the darkest, a dark grey.
C_DGREY  equ 0
C_RED    equ 1
C_GREEN  equ 2
C_TAN    equ 3
C_BLUE   equ 4
C_VIOLET equ 5
C_CYAN   equ 6
C_WHITE  equ 7

; The colour register ($A3): bits 5-3 the background (MAME bitswap 3,4,5:
; bit 5 red, 4 green, 3 blue), bits 6 and 2-0 the grid (bitswap 6,0,1,2:
; bit 6 bright, 2 red, 1 green, 0 blue).
BG_BLACK equ 000h
BG_BLUE  equ 008h
GRID_BLUE  equ 001h             ; the selection bar: dark blue
GRID_LBLUE equ 041h
GRID_LGREEN equ 042h            ; the boot's progress bar

; The control register ($A0). The selection bar is the grid in fill mode: a
; vertical segment is a whole 16 x 24 cell.
CTL_TOP equ vdc_ctrl_fore                               ; T0: the grid hidden
CTL_ALL equ vdc_ctrl_fill|vdc_ctrl_fore|vdc_ctrl_grid   ; the rows
CTL_GON equ vdc_ctrl_fill|vdc_ctrl_grid                 ; a band: grid on
CTL_OFF equ 0                                           ; a band: all off

; ---- the screen ----
; Characters are 7 rows of 2 lines. Every screen is T0 at the top and T1 at
; the bottom (the quads, 16 characters) and eight rows of twelve singles
; between them (fixed.inc). The grid's rows are 24 lines from line 24, its
; columns 16 units from X 8 (a character's X register; it draws 5 units
; right); row r's characters sit in grid row r.
X_TEXT  equ 16                  ; 16 characters at 8 px, centred
TXT_LEN equ 16
TXT_TAB equ 1+2*TXT_LEN         ; a text table: Y, then 16 x (pointer, control)
    ifndef Y_T0
Y_T0    equ 16                  ; MAME hides characters above Y=14 (NTSC)
    endif
L_TOP   equ 0                   ; say_line's lines: T0, T1
L_BOT   equ 1
NCOLS   equ 12                  ; a row's singles
ROW_TAB equ 2*NCOLS+1           ; a row: NCOLS x (pointer, control), sprite 1's Y
ROW_SPR equ 2*NCOLS             ; ... its last byte
NROWS   equ 8
    ifndef Y_ROW0
Y_ROW0  equ 30                  ; row 0's characters; row r's is +24r
    endif
ROW_PITCH equ 24
    ifndef Y_T1
Y_T1    equ 228                 ; T1 (MAME draws to line 242)
    endif
    ifndef LEAD
LEAD    equ 1                   ; lines a band starts before its line ends
    endif
    ifndef GRIDNOP
GRIDNOP equ 3                   ; h_grid: nops to put its write in the blank
    endif                       ; (NTSC)
PALPASS equ 190                 ; pal_check (p0.inc): the passes a frame from
                                ; which a console is PAL (MAME: NTSC ~100, PAL ~290)
    ifndef GRIDNOPP
GRIDNOPP equ 5                  ; ... and on PAL, more (h_gridp's jmp: 2 more)
    endif
    ifndef X_ROWTXT
X_ROWTXT equ 36                 ; a row's 12 characters, centred
    endif
X_ICON  equ X_ROWTXT-12         ; sprite 1 (a folder) left of a row
SPR_OFF equ 0F8h                ; a sprite's Y below the screen
    ifndef SPKNOIN
SPKNOIN equ 0                   ; spike: no input polls
    endif
    ifndef PADN
PADN    equ 0                   ; spike: pad a row band by 2+2n cycles
    endif

; ---- the stick (input.inc): JOY / JOYEV bits ----
J_UP    equ 01h
J_DOWN  equ 02h
J_LEFT  equ 04h
J_RIGHT equ 08h
J_FIRE  equ 10h

; ---- the keyboard (input.inc): BIOS readkey codes, row x 8 + column ----
K_0     equ 00h                 ; 0-9 are 00h-09h
K_SPACE equ 0Ch
K_QUEST equ 0Dh
K_L     equ 0Eh
K_P     equ 0Fh
K_PLUS  equ 10h
K_W     equ 11h
K_E     equ 12h
K_R     equ 13h
K_T     equ 14h
K_U     equ 15h
K_I     equ 16h
K_O     equ 17h
K_Q     equ 18h
K_S     equ 19h
K_D     equ 1Ah
K_F     equ 1Bh
K_G     equ 1Ch
K_H     equ 1Dh
K_J     equ 1Eh
K_K     equ 1Fh
K_A     equ 20h
K_Z     equ 21h
K_X     equ 22h
K_C     equ 23h
K_V     equ 24h
K_B     equ 25h
K_M     equ 26h
K_DOT   equ 27h
K_MINUS equ 28h
K_TIMES equ 29h                 ; x: the shift key
K_DIV   equ 2Ah                 ; divide: the second shift
K_EQ    equ 2Bh
K_YES   equ 2Ch
K_NO    equ 2Dh
K_CLR   equ 2Eh
K_ENT   equ 2Fh
K_NONE  equ 0FFh

; ---- sounds (snd.inc): beep ids; from SND_SOFT on they never cut another ----
SND_SELECT equ 0
SND_OK     equ 1                ; a name saved
SND_ERROR  equ 2
SND_BOOT   equ 3                ; the fanfare
SND_BACK   equ 4
SND_SOFT   equ 5
SND_MOVE   equ 5                ; a click
SND_KEY    equ 6                ; a key typed
SND_TICK   equ 7                ; the progress bar's tick
SND_N1     equ 8                ; the jingle's notes (snd.inc), then BOOT
SND_N2     equ 9
SND_N3     equ 10

; ---- pages, and their entries ----
PG_BOOT  equ 0
PG_LIB   equ 1                  ; the display's routines (p1.inc), by farcall
PG_LIST  equ 2                  ; the rooms
PG_NET   equ 3                  ; one GET (NREQ), back to RETPG/RETEN
PG_JOIN  equ 4
PG_MOUNT equ 5
PG_EDIT  equ 6
PG_HELP  equ 7
PG_EDLIB equ 8                  ; the editor's texts and magnifier, by farcall
PG_NAME  equ 9                  ; key 0: read, edited, saved
E_RUN    equ 0                  ; every page's: its screen

; HELP: the help screen on context ctx, back to page pg's entry en.
HELP    macro ctx,pg,en
    mov r0,#HELPCTX
    mov a,#ctx
    movx @r0,a
    mov r0,#HRET
    mov a,#(en<<4)|pg
    movx @r0,a
    mov r7,#E_RUN
    mov a,#PG_HELP
    jmp far_go
    endm

; GOTO: one way to another page's entry (page.inc far_go).
GOTO    macro pg,en
    mov r7,#en
    mov a,#pg
    jmp far_go
    endm
E_FETCH  equ 1                  ; the list's: this page again, fetched
E_SHOW   equ 2                  ; ... the GET is back (NRES)
E_EDIT   equ 1                  ; the name's: the editor on it
E_NAMED  equ 2                  ; ... the editor is back
E_REC    equ 1                  ; the join's: the room fetched again
E_SCAN   equ 2                  ; ... its neighbours, for a room that moved
E_HRET   equ 1                  ; the editor's: back from the help

LC_FIRST equ 0                  ; LCUR: the bar on the first room
LC_KEEP  equ 1                  ; ... where it was
LC_LAST  equ 0FFh               ; ... the last room
NQ_PAGE  equ 0                  ; NREQ: the list's page LPAGE
NQ_REC   equ 1                  ; ... the record at JABS
NQ_NEAR  equ 2                  ; ... five from JABS-2
NR_NONE  equ 0FFh               ; NRES (else 0, or the nDevStatus): no answer
NR_BAD   equ 0FEh               ; ... a reply that isn't the lobby's
E_NOLIST equ 1                  ; the help page's: the list has nothing

MQ_WAIT  equ 45                 ; a long name rests this long at each end
MQ_STEP  equ 8                  ; ... and steps a character this often

; the editor (p5.inc; its texts and magnifier, edtext.inc)
ED_ROW0 equ 1                   ; the field's first row
ED_ROWS equ 6
ED_MAGX equ 138                 ; the magnifier: sprite 2, zoomed (16 x 28),
ED_MAGY equ Y_T0                ; T0 and row 0: no band blanks it there
SH_NONE equ 0                   ; SHIFT
SH_ONE  equ 1                   ; x: the next key
SH_SYM  equ 2                   ; divide: the next key
SH_LOCK equ 3                   ; x x: until x

; the editor's kinds, the help's screens (p7.inc)
ED_NAME   equ 0
HC_LIST   equ 0
HC_EDIT   equ 1

; the library's entries (p1.inc; local.inc's doors)
L_SAYLINE  equ 0
L_SAYLINEC equ 1
L_ROWSTR   equ 2
L_ROWLINE  equ 3
L_SCRTEXT  equ 4
L_BAR      equ 5
L_GRID     equ 6
L_SPRAT    equ 7
L_SPRHIDE  equ 8
L_GCOL     equ 9
L_FOLDER   equ 10
L_COLOR    equ 11

; the editor's library's entries (p8.inc)
L_EDPROMPT equ 0
L_EDSTAT   equ 1
L_EDCHART  equ 2
L_MAG      equ 3
