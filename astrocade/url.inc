; url.inc -- building the /view devicespec.
;
; Streamed straight into the mailbox's TX byte stream (5 Card Stud's way):
; never assembled in RAM. No appkey= filter: with one the server sorts by
; status instead of game name and the game-header grouping falls apart.
;
;   N:<ENDPOINT>?bin=1&platform=<PLATFORM>&pagesize=<V_PSZ>&offset=<V_OFF>

BLDURL: LD      A,(UENDPT)      ; "N:" unless the endpoint carries its own
        CP      'N'
        JR      Z,BURL1
        LD      A,'N'
        CALL    FNTXBYT
        LD      A,':'
        CALL    FNTXBYT
BURL1:  LD      HL,UENDPT
        CALL    FNTXSTR
        LD      HL,UQPLAT
        CALL    FNTXSTR
        LD      HL,UPLAT
        CALL    FNTXSTR
        LD      HL,UQPSZ
        CALL    FNTXSTR
        LD      A,(V_PSZ)
        CALL    TXDEC
        LD      HL,UQOFF
        CALL    FNTXSTR
        LD      A,(V_OFF)

; TXDEC: stream A as decimal. Clobbers A,BC,DE,HL.
TXDEC:  LD      HL,NUMBUF
        CALL    PUTDEC
        LD      (HL),0
        LD      HL,NUMBUF
        JP      FNTXSTR

UQPLAT: DB      "?bin=1&platform=",0
UQPSZ:  DB      "&pagesize=",0
UQOFF:  DB      "&offset=",0
