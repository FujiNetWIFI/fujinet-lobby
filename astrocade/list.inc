; list.inc -- the room list: fetch, group, draw, navigate.
;
; Screen (40 x 15 cells of 4x6):
;   row 0       FUJINET GAME LOBBY ................ name
;   row 1       GAME / ROOM ....................... PLAYERS
;   rows 2-12   the list, LROWS lines
;   row 13      status / page indicator
;   row 14      legend
;
; Grouping follows the coleco port: a game header (color 2) whenever the
; game name changes and always at the top of a page, then its rooms
; (color 3) indented under it, players right-aligned. A header is never
; left alone on the last row -- its record starts the next page instead.
; Only rooms are selectable.
;
; Nothing of a page is kept but ROWREC, the absolute record offset each
; row stands for (0FFH = a header row): the text lives on the screen, the
; highlight bar is the room row's bytes complemented in place (rooms use
; colors 0 and 3 only, so CPL swaps them), and joining re-fetches its one
; record. PGSTK holds the start offset of every page visited, so paging
; back never has to re-walk the list.

LROWS   EQU     11              ; list rows
LTOP    EQU     2               ; first list row on screen
MAXPG   EQU     32              ; PGSTK entries

; ----------------------------------------------------------------------
; LSTSCR: the list screen, from the first page. Never returns: CE and
; the trigger leave through their own screens and come back here.
LSTSCR: XOR     A
        LD      (V_PG),A
        LD      (PGSTK),A
LSTRDR: CALL    CLRSCR
        LD      HL,TTITLE
        LD      DE,0*256+0
        LD      C,XCHROM
        CALL    TXTAT
        LD      HL,PLNBUF       ; the name, right-aligned
        CALL    STRLEN
        LD      A,40
        SUB     B
        LD      E,A
        LD      D,0
        LD      HL,PLNBUF
        LD      C,XROOM
        CALL    TXTAT
        LD      HL,TCOLS
        LD      DE,1*256+0
        LD      C,XCHROM
        CALL    TXTAT
        LD      HL,TPLAYS
        LD      DE,1*256+32
        LD      C,XCHROM
        CALL    TXTAT
        LD      HL,TLEGND
        LD      DE,14*256+0
        LD      C,XCHROM
        CALL    TXTAT
LSTNEW: CALL    LSTPG           ; draw the page at PGSTK[V_PG]
        XOR     A
        LD      (V_SEL),A
        CALL    SELDN           ; the first room, if there is one
        CALL    KEYRAW          ; a key still held from the previous
        LD      (V_KEY),A       ; screen must not fire here

LSTKEY: CALL    KEYSCN
        OR      A
        JR      Z,LSTKEY
        CP      'D'
        JR      NZ,LK1
        CALL    SELDN
        JR      NC,LKMOV        ; moved within the page
        LD      A,(V_MORE)      ; off the bottom: the next page
        OR      A
        JR      Z,LSTKEY
LKNXT:  CALL    PGNEXT
        JR      C,LSTKEY
LKFST:  CALL    SNDCUR
        JR      LSTNEW
LK1:    CP      'U'
        JR      NZ,LK2
        CALL    SELUP
        JR      NC,LKMOV
        CALL    PGPREV          ; off the top: the previous page, its
        JR      C,LSTKEY        ; last room
        CALL    SNDCUR
        CALL    LSTPG
        LD      A,(V_ROWS)
        LD      (V_SEL),A
        CALL    SELUP
        JR      LSTKEY
LK2:    CP      'R'
        JR      NZ,LK3
        LD      A,(V_MORE)
        OR      A
        JR      NZ,LKNXT
        JR      LSTKEY
LK3:    CP      'L'
        JR      NZ,LK4
        CALL    PGPREV
        JR      C,LSTKEY
        JR      LKFST
LK4:    CP      'T'
        JR      NZ,LK5
        LD      A,(V_ROWS)      ; nothing listed: nothing to join
        OR      A
        JR      Z,LSTKEY
        CALL    SNDOK
        JP      JOIN            ; returns here only through LSTRDR
LK5:    CP      '='
        JR      NZ,LK6
        CALL    SNDCUR
        JR      LSTNEW          ; refetch this page
LK6:    CP      'E'
        JR      NZ,LSTKEY
        CALL    NAMESCR         ; CE: change the name
        CALL    NAMPUT
        JP      LSTRDR

; The bar moved within the page: SELDN/SELUP already redrew it.
LKMOV:  CALL    SNDCUR
        JR      LSTKEY

; ----------------------------------------------------------------------
; SELDN / SELUP: move the bar to the next / previous room row on this
; page. Carry set (and nothing drawn) if there is none. With no bar
; drawn (V_BAR 0FFH), SELDN lands on the first room and SELUP searches
; up from V_SEL.
SELDN:  LD      A,(V_BAR)       ; the row after the bar; no bar (0FFH)
        INC     A               ; wraps to row 0
SDN2:   LD      HL,V_ROWS
        CP      (HL)
        CCF
        RET     C               ; past the last row
        LD      C,A
        CALL    ROWGET
        CP      0FFH
        LD      A,C
        JR      NZ,SELTO
        INC     A
        JR      SDN2

SELUP:  LD      A,(V_BAR)
        CP      0FFH
        JR      NZ,SUP0
        LD      A,(V_SEL)       ; no bar: start from V_SEL (the page end
SUP0:   OR      A               ; after a backwards page flip)
        SCF
        RET     Z               ; nothing above row 0
        DEC     A
        LD      C,A
        CALL    ROWGET
        CP      0FFH
        LD      A,C
        JR      NZ,SELTO
        JR      SUP0

; SELTO: bar off the old row, onto row A. Carry clear.
SELTO:  PUSH    AF
        LD      A,(V_BAR)
        CP      0FFH
        CALL    NZ,BARXOR
        POP     AF
        LD      (V_BAR),A
        LD      (V_SEL),A
        CALL    BARXOR
        OR      A
        RET

; BARXOR: complement list row A's six lines, 40 bytes each: a room row
; turns to background-on-white and back. Preserves A.
BARXOR: PUSH    AF
        ADD     A,LTOP
        CALL    ROWADR
        LD      B,6*BYTEPL
BXR1:   LD      A,(HL)
        CPL
        LD      (HL),A
        INC     HL
        DJNZ    BXR1
        POP     AF
        RET

; ROWGET: A = ROWREC[A]. Clobbers HL, DE.
ROWGET: LD      HL,ROWREC
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
        RET

; ROWADR: HL = screen address of text row A. Clobbers DE.
ROWADR: ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,ROWTAB
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      HL,NORMEM
        ADD     HL,DE
        RET

; ----------------------------------------------------------------------
; PGNEXT / PGPREV: step V_PG for the caller to draw. Carry set if there
; is no such page.
PGNEXT: LD      A,(V_PG)
        INC     A
        CP      MAXPG
        CCF
        RET     C
        LD      (V_PG),A
        LD      HL,PGSTK
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      A,(V_NXT)
        LD      (HL),A
        JR      PGOK
PGPREV: LD      A,(V_PG)
        OR      A
        SCF
        RET     Z
        DEC     A
        LD      (V_PG),A
PGOK:   XOR     A
        LD      (V_SEL),A
        OR      A
        RET

; ----------------------------------------------------------------------
; LSTPG: fetch and draw the page starting at PGSTK[V_PG]. Leaves V_ROWS,
; ROWREC, V_MORE and V_NXT, the page indicator or an error on row 13, and
; no bar (V_BAR 0FFH).
LSTPG:  LD      A,0FFH
        LD      (V_BAR),A
        LD      HL,ROWREC
        LD      B,LROWS
LP0:    LD      (HL),A
        INC     HL
        DJNZ    LP0
        LD      A,LTOP          ; wipe the list and the status row
        CALL    ROWADR
        LD      D,0
        LD      C,BYTEPL
        LD      B,(LROWS+1)*6
        CALL    FILLRC
        LD      HL,TLOAD
        CALL    STATUS
        XOR     A
        LD      (V_ROWS),A
        LD      (V_MORE),A
        LD      HL,PGSTK        ; V_CUR = this page's first record
        LD      A,(V_PG)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      A,(HL)
        LD      (V_CUR),A

LPFET:  LD      A,(V_CUR)       ; the next chunk of PAGESZ records
        LD      (V_OFF),A
        LD      A,PAGESZ
        LD      (V_PSZ),A
        CALL    APICALL
        JR      NC,LPGOT
        LD      A,(V_HTTPE)     ; an error status is the empty page's
        OR      A               ; 404: the end of the list
        JP      NZ,LPEND
        LD      HL,ENET
        JP      STATUS
LPGOT:  LD      HL,0
        CALL    RXGETB          ; count
        CP      PAGESZ+1
        JR      NC,LPBAD
        LD      (V_CNT),A
        OR      A
        JP      Z,LPEND
        CALL    RECOFF          ; the reply must hold every record it counts
        LD      DE,(RXLEN)
        EX      DE,HL
        OR      A
        SBC     HL,DE
        JR      NC,LPREC0
LPBAD:  LD      HL,EBAD
        JP      STATUS
LPREC0: XOR     A
        LD      (V_I),A

LPREC:  LD      A,(V_I)
        LD      HL,V_CNT
        CP      (HL)
        JR      C,LPR1
        LD      A,(HL)          ; chunk used up: a short one ends the list
        CP      PAGESZ
        JP      C,LPEND
        LD      A,(V_CUR)
        ADD     A,PAGESZ
        JP      C,LPEND         ; 255 records is all a byte offset reaches
        LD      (V_CUR),A
        JP      LPFET
LPR1:   CALL    RECOFF          ; HL = this record's reply offset
        LD      (V_REC),HL
        LD      A,(V_ROWS)      ; a header first? always atop a page
        OR      A
        LD      C,2
        JR      Z,LPR2
        INC     HL              ; ...and whenever the game changes
        CALL    GMCMP
        LD      C,1
        JR      Z,LPR2
        LD      C,2
LPR2:   LD      A,(V_ROWS)      ; does it fit?
        ADD     A,C
        CP      LROWS+1
        JR      C,LPR3
        LD      A,1             ; no: it starts the next page
        LD      (V_MORE),A
        LD      A,(V_CUR)
        LD      HL,V_I
        ADD     A,(HL)
        LD      (V_NXT),A
        JP      LPEND
LPR3:   DEC     C
        JR      Z,LPROOM
        LD      HL,(V_REC)      ; the game header
        INC     HL
        LD      DE,PREVGM
        LD      B,16
        CALL    RXSTRN
        LD      A,(V_ROWS)
        ADD     A,LTOP
        LD      D,A
        LD      E,0
        LD      HL,PREVGM
        LD      C,XGAME
        CALL    TXTAT
        LD      HL,V_ROWS       ; ROWREC already says header (0FFH)
        INC     (HL)
LPROOM: LD      HL,(V_REC)      ; the room: name...
        LD      DE,RSERV
        ADD     HL,DE
        LD      DE,LINBUF
        LD      B,30
        CALL    RXSTRN
        LD      A,(V_ROWS)
        ADD     A,LTOP
        LD      D,A
        LD      E,2
        LD      HL,LINBUF
        LD      C,XROOM
        CALL    TXTAT
        LD      HL,(V_REC)      ; ...and players, "cur/max" ending col 38
        LD      DE,RPLAY
        ADD     HL,DE
        CALL    RXGETB
        LD      C,A
        INC     HL
        CALL    RXGETB
        PUSH    AF
        LD      A,C
        LD      HL,LINBUF
        CALL    PUTDEC
        LD      (HL),'/'
        INC     HL
        POP     AF
        CALL    PUTDEC
        LD      (HL),0
        LD      A,39            ; LINBUF is page aligned: L = length
        SUB     L
        LD      E,A
        LD      A,(V_ROWS)
        ADD     A,LTOP
        LD      D,A
        LD      HL,LINBUF
        LD      C,XROOM
        CALL    TXTAT
        LD      A,(V_ROWS)      ; ROWREC[row] = V_CUR + V_I
        CALL    ROWGET
        LD      A,(V_CUR)
        LD      DE,V_I
        EX      DE,HL
        ADD     A,(HL)
        LD      (DE),A
        LD      HL,V_ROWS
        INC     (HL)
        LD      HL,V_I
        INC     (HL)
        JP      LPREC

; The page is complete: the indicator, or the empty-list message.
LPEND:  LD      A,(V_ROWS)
        OR      A
        LD      HL,ENONE
        JR      Z,STATUS
        LD      HL,LINBUF       ; "< PAGE n >", arrows where they lead
        LD      A,(V_PG)
        OR      A
        LD      A,' '
        JR      Z,LPE1
        LD      A,'<'
LPE1:   LD      (HL),A
        INC     HL
        LD      DE,TPAGE
LPE2:   LD      A,(DE)
        LD      (HL),A
        INC     HL
        INC     DE
        OR      A
        JR      NZ,LPE2
        DEC     HL
        LD      A,(V_PG)
        INC     A
        CALL    PUTDEC
        LD      A,(V_MORE)
        OR      A
        LD      A,' '
        JR      Z,LPE3
        LD      A,'>'
LPE3:   LD      (HL),' '
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),0
        LD      HL,LINBUF
        LD      DE,13*256+14
        JR      STAT1

; STATUS: message HL on row 13 in the chrome color, the row cleared
; first. Clobbers everything.
STATUS: LD      DE,13*256+0
STAT1:  PUSH    HL
        PUSH    DE
        LD      A,13
        CALL    ROWADR
        LD      D,0
        LD      C,BYTEPL
        LD      B,6
        CALL    FILLRC
        POP     DE
        POP     HL
        LD      C,XCHROM
        JP      TXTAT

; RECOFF: HL = reply offset of record A (0-5), 3 + A*189.
RECOFF: LD      HL,RECTAB
        ADD     A,A
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL
        RET
RECTAB: DW      3,3+RECLEN,3+2*RECLEN,3+3*RECLEN,3+4*RECLEN,3+5*RECLEN

; GMCMP: Z if the game name at reply offset HL equals PREVGM (compared
; through the NUL). Preserves HL.
GMCMP:  PUSH    HL
        LD      DE,PREVGM
        LD      B,17
GMC1:   CALL    RXGETB
        LD      C,A
        LD      A,(DE)
        CP      C
        JR      NZ,GMC9
        OR      A
        JR      Z,GMC9          ; both ended: equal
        INC     HL
        INC     DE
        DJNZ    GMC1
        XOR     A               ; 17 equal bytes
GMC9:   POP     HL
        RET

; PUTDEC: A as decimal at (HL), leading zeros suppressed; HL ends just
; past the last digit. Clobbers A,BC,DE.
PUTDEC: LD      C,A
        LD      D,0             ; no digit yet
        LD      B,100
        CALL    PDG
        LD      B,10
        CALL    PDG
        LD      A,C
        ADD     A,'0'
        LD      (HL),A
        INC     HL
        RET
PDG:    LD      A,C
        LD      E,'0'-1
PDG1:   INC     E
        SUB     B
        JR      NC,PDG1
        ADD     A,B
        LD      C,A
        LD      A,E
        CP      '0'
        JR      NZ,PDG2
        INC     D
        DEC     D
        RET     Z               ; a leading zero
PDG2:   LD      D,1
        LD      (HL),A
        INC     HL
        RET

; STRLEN: B = length of the NUL-terminated string at HL. Clobbers A.
STRLEN: PUSH    HL
        LD      B,0
STL1:   LD      A,(HL)
        OR      A
        JR      Z,STL2
        INC     HL
        INC     B
        JR      STL1
STL2:   POP     HL
        RET

TTITLE: DB      "FUJINET GAME LOBBY",0
TCOLS:  DB      "GAME / ROOM",0
TPLAYS: DB      "PLAYERS",0
TLEGND: DB      "TRIG JOIN  L/R PAGE  CE NAME  = REFRESH",0
TPAGE:  DB      " PAGE ",0
TLOAD:  DB      "LOADING...",0
ENET:   DB      "NETWORK ERROR - = RETRIES",0
EBAD:   DB      "BAD REPLY - = RETRIES",0
ENONE:  DB      "NO ROOMS OPEN - = REFRESHES",0
