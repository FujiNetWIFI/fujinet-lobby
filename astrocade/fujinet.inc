; fujinet.inc -- N: device, Fuji-device commands, appkeys, and the lobby's
; binary wire format.
;
; The mailbox transport itself is fujilib.inc; the N: device values are
; 5 Card Stud's (fujinet-5cardstud/astrocade/fujinet.inc) and the Fuji
; command payload shapes are CONFIG's (fujinet-config/astrocade/fujicmd.inc).

; ---- Network device (N:) ----------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH             ; 'O'
NCCLOSE EQU     43H             ; 'C'
NCREAD  EQU     52H             ; 'R'
NCSTAT  EQU     53H             ; 'S'

NMHTTPG EQU     0CH             ; open mode: HTTP GET
NTRNONE EQU     0               ; translation: none

; NET_STATUS reply: 4 bytes in slice 0.
NSAVLO  EQU     0               ; bytes waiting, low
NSAVHI  EQU     1               ; bytes waiting, high
NSCONN  EQU     2               ; connected flag
NSDEVST EQU     3               ; nDevStatus_t; 1 = SUCCESS. An empty page
                                ; is an HTTP 404 and lands here, not as a
                                ; zero count (server/actions.go).

; ---- Fuji device commands beyond fujilib.inc's set --------------------
FCWHSL  EQU     0F3H            ; WRITE_HOST_SLOTS
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY

DEVSLOT EQU     0               ; device slot the client image mounts in
HSLOT   EQU     7               ; host slot the lobby claims (slot 8): the
                                ; others accumulate what other clients left

; Appkeys: creator 1 / app 1. Key 0 is the username every FujiNet client
; shares; key <game_type> carries the chosen room's server URL to the
; client about to boot (clients/src/main.c).
AKCRLO  EQU     1
AKCRHI  EQU     0
AKAPP   EQU     1
AKKNAME EQU     0
AKMRD   EQU     0
AKMWR   EQU     1

; ---- Wire format: /view?bin=1 -----------------------------------------
; A 3-byte header {count, 0, 0}, then count fixed 189-byte records
; (server/model.go). Five records fit the 1K reply window; every offset
; is read in place through state.inc.
PAGESZ  EQU     5               ; records per request
RECLEN  EQU     189
RXMAX   EQU     1024            ; the reply window

RGTYPE  EQU     0               ; game_type = the handoff appkey's key id
RGAME   EQU     1               ; game name, 17
RSERV   EQU     18              ; room / server name, 33
RSURL   EQU     51              ; server url, 65 -> appkey[game_type]
RCURL   EQU     116             ; client url, 65: scheme://host/path
RPLAY   EQU     185             ; players now
RMAXP   EQU     186             ; players max
