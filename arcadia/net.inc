; net.inc -- the N: round trip for one /view page (block 2).
;
; fujinet-5cardstud/arcadia's sequence, on CONFIG's mailbox calls. The
; rules it carries forward (o2, Intellivision, Astrocade):
;
;  * The CLOSE belongs at the START of the next request, never after the
;    READ: every transaction repaints the whole reply window, and the
;    page renders out of the window until the next request.
;  * The ESP32 reports STATUS as soon as SOME of the response has
;    arrived; poll until two consecutive readings agree (bounded).
;  * The 4th STATUS byte is nDevStatus and checking it is not optional:
;    the GET is deferred until the first STATUS, and an HTTP error page
;    still has a body. 170 is a 404, which is how /view says "no rooms".
;  * The reply length belongs to the latest transaction, so it is
;    captured right after the READ.

NCOPEN  EQU     $4F             ; 'O'
NCCLOSE EQU     $43             ; 'C'
NCREAD  EQU     $52             ; 'R'
NCSTAT  EQU     $53             ; 'S'
NMHTTPG EQU     $0C             ; open mode: HTTP GET
NTRNONE EQU     0               ; translation: none
NSDEVST EQU     3               ; STATUS reply: nDevStatus (1 = ok)
NOTFND  EQU     170             ; nDevStatus for a 404: an empty page
ERBAD   EQU     $FE             ; helper-local: the reply makes no sense
RXMAX   EQU     1024            ; the cart's reply window, 4 x 256
APTRIES EQU     60              ; STATUS polls, ~3 frames apart

; ---- the bin=1 record (server/model.go; 189 bytes, frozen) ---------------
RTYPE   EQU     0               ; game type = the game's appkey id
RGAME   EQU     1               ; game name, 17
RSERV   EQU     18              ; room (server) name, 33
RSURL   EQU     51              ; server URL, 65
RCURL   EQU     116             ; client URL, 65
RPLAY   EQU     185             ; players now
RMAXP   EQU     186             ; players max
RECLEN  EQU     189
GAMEND  EQU     RSERV           ; where the game name ends

; APICALL: GET the page at V_OFF. R0 = 0 / CC EQ with the reply in the
; window; NOTFND for an empty page; ERBAD for a reply too short to be
; one; else a transport error code. Calls four down. Clobbers R0-R3,
; V_TMP, V_AVL/H, V_PVL/H, V_RXL/H.
APICALL: BSTA,UN NCLOSE         ; the PREVIOUS request's connection
        BSTA,UN NOPEN
        BCFA,EQ APERR
        EORZ    R0
        STRA,R0 *PPVL
        STRA,R0 *PPVH
        LODI,R0 APTRIES
        STRA,R0 *PVTMP
APSET:  BSTA,UN NSTAT
        BCFA,EQ APERR
        LODA,R0 *PAVL
        IORA,R0 *PAVH
        BCTR,EQ APAGN           ; nothing has arrived yet
        LODA,R0 *PAVL
        COMA,R0 *PPVL
        BCFR,EQ APAGN
        LODA,R0 *PAVH
        COMA,R0 *PPVH
        BCTR,EQ APRD            ; two readings agree: it has landed
APAGN:  LODA,R0 *PAVL
        STRA,R0 *PPVL
        LODA,R0 *PAVH
        STRA,R0 *PPVH
        LODI,R3 3
APDLY:  ZBSR    *ZWAITVB
        BDRR,R3 APDLY
        LODA,R0 *PVTMP
        SUBI,R0 1
        STRA,R0 *PVTMP
        BCFA,EQ APSET
        LODA,R0 *PPVL           ; out of patience: read what came, if any
        IORA,R0 *PPVH
        BCFR,EQ APRD
        LODI,R0 FETIMO
        RETC,UN
APRD:   BSTA,UN NREAD
        BCFR,EQ APERR
        LODA,R0 *PRXH
        BCFR,EQ APOK
        LODA,R0 *PRXL
        COMI,R0 3
        BCFR,LT APOK
        LODI,R0 ERBAD           ; not even the header
        RETC,UN
APOK:   EORZ    R0
        RETC,UN
APERR:  COMI,R0 0               ; a failure must not read as success
        BCFR,EQ APE9
        LODI,R0 FETIMO
APE9:   RETC,UN

; NCLOSE: close the previous request's connection. A first call has
; nothing to close and earns a NAK; the result is ignored.
; Calls two down. Clobbers R0-R3.
NCLOSE: LODI,R3 NCCLOSE
        LODI,R2 0
        ZBSR    *ZMSTRTN
        BCTA,UN MCOMMIT

; NOPEN: OPEN the page's URL as an HTTP GET, no translation. Results as
; CGO. Calls three down.
NOPEN:  LODI,R3 NCOPEN
        LODI,R2 2
        ZBSR    *ZMSTRTN
        LODI,R1 NMHTTPG
        ZBSR    *ZMPARB
        LODI,R1 NTRNONE
        ZBSR    *ZMPARB
        BSTA,UN BLDURL
        ZBRR    *ZCGO

; NSTAT: one STATUS poll. R0 = 0 / CC EQ with the bytes waiting in
; V_AVL/H; else the device status or the transport error (CC not EQ).
; Calls three down.
NSTAT:  LODI,R3 NCSTAT
        LODI,R2 2
        ZBSR    *ZMSTRTN
        LODI,R1 0
        ZBSR    *ZMPARB
        LODI,R1 0
        ZBSR    *ZMPARB
        ZBSR    *ZCGO
        BCFR,EQ NST9
        LODA,R0 FNRDATA+NSDEVST
        COMI,R0 1
        BCFR,EQ NST9
        LODA,R0 FNRDATA
        STRA,R0 *PAVL
        LODA,R0 FNRDATA+1
        STRA,R0 *PAVH
        EORZ    R0
NST9:   RETC,UN

; NREAD: READ min(bytes waiting, RXMAX) into the window, then capture
; the reply length. Results as CGO. Calls three down.
NREAD:  LODI,R3 NCREAD
        LODI,R2 1
        ZBSR    *ZMSTRTN
        LODA,R0 FNDATA+2        ; one 2-byte parameter, little-endian
        LODA,R0 *PAVH
        COMI,R0 RXMAX>>8
        BCFR,LT NRCLMP
        LODA,R1 *PAVL
        LODA,R0 FNDATA,R1
        LODA,R1 *PAVH
        LODA,R0 FNDATA,R1
        BCTR,UN NRGO
NRCLMP: LODA,R0 FNDATA+(RXMAX&$FF)
        LODA,R0 FNDATA+(RXMAX>>8)
NRGO:   ZBSR    *ZCGO
        BCFR,EQ NRD9
        LODA,R0 FNRXLL          ; now: the next commit replaces it
        STRA,R0 *PRXL
        LODA,R0 FNRXLH
        STRA,R0 *PRXH
        EORZ    R0
NRD9:   RETC,UN

; BLDURL: stream N:<endpoint>?bin=1&platform=<platform>&pagesize=6
; &offset=<V_OFF> into TX; it is never assembled in RAM. No appkey=
; filter: with one the server sorts by status instead of game, and the
; grouping falls apart. Calls one down. Clobbers R0-R3, V_TXN.
BLDURL: LODA,R0 UENDPT          ; "N:" unless the endpoint carries its own
        COMI,R0 'N'
        BCTR,EQ BURL1
        LODA,R0 FNDATA+'N'
        LODA,R0 FNDATA+':'
BURL1:  SETFS1  UENDPT
        ZBSR    *ZCTXSTR
        SETFS1  UQPLAT
        ZBSR    *ZCTXSTR
        SETFS1  UPLAT
        ZBSR    *ZCTXSTR
        SETFS1  UQPAGE
        ZBSR    *ZCTXSTR
        LODA,R1 *PVOFF

; TXDEC: R1 (0-255) into TX in decimal, no leading zeros. Leaf.
; Clobbers R0-R3.
TXDEC:  LODI,R3 '0'
TXH1:   COMI,R1 100
        BCTR,LT TXH2
        SUBI,R1 100
        ADDI,R3 1
        BCTR,UN TXH1
TXH2:   LODI,R2 0               ; nonzero once a digit is out
        COMI,R3 '0'
        BCTR,EQ TXT0
        LODA,R0 FNDATA,R3
        LODI,R2 1
TXT0:   LODI,R3 '0'
TXT1:   COMI,R1 10
        BCTR,LT TXT2
        SUBI,R1 10
        ADDI,R3 1
        BCTR,UN TXT1
TXT2:   COMI,R2 0
        BCFR,EQ TXT3            ; after a hundreds digit, a zero prints
        COMI,R3 '0'
        BCTR,EQ TXO
TXT3:   LODA,R0 FNDATA,R3
TXO:    ADDI,R1 '0'
        LODA,R0 FNDATA,R1
        RETC,UN

        INCLUDE "build/endpoint.inc"
UQPLAT: DB      "?bin=1&platform=",0
UQPAGE: DB      "&pagesize=6&offset=",0
