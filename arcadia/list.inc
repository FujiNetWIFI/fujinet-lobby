; list.inc -- the room list (block 1).
;
;   row 0     LOBBY            THOMC     black band: title, username
;   rows 1-8  5 CARD STUD ====           game header, yellow, mosaic rule
;             |HIGH ROLLER 2/4           room: yellow bracket, white name,
;             |BASEMENT    0/4             players right-aligned
;             `AI TABLE    1/8           the group's last room
;   row 9     ---< PAGE 2 >---           rule; arrows only where a page is
;   rows 10-11                           the selected room's full name
;   row 12    FIRE JOIN ROOM             legend, four lines in turn
;
; One page is one fetch: N:<endpoint>?bin=1&platform=arcadia&pagesize=6
; &offset=<V_OFF>. The cart returns at most 1024 bytes, so five 189-byte
; records arrive whole and the sixth only partly -- but its game name
; (bytes 949-966) is there. So the sixth says whether more pages exist
; and whether the fifth room ends its group, with no extra request. The
; reply window stays valid for the life of the page (the next commit is
; the next page), so the detail panel and the join read it in place.
;
; Grouping: a game header goes before a room when it is the page's first
; row or its game differs from the room above. A header never ends a
; page alone (it needs its first room under it). V_ROWREC maps each row
; to its record, or $FF for a header; only rooms are selectable.
;
; Paging: V_PSTK[depth] is each visited page's offset; the next page
; starts at this one's offset + the records drawn.

LROWS   EQU     8               ; list rows (screen rows 1-8)
RULEC   EQU     AACC|$38        ; header rule: yellow, bottom half lit
RULEG   EQU     ANORM|GFTOP     ; the page rule on row 9
BRKMID  EQU     AACC|GFLFT      ; room bracket: the group goes on
BRKEND  EQU     AACC|GFBL       ;   the group's last room
LEGPER  EQU     120             ; frames each legend line shows

; ---- loading a page -------------------------------------------------------
; LSTLD: fetch and draw the page at V_OFF, select V_WANT, take keys.
LSTLD:  EORZ    R0
        STRA,R0 V_NROOM
        STRA,R0 V_MORE
        STRA,R0 V_NROW
        BSTA,UN LFRAME
        LODI,R0 TLOAD>>8
        LODI,R1 TLOAD&$FF
        ZBSR    *ZPANEL
        BSTA,UN APICALL
        BCTA,EQ LOK
        COMI,R0 NOTFND
        BCTA,EQ LEMPTY
        COMI,R0 ERBAD
        BCTA,EQ LBAD
        STRA,R0 V_ERRC
        LODI,R0 ENET>>8
        LODI,R1 ENET&$FF
LERR:   ZBSR    *ZPANEL
        DAT     11,14
        LODA,R0 V_ERRC
        ZBSR    *ZDHEX
        ZBSR    *ZSNDERR
        BSTA,UN LARROW
        ZBRR    *ZLKLOOP

LBAD:   LODI,R0 ERBAD
        STRA,R0 V_ERRC
        LODI,R0 EBADR>>8
        LODI,R1 EBADR&$FF
        BCTR,UN LERR

; An empty page past the first: the list shrank under us, so back up.
LEMPTY: LODA,R0 V_DEP
        BCTR,EQ LEM1
        LODI,R0 $FF
        STRA,R0 V_WANT
        BCTA,UN LPBACK
LEM1:   LODI,R0 TNONE>>8
        LODI,R1 TNONE&$FF
        ZBSR    *ZPANEL
        ZBRR    *ZLKLOOP

; LOK: the reply is in. Count the whole records and the readable game
; names, then lay the page out.
LOK:    LODA,R0 *FRDP           ; the count byte (slice 0 is showing)
        STRA,R0 V_CNT
        BCTR,EQ LEMPTY
        COMI,R0 7
        BCFR,LT LBAD            ; more than were asked for
        EORZ    R0
        STRA,R0 V_NREC
LN1:    LODA,R1 V_NREC
        COMI,R1 5
        BCTR,EQ LN2
        COMA,R1 V_CNT
        BCFR,LT LN2
        LODI,R0 RECLEN
        BSTA,UN RXHAS
        BCFR,EQ LN2
        LODA,R0 V_NREC
        ADDI,R0 1
        STRA,R0 V_NREC
        BCTR,UN LN1
LN2:    LODA,R0 V_NREC
        BCTA,EQ LBAD            ; records claimed, none whole
        STRA,R0 V_GAVL
        LODA,R1 V_NREC
        COMA,R1 V_CNT
        BCFR,LT LN3             ; nothing past the whole ones
        LODI,R0 GAMEND
        BSTA,UN RXHAS
        BCFR,EQ LN3
        LODA,R0 V_GAVL
        ADDI,R0 1
        STRA,R0 V_GAVL

LN3:    LODI,R1 LROWS
        LODI,R0 $FF
LN4:    STRA,R0 V_ROWREC-1,R1
        BDRR,R1 LN4
        EORZ    R0
        STRA,R0 V_RI
        LODI,R1 0
        BSTA,UN GLOAD
        LODI,R0 1
        STRA,R0 V_HDR

; One record per pass: its header if it needs one, then the room.
LDR1:   LODA,R0 V_RI
        COMA,R0 V_NREC
        BCFA,LT LDONE           ; no whole record left
        LODA,R0 V_NROW          ; rows it needs: the room + its header
        ADDI,R0 1
        ADDA,R0 V_HDR
        COMI,R0 LROWS
        BCTA,GT LDONE           ; the page is full
        LODA,R0 V_HDR
        BCTR,EQ LDR2
        BSTA,UN LHEAD
LDR2:   EORZ    R0              ; is the next record the same game?
        STRA,R0 V_SAME
        LODI,R0 BRKEND
        STRA,R0 V_BRK
        LODA,R1 V_RI
        ADDI,R1 1
        COMA,R1 V_CNT
        BCFR,LT LDR4            ; there is no next: the group ends
        COMA,R1 V_GAVL
        BCFR,LT LDR3            ; next unreadable: assume it goes on
        BSTA,UN GSAME
        BCFR,EQ LDR4
        LODI,R0 1
        STRA,R0 V_SAME
LDR3:   LODI,R0 BRKMID
        STRA,R0 V_BRK
LDR4:   BSTA,UN LROOM
        LODA,R0 V_RI
        ADDI,R0 1
        STRA,R0 V_RI
        EORZ    R0
        STRA,R0 V_HDR
        LODA,R0 V_SAME
        BCFA,EQ LDR1            ; same game: no header
        LODI,R0 1
        STRA,R0 V_HDR
        LODA,R1 V_RI
        COMA,R1 V_GAVL
        BCFA,LT LDR1            ; (the loop ends on it anyway)
        BSTA,UN GLOAD
        BCTA,UN LDR1

LDONE:  LODA,R0 V_RI
        ADDA,R0 V_OFF
        STRA,R0 V_NEXT
        TPSL    $01             ; past offset 255 the URL cannot follow
        BCTR,EQ LSEL
        LODA,R0 V_RI
        COMA,R0 V_CNT
        BCFR,LT LSEL            ; everything the server sent is shown
        LODI,R0 1
        STRA,R0 V_MORE

; LSEL: put the bar on V_WANT if that is a room on this page, on the
; last room for $FF, else on the first room.
LSEL:   LODA,R1 V_WANT
        COMI,R1 $FF
        BCTR,EQ LSLAST
        COMA,R1 V_NROW
        BCFR,LT LSFRST
        LODA,R0 V_ROWREC,R1
        COMI,R0 $FF
        BCFR,EQ LSSET
LSFRST: LODI,R1 0
LSF1:   LODA,R0 V_ROWREC,R1
        COMI,R0 $FF
        BCFR,EQ LSSET
        ADDI,R1 1
        BCTR,UN LSF1
LSLAST: LODA,R1 V_NROW          ; a page always ends on a room
        SUBI,R1 1
LSSET:  STRA,R1 V_CUR
        ADDI,R1 1
        BSTA,UN RHILIT
        BSTA,UN LDETL
        BSTA,UN LARROW
        ZBRR    *ZLKLOOP

; ---- drawing ------------------------------------------------------------
; LFRAME: the screen around the list: title band, page rule, an empty
; panel, the legend. Calls three down. Clobbers R0-R3.
LFRAME: BSTA,UN DCLS
        LODI,R0 ASEL            ; the title band
        DAT     0,0
        LODI,R3 16
        ZBSR    *ZDFILL
        DCOL    ASELA
        LODI,R0 TTITLE>>8
        LODI,R1 TTITLE&$FF
        BSTA,UN PRSC
        LODI,R3 $FF             ; the name, right-aligned
LF1:    LODA,R0 V_NAME,R3,+
        BCFR,EQ LF1
        LODI,R0 15
        SUBZ    R3
        STRZ    R2
        DCOL    ASEL
        SETSTR  V_NAME
        ZBSR    *ZDPRINT
        LODI,R0 RULEG           ; the page rule
        DAT     9,0
        LODI,R3 16
        ZBSR    *ZDFILL
        PRINT   TPAGE
        DAT     9,10
        LODA,R0 V_DEP
        ADDI,R0 1
        ZBSR    *ZDDEC
        EORZ    R0
        ZBSR    *ZDPUTC
        BCTA,UN LEGDRW

; LARROW: the page arrows on the rule, where there is a page to go to.
; Leaf. Clobbers R0.
LARROW: LODA,R0 V_DEP
        BCTR,EQ LA1
        LODI,R0 AACC|GLARR
        STRA,R0 SCREEN+9*16+3
LA1:    LODA,R0 V_MORE
        BCTR,EQ LA2
        LODI,R0 AACC|GRARR
        STRA,R0 SCREEN+9*16+12
LA2:    RETC,UN

; LHEAD: the game header for V_GBUF on list row V_NROW: the name in
; yellow, then -- when at least two cells are left -- the mosaic switch
; and a yellow rule to the row's end. Calls two down. Clobbers R0-R3.
LHEAD:  LODA,R0 V_NROW
        BSTA,UN ROWCEL
        DCOL    AACC
        SETSTR  V_GBUF
        ZBSR    *ZDPRINT
        LODZ    R2              ; R2 is the last cell written
        ADDI,R0 1
        ANDI,R0 $0F
        BCTR,EQ LH9             ; the name filled the row
        LODI,R0 $C0             ; the switch: blank, mosaic from here
        STRA,R0 SCREEN,R2,+
LH1:    LODZ    R2
        ADDI,R0 1
        ANDI,R0 $0F
        BCTR,EQ LH9
        LODI,R0 RULEC
        STRA,R0 SCREEN,R2,+
        BCTR,UN LH1
LH9:    LODA,R0 V_NROW
        ADDI,R0 1
        STRA,R0 V_NROW
        RETC,UN

; LROOM: room V_RI on list row V_NROW with bracket V_BRK: the name in
; the cells the players leave, a gap, "now/max" ending in column 15.
; Calls three down. Clobbers R0-R3, V_T0, V_T1, V_FLD.
LROOM:  LODA,R0 V_NROW
        BSTA,UN ROWCEL
        LODA,R0 V_BRK
        STRA,R0 SCREEN,R2,+
        STRA,R2 V_CEL
        LODA,R1 V_RI
        LODI,R0 RPLAY
        ZBSR    *ZRXREC
        ZBSR    *ZRXNEXT
        STRA,R0 V_PCUR
        ZBSR    *ZRXNEXT
        STRA,R0 V_PMAX
        BSTA,UN NDIG            ; name cells = 14 - (digits + 1 + digits)
        STRA,R0 V_T1
        LODA,R0 V_PCUR
        BSTA,UN NDIG
        ADDA,R0 V_T1
        STRA,R0 V_T1
        LODI,R0 13
        SUBA,R0 V_T1
        STRA,R0 V_T1
        LODA,R1 V_RI
        LODI,R0 RSERV
        ZBSR    *ZRXREC
        DCOL    ANORM
        LODA,R2 V_CEL
        LODA,R0 V_T1
        ZBSR    *ZRXFLD
        EORZ    R0              ; the gap
        ZBSR    *ZDPUTC
        LODA,R0 V_PCUR
        ZBSR    *ZDDEC
        LODI,R0 GDIAG
        ZBSR    *ZDPUTC
        LODA,R0 V_PMAX
        ZBSR    *ZDDEC
        LODA,R1 V_NROW
        LODA,R0 V_RI
        STRA,R0 V_ROWREC,R1
        ADDI,R1 1
        STRA,R1 V_NROW
        LODA,R0 V_NROOM
        ADDI,R0 1
        STRA,R0 V_NROOM
        RETC,UN

; LDETL: the panel shows the selected room's whole name (up to 32
; characters, rows 10 and 11 run on). Calls three down.
LDETL:  BSTA,UN PANCLR
        LODA,R1 V_CUR
        LODA,R0 V_ROWREC,R1
        STRZ    R1
        LODI,R0 RSERV
        ZBSR    *ZRXREC
        DCOL    ASEL
        DAT     10,0
        LODI,R0 32
        ZBRR    *ZRXFLD

; GLOAD: V_GBUF = record R1's game name. Calls three down.
GLOAD:  LODI,R0 RGAME
        ZBSR    *ZRXREC
        LODI,R2 V_GBUF&$FF
        LODI,R3 GAMEND-RGAME-1
        BCTA,UN RXCOPY

; GSAME: R0 = 0 / CC EQ when record R1's game name equals V_GBUF.
; Calls two down. Clobbers R0-R3.
GSAME:  LODI,R0 RGAME
        ZBSR    *ZRXREC
        LODI,R2 0
GS1:    ZBSR    *ZRXNEXT
        COMA,R0 V_GBUF,R2
        BCFR,EQ GSNE
        COMI,R0 0
        BCTR,EQ GSEQ            ; both ended together
        ADDI,R2 1
        COMI,R2 GAMEND-RGAME
        BCTR,LT GS1
GSEQ:   EORZ    R0
        RETC,UN
GSNE:   LODI,R0 1
        RETC,UN

; ---- the legend -----------------------------------------------------------
; LEGDRW: legend line V_LEGI on row 12. Calls three down.
LEGDRW: DAT     12,0
        LODI,R3 16
        ZBSR    *ZDROWS
        LODA,R0 V_LEGI
        RRL,R0
        STRZ    R3
        LODA,R0 LEGTAB+1,R3
        STRZ    R1
        LODA,R0 LEGTAB,R3
        ZBRR    *ZPRS
LEGTAB: DWBE    LEG0
        DWBE    LEG1
        DWBE    LEG2
        DWBE    LEG3
NLEG    EQU     4

; LEGTIC: one frame of the legend's clock: every LEGPER frames the next
; line. Calls four down.
LEGTIC: LODA,R0 V_FRM
        ADDI,R0 1
        STRA,R0 V_FRM
        COMI,R0 LEGPER
        BCFR,LT LGT1
        RETC,UN
LGT1:   EORZ    R0
        STRA,R0 V_FRM
        LODA,R0 V_LEGI
        ADDI,R0 1
        COMI,R0 NLEG
        BCTR,LT LGT2
        EORZ    R0
LGT2:   STRA,R0 V_LEGI
        BCTR,UN LEGDRW

; ---- keys -------------------------------------------------------------------
LKLOOP: BSTR,UN LEGTIC
        ZBSR    *ZKEYSCN
        COMI,R0 KNONE
        BCTR,EQ LKLOOP
        COMI,R0 KDOWN
        BCTR,EQ LKDN
        COMI,R0 KUP
        BCTA,EQ LKUP
        COMI,R0 KRIGHT
        BCTA,EQ LPNEXT
        COMI,R0 KLEFT
        BCTA,EQ LPPREV
        COMI,R0 KFIRE
        BCTA,EQ LJOIN
        COMI,R0 KENT
        BCTA,EQ LJOIN
        COMI,R0 0
        BCTA,EQ LTOP
        COMI,R0 KCLR
        BCTA,EQ LNAME
        BCTR,UN LKLOOP

; Down: the next room, or the next page's first.
LKDN:   LODA,R0 V_NROOM
        BCTR,EQ LKLOOP
        LODA,R1 V_CUR
LKD1:   ADDI,R1 1
        COMA,R1 V_NROW
        BCFR,LT LKD2
        LODA,R0 V_ROWREC,R1
        COMI,R0 $FF
        BCTR,EQ LKD1            ; a header
        BCTR,UN LMOVE
LKD2:   LODA,R0 V_MORE
        BCTA,EQ LKLOOP
        EORZ    R0
        STRA,R0 V_WANT
        BCTA,UN LPGO

; Up: the room above, or the previous page's last.
LKUP:   LODA,R0 V_NROOM
        BCTA,EQ LKLOOP
        LODA,R1 V_CUR
LKU1:   COMI,R1 0
        BCTR,EQ LKU2
        SUBI,R1 1
        LODA,R0 V_ROWREC,R1
        COMI,R0 $FF
        BCTR,EQ LKU1
        BCTR,UN LMOVE
LKU2:   LODA,R0 V_DEP
        BCTA,EQ LKLOOP
        LODI,R0 $FF
        STRA,R0 V_WANT
        BCTA,UN LPBACK

; LMOVE: the bar from V_CUR to row R1.
LMOVE:  STRA,R1 V_T2
        LODA,R1 V_CUR
        ADDI,R1 1
        BSTA,UN RHILIT
        LODA,R1 V_T2
        STRA,R1 V_CUR
        ADDI,R1 1
        BSTA,UN RHILIT
        BSTA,UN LDETL
        ZBSR    *ZSNDMOV
        BCTA,UN LKLOOP

; Right / left: a page on, a page back.
LPNEXT: LODA,R0 V_MORE
        BCTA,EQ LKLOOP
        EORZ    R0
        STRA,R0 V_WANT
LPGO:   LODA,R1 V_DEP
        ADDI,R1 1
        COMI,R1 PAGEMAX
        BCTR,LT LPG1
        ZBSR    *ZSNDERR        ; the page stack is full
        BCTA,UN LKLOOP
LPG1:   STRA,R1 V_DEP
        LODA,R0 V_NEXT
        STRA,R0 V_PSTK,R1
        STRA,R0 V_OFF
        BCTR,UN LPSND
LPPREV: LODA,R0 V_DEP
        BCTA,EQ LKLOOP
        EORZ    R0
        STRA,R0 V_WANT
LPBACK: LODA,R1 V_DEP
        SUBI,R1 1
        STRA,R1 V_DEP
        LODA,R0 V_PSTK,R1
        STRA,R0 V_OFF
LPSND:  BSTA,UN SNDPG
        BCTA,UN LSTLD

; 0: the list again from its first page.
LTOP:   EORZ    R0
        STRA,R0 V_DEP
        STRA,R0 V_OFF
        STRA,R0 V_WANT
        BCTR,UN LPSND

; Clear: the name editor, then this page again.
LNAME:  LODA,R0 V_CUR
        STRA,R0 V_WANT
        BCTA,UN NAMED
