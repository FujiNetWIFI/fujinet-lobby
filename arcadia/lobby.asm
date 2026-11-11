; lobby.asm -- the FujiNet Game Lobby for the Emerson Arcadia 2001.
;
; Lists the game rooms the lobby server knows for platform "arcadia",
; grouped under their games, and joins one: the room's client image is
; mounted from its host and booted, with the room's server URL left in
; appkey 1/1/<game type> for the game to find. The same behaviour as the
; other console lobbies (astrocade on the nes branch, sms, coleco).
;
; The platform layer is a snapshot of fujinet-config/arcadia (display,
; input, editor, mailbox, boot); the N: round trip is
; fujinet-5cardstud/arcadia's, rewritten to CONFIG's mailbox calls.
; CONFIG's GAME LOBBY (hosts key 9) boots this image from
; ec.tnfs.io:/arcadia/lobby.bin.
;
; Layout: block 1 (CPU $0000-$0FFF) holds everything that touches the
; screen or RAM; block 2 (CPU $2000-$2AFF) shares 2650 page 1 with the
; mailbox and holds the transport, the Fuji and N: commands, and the
; tables and strings. Cross-page calls are BSTA/BCTA (15-bit);
; cross-page DATA goes through DWBE pointer words, and AS refuses a
; direct operand on the wrong page.
;
; Every state is entered with BCTA on whatever the return stack holds:
; the 2650's 8-entry stack wraps, so an abandoned frame costs nothing,
; and tools/checkdepth.py bounds every live call chain.
;
; RESET on the console restarts this program but not the cart, which is
; why sequence numbers always derive from ACKSEQ and START initialises
; every variable.

        CPU     2650

        INCLUDE "macros.inc"

; ---- RAM ledger ---------------------------------------------------------
; 13-line mode leaves: zone A $18D0-$18EF, the island $18F8-$18FB, and
; zone B $1A00-$1AFF (the lower screen in 26-line mode, never used
; here). 292 bytes.

; -- zone A --
V_CUR   EQU     $18D0           ; selected list row 0-7
V_NROW  EQU     $18D1           ; rows drawn on this page
V_NROOM EQU     $18D2           ; rooms drawn on this page
V_CNT   EQU     $18D3           ; records the reply claims (<= 6)
V_NREC  EQU     $18D4           ; whole records in the reply
V_GAVL  EQU     $18D5           ; records whose game name is readable
V_MORE  EQU     $18D6           ; nonzero: a next page exists
V_NEXT  EQU     $18D7           ; offset of the next page
V_OFF   EQU     $18D8           ; offset of this page
V_DEP   EQU     $18D9           ; page depth (V_PSTK index)
V_WANT  EQU     $18DA           ; row to select after a load ($FF last)
V_FLD   EQU     $18DB           ; DDEC / RXFLD field counter
DCOLOR  EQU     $18DC           ; attribute DPUTC ORs in
V_KEY   EQU     $18DD           ; last raw control (edge detect)
V_T0    EQU     $18DE           ; leaf-helper scratch
V_T1    EQU     $18DF
; $18E0-$18EA is where the boot stub lands, so only state that is dead
; at the handover lives here.
V_RPT   EQU     $18E0           ; auto-repeat countdown
AXV     EQU     $18E1           ; cached vertical pot
AXH     EQU     $18E2           ; cached horizontal pot
AXSEL   EQU     $18E3           ; axis KEYRAW samples this frame
V_T2    EQU     $18E4           ; screen-level scratch
V_T3    EQU     $18E5
V_T4    EQU     $18E6
V_T5    EQU     $18E7
V_T6    EQU     $18E8
V_T7    EQU     $18E9
V_T8    EQU     $18EA
FSTRP   EQU     $18EB           ; TX string pointer, big-endian
DSTRP   EQU     $18ED           ; display string pointer, big-endian
V_CEL   EQU     $18EF           ; a row's screen cell across calls
;       $18F0-$18F7 sprite positions (hardware)
V_FRM   EQU     $18F8           ; legend frame counter
V_LEGI  EQU     $18F9           ; legend shown (0-3)
V_PCUR  EQU     $18FA           ; a room's players, now / max
V_PMAX  EQU     $18FB
;       $18FC-$18FF UVI registers

; -- zone B --
; The join buffers are dead outside a join, so the page draw's game
; name (V_GBUF) and the editor's working copy (V_NEDT) sit inside them.
ZONEB   EQU     $1A00
V_SURL  EQU     $1A00           ; 65: the room's server URL
V_GBUF  EQU     $1A00           ; 17: the group's game name (page draw)
V_NEDT  EQU     $1A20           ; 9: the name being edited / read
V_CURL  EQU     $1A41           ; 65: the room's client URL
V_HNAME EQU     $1A82           ; 33: its host, for host slot 8
V_PSTK  EQU     $1AA3           ; 32: page-start offsets by depth
V_ROWREC EQU    $1AC3           ; 8: per row, record index or $FF (header)
V_NAME  EQU     $1ACB           ; 9: the username
V_EBUF  EQU     $1AD4           ; u16 HI,LO: the editor's buffer
V_EMAX  EQU     $1AD6           ; editor max length
V_ELEN  EQU     $1AD7           ; editor current length
V_GX    EQU     $1AD8           ; editor grid cursor
V_GY    EQU     $1AD9
V_TXN   EQU     $1ADA           ; payload bytes streamed (CPAD's count)
V_HOST  EQU     $1ADB           ; host slot the join mounts (7)
V_GTYPE EQU     $1ADC           ; the room's game type = its appkey id
V_PATHO EQU     $1ADD           ; where the path starts in V_CURL
V_RI    EQU     $1ADE           ; record being drawn / joined
V_ERRC  EQU     $1ADF           ; the code UFAIL shows
V_RXL   EQU     $1AE0           ; reply length, LO / HI
V_RXH   EQU     $1AE1
V_AVL   EQU     $1AE2           ; STATUS bytes waiting, LO / HI
V_AVH   EQU     $1AE3
V_PVL   EQU     $1AE4           ; the previous reading, LO / HI
V_PVH   EQU     $1AE5
RXSLC   EQU     $1AE6           ; reply cursor: slice
RXOFF   EQU     $1AE7           ;   and offset within it
CURSLC  EQU     $1AE8           ; the slice the cart is publishing
V_TMP   EQU     $1AE9           ; settle-loop / retry counter
V_SAV   EQU     $1AEA           ; RXNEXT's shelter for R2
V_SAME  EQU     $1AEB           ; the next record is the same game
V_HDR   EQU     $1AEC           ; the next room needs a game header
V_AKM   EQU     $1AED           ; appkey open mode
V_BRK   EQU     $1AEE           ; the room row's bracket cell
;       $1AEF-$1AFF spare

PAGEMAX EQU     32              ; V_PSTK entries
NAMEMAX EQU     8

; ---- cartridge header ---------------------------------------------------
        ORG     $0000
        BCTA,UN START
        DB      $17             ; RETC,UN: the interrupt guard at $0003

; ---- zero-page vectors -----------------------------------------------------
; ZBSR / ZBRR reach $0000-$003F from ANY page in two bytes, and their
; indirect form jumps through a pointer word there -- so this table
; turns the most-used unconditional calls and jumps into 2-byte
; instructions: ZBSR *ZPRS is "BSTA,UN PRS".
ZPRS:    DWBE    PRS
ZMPARB:  DWBE    MPARB
ZWAITVB: DWBE    WAITVB
ZMSTART: DWBE    MSTART
ZCDO0:   DWBE    CDO0
ZDPUTC:  DWBE    DPUTC
ZUTITLE: DWBE    UTITLE
ZUFAIL:  DWBE    UFAIL
ZBEEP:   DWBE    BEEP
ZCTXSTR: DWBE    CTXSTR
ZSNDSEL: DWBE    SNDSEL
ZSNDBCK: DWBE    SNDBCK
ZBARPCT: DWBE    BARPCT
ZCGO:    DWBE    CGO
ZELOOP:  DWBE    ELOOP
ZKEYSCN: DWBE    KEYSCN
ZSNDERR: DWBE    SNDERR
ZDROWS:  DWBE    DROWS
ZDHEX:   DWBE    DHEX
ZRXNEXT: DWBE    RXNEXT
ZRXREC:  DWBE    RXREC
ZDFILL:  DWBE    DFILL
ZMSTRTN: DWBE    MSTARTN
ZDPRINT: DWBE    DPRINT
ZDDEC:   DWBE    DDEC
ZLSTLD:  DWBE    LSTLD
ZLKLOOP: DWBE    LKLOOP
ZPANEL:  DWBE    PANEL
ZRXFLD:  DWBE    RXFLD
ZSNDMOV: DWBE    SNDMOV

START:  LODI,R0 $20             ; interrupts inhibited
        LPSU
        LODI,R0 $02             ; COM=1: logical compares; WC off
        LPSL
        EORZ    R0              ; RESET leaves RAM arbitrary
        STRA,R0 AXSEL
        STRA,R0 V_FRM
        STRA,R0 V_LEGI
        STRA,R0 V_OFF
        STRA,R0 V_DEP
        STRA,R0 V_WANT
        STRA,R0 V_PSTK
        STRA,R0 V_NAME
        STRA,R0 CURSLC
        LODI,R0 JOYCTR          ; a cache that starts centred cannot
        STRA,R0 AXV             ;   report a phantom direction
        STRA,R0 AXH
        LODI,R0 KNONE
        STRA,R0 V_KEY
        BSTA,UN DINI
        BSTA,UN MCHECK
        BCTA,EQ SPLASH
        PRINT   TNOCRT          ; no mailbox: show what was read, halt
        DAT     6,6
        LODA,R0 *FMAGFP
        ZBSR    *ZDHEX
        LODI,R1 1
        LODA,R0 *FMAGFP,R1
        ZBSR    *ZDHEX
        ZBSR    *ZSNDERR
HALTNC: BCTR,UN HALTNC

; ---- block 1 ------------------------------------------------------------
MB_DISP:
        INCLUDE "disp.inc"
MB_INPUT:
        INCLUDE "input.inc"
MB_BAR:
        INCLUDE "bar.inc"
MB_TXLIB:
        INCLUDE "txlib.inc"
MB_UI:
        INCLUDE "ui.inc"
MB_EDIT:
        INCLUDE "edit.inc"
MB_RX:
        INCLUDE "rx.inc"
MB_LIST:
        INCLUDE "list.inc"
MB_NAMENT:
        INCLUDE "nament.inc"
MB_JOIN:
        INCLUDE "join.inc"
MB_END1:

; ---- block 2 ------------------------------------------------------------
        ORG     $2000
MB_MAILBOX:
        INCLUDE "mailbox.inc"
MB_NET:
        INCLUDE "net.inc"
MB_APPKEY:
        INCLUDE "appkey.inc"
MB_BOOT:
        INCLUDE "boot.inc"
MB_SOUND:
        INCLUDE "sound.inc"
MB_TABLES:
        INCLUDE "tables.inc"
MB_GLYPHS:
        INCLUDE "glyphs.inc"
MB_STRING:
        INCLUDE "strings.inc"
MB_END2:
