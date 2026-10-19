; mailbox.inc -- the cartridge mailbox, spoken from page 1 (block 2).
;
; fujinet-config/arcadia's, plus the N: device and reply slices.
; Mirrors fujinet-firmware/pico/arcadia/firmware/include/fuji_mailbox.h,
; the single source of truth; every equate is a CONSOLE address (image
; offset + $1000 for image $1000 and up). The connector has no write
; strobe, so both directions ride reads:
;   - a register write is a REGSEL read of FNRSEL+reg and then, back to
;     back, a REGDATA read of FNRDAT+value (the pairing is the cart's
;     stray-read defense);
;   - a TX byte is a read of FNDATA+value;
;   - the reply window and status page are ROM bytes the cart repaints,
;     only on a commit or a slice select -- which is what lets screens
;     draw, and stream TX, straight out of the window.
; Block 2 shares page 1 with all of it, so every access here is a direct
; (indexed) load. Page-0 code reaches the window through FRDP and
; friends in txlib.inc.
;
; SEQ is always the cart's own persisted ACKSEQ + 1 (255 wraps to 1, 0
; reserved), never a local counter: console RESET restarts the lobby
; but not the cart.

; ---- reply bytes (repainted ROM) ----------------------------------------
FNRDATA EQU     $2B00           ; reply slice (RXSLICE picks which)
FNACKSQ EQU     $2C00           ; echoes SEQ once the reply is ready
FNSTATS EQU     $2C01           ; b0 link up
FNERR   EQU     $2C02           ; fb_status of the last transaction
FNREPLY EQU     $2C03           ; $06 ACK / $15 NAK
FNRXLL  EQU     $2C04           ; reply length, LO
FNRXLH  EQU     $2C05           ;   and HI
FNBSTAT EQU     $2C06           ; boot state
FNBPCT  EQU     $2C07           ; boot progress 0-100
FNBERR  EQU     $2C08           ; boot error
FNMAGF  EQU     $2C09           ; 'F'
FNMAGN  EQU     $2C0A           ; 'N'
FNSECHO EQU     $2C0C           ; echoes RXSLICE once a slice is whole

; ---- hotspot pages --------------------------------------------------------
FNRSEL  EQU     $2D00           ; read +reg: arm register
FNRDAT  EQU     $2E00           ; read +val: armed register = val
FNDATA  EQU     $2F00           ; read +val: append val to the TX stream
FNSWAP  EQU     $2DFE           ; armed only: serve the staged image

; ---- registers and values -------------------------------------------------
FRDEVIC EQU     $00
FRCMD   EQU     $01
FRNPARM EQU     $02
FRDATRS EQU     $05
FRRXSLC EQU     $06
FRSEQ   EQU     $10
FRBOOTL EQU     $11             ; BOOTLOCK: FNBLMAG arms the swap
FNBLMAG EQU     $B5
FNDFUJI EQU     $70             ; FujiBus id of the FujiNet itself
NETDEV  EQU     $71             ; N1:
FCACK   EQU     $06
FCNAK   EQU     $15
FETIMO  EQU     $FF             ; helper-local: the commit gave up
FBIDLE  EQU     0               ; FNBSTAT values
FBXFER  EQU     1
FBREADY EQU     2
FBFAIL  EQU     $80

; ---- Fuji commands ---------------------------------------------------------
FCCKEY  EQU     $DB             ; CLOSE_APPKEY
FCOKEY  EQU     $DC             ; OPEN_APPKEY (6 raw bytes)
FCRKEY  EQU     $DD             ; READ_APPKEY (u16 LE length, data)
FCWKEY  EQU     $DE             ; WRITE_APPKEY (raw data)
FCSDFP  EQU     $E2             ; SET_DEVICE_FULLPATH
FCWHSL  EQU     $F3             ; WRITE_HOST_SLOTS
FCRHSL  EQU     $F4             ; READ_HOST_SLOTS
FCMIMG  EQU     $F8             ; MOUNT_IMAGE
FCMHOST EQU     $F9             ; MOUNT_HOST

; ---- routines ---------------------------------------------------------------

; MREGWR: mailbox register R1 = value R2. Leaf. Clobbers R0.
MREGWR: LODA,R0 FNRSEL,R1
        LODA,R0 FNRDAT,R2
        RETC,UN

; MSTART: begin a Fuji-device transaction: TX rewound, device $70,
; command R3, R2 parameters to follow. MSTARTN: the same on the N:
; device. Leaf. Clobbers R0 only.
MSTARTN: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+NETDEV
        BCTR,UN MST1
MSTART: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+FNDFUJI
MST1:   LODA,R0 FNRSEL+FRCMD
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRNPARM
        LODA,R0 FNRDAT,R2
        RETC,UN

; MPARB: append a 1-byte parameter, value R1. Leaf. Clobbers R0.
MPARB:  LODA,R0 FNDATA+1
        LODA,R0 FNDATA,R1
        RETC,UN

; MSEQ: write SEQ = ACKSEQ + 1. R0 = the SEQ written, i.e. the ACKSEQ
; that will mean "done". Every commit makes the cart republish slice 0,
; so the slice cache is reset here. Leaf. Clobbers R0,R2.
MSEQ:   EORZ    R0
        STRA,R0 *PCURSL
        LODA,R0 FNACKSQ
        ADDI,R0 1
        BCFR,EQ MSQ1
        LODI,R0 1               ; 255 wraps past the reserved 0
MSQ1:   STRZ    R2
        LODA,R0 FNRSEL+FRSEQ
        LODA,R0 FNRDAT,R2
        LODZ    R2
        RETC,UN

; MCOMMIT: commit and poll for the ACKSEQ echo. R0 = FNERR (0 = ok) or
; FETIMO after ~12 s: 64K polls of ~180 us, past the cart's worst case
; for an ordinary command (3 s link wait + 5 s transact) -- a client
; that gave up early would reuse the SEQ and read the late reply as its
; own. Calls one down. Clobbers R0-R3.
MCOMMIT: BSTR,UN MSEQ
        LODI,R3 0               ; outer x256
MCMO:   LODI,R1 0               ; inner x256
MCMI:   LODI,R0 8               ; pad the poll (~12 NOPs' worth)
MCMD:   BDRR,R0 MCMD
        LODA,R0 FNACKSQ
        COMZ    R2
        BCTR,EQ MCMOK
        BDRR,R1 MCMI
        BDRR,R3 MCMO
        LODI,R0 FETIMO
        RETC,UN
MCMOK:  LODA,R0 FNERR
        RETC,UN

; CGO: commit and validate. R0 = 0 with CC EQ on an ACK; otherwise R0 is
; FNERR, FETIMO or the reply byte, with CC not EQ. CGOCHK validates a
; commit someone else waited out (R0 = FNERR). Calls two down.
; Clobbers R0-R3.
CGO:    BSTR,UN MCOMMIT
CGOCHK: COMI,R0 0
        BCFR,EQ CGO9
        LODA,R0 FNREPLY
        COMI,R0 FCACK
        BCFR,EQ CGO9
        EORZ    R0
CGO9:   RETC,UN

; CDO0: command R3, no parameters. CDO1: command R3, 1-byte param R1.
; Results as CGO. Calls two down. Clobbers R0-R3.
CDO0:   LODI,R2 0
        ZBSR    *ZMSTART
        BCTR,UN CGO
CDO1:   LODI,R2 1
        ZBSR    *ZMSTART
        ZBSR    *ZMPARB
        BCTR,UN CGO

; MSLICE: show reply slice R2 (0-3) and wait for FNSECHO (~1.5 s).
; R0 = 0 ok / FETIMO. Preserves R2. Leaf. Clobbers R0,R1,R3.
MSLICE: LODA,R0 FNRSEL+FRRXSLC
        LODA,R0 FNRDAT,R2
        LODI,R3 64
MSLO:   LODI,R1 0
MSLI:   LODA,R0 FNSECHO
        COMZ    R2
        BCTR,EQ MSLOK
        BDRR,R1 MSLI
        BDRR,R3 MSLO
        LODI,R0 FETIMO
        RETC,UN
MSLOK:  EORZ    R0
        RETC,UN

; MCHECK: R0 = 0 / CC EQ when the 'F','N' cart magic is present. Leaf.
MCHECK: LODA,R0 FNMAGF
        COMI,R0 'F'
        BCFR,EQ MCKNO
        LODA,R0 FNMAGN
        COMI,R0 'N'
        BCFR,EQ MCKNO
        EORZ    R0
        RETC,UN
MCKNO:  LODI,R0 $FF
        RETC,UN

; ---- pointer words: the page-0 cells page-1 code touches ------------------
; Indirect operands carry 15 bits, so STRA,R0 *PVOFF is page-1 code's
; three-byte "STRA,R0 V_OFF"; an indexed indirect (*PSTUBM,R3) reaches a
; whole page-0 buffer through one word.
PDSTRH: DWBE    DSTRP
PDSTRL: DWBE    DSTRP+1
PDCOL:  DWBE    DCOLOR
PFSTRH: DWBE    FSTRP
PFSTRL: DWBE    FSTRP+1
PVHOST: DWBE    V_HOST
PVTXN:  DWBE    V_TXN
PVT2:   DWBE    V_T2
PVT3:   DWBE    V_T3
PVT4:   DWBE    V_T4
PVT5:   DWBE    V_T5
PSTUBM: DWBE    STUB-1
PCURSL: DWBE    CURSLC
PVOFF:  DWBE    V_OFF
PVTMP:  DWBE    V_TMP
PAVL:   DWBE    V_AVL
PAVH:   DWBE    V_AVH
PPVL:   DWBE    V_PVL
PPVH:   DWBE    V_PVH
PRXL:   DWBE    V_RXL
PRXH:   DWBE    V_RXH
PVAKM:  DWBE    V_AKM
PVNEDT: DWBE    V_NEDT
