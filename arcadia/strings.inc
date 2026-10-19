; strings.inc -- every literal the screens print (block 2). DPRINT and
; PRS reach a string through DSTRP with an INDIRECT load, which carries
; the full 15 bits, so page-0 callers can name page-1 strings.
;
; Positioned strings (STRAT) start with their screen cell; a $01 byte
; toggles the accent ink. Rows run on into the next, so a panel message
; is one string across rows 10 and 11.

; ---- the splash and the frame ------------------------------------------------
TSPL1:  STRAT   5,4
        DB      1,"FUJINET",0
TSPL2:  STRAT   7,3
        DB      "GAME LOBBY",0
TTITLE: STRAT   0,0
        DB      "LOBBY",0
TPAGE:  STRAT   9,4
        DB      " PAGE ",0
TNOCRT: STRAT   4,0
        DB      "NO FUJINET CART",0

; ---- the panel (rows 10-11; LERR adds a hex code at row 11 col 14) ---------
TLOAD:  STRAT   10,0
        DB      "LOADING ROOMS",0
TNONE:  STRAT   10,0
        DB      "NO ROOMS OPEN   ",1,"0",1," LOOKS AGAIN",0
ENET:   STRAT   10,0
        DB      "NETWORK ERROR   ",1,"0",1," RETRIES",0
EBADR:  STRAT   10,0
        DB      "BAD REPLY       ",1,"0",1," RETRIES",0
ENGAME: STRAT   10,0
        DB      "NOT A GAME ROOM",0
EBURL:  STRAT   10,0
        DB      "BAD CLIENT URL",0

; ---- legends (row 12, shown in turn) -------------------------------------------
LEG0:   STRAT   12,1
        DB      1,"FIRE",1," JOIN ROOM",0
LEG1:   STRAT   12,1
        DB      1,"4",1,"/",1,"6",1," TURN PAGE",0
LEG2:   STRAT   12,1
        DB      1,"0",1," REFRESH LIST",0
LEG3:   STRAT   12,0
        DB      1,"CLR",1," CHANGE NAME",0
LEDIT:  STRAT   11,0
        DB      1,"FIRE",1," ADD ",1,"CLR",1," DEL"
        DB      1,"ENTER",1," OK  ",1,"0",1," ESC",0

; ---- the name ------------------------------------------------------------------
TYNAME: STRAT   0,0
        DB      1,"YOUR NAME",0
ENAME:  STRAT   10,0
        DB      "USE 2-8 A-Z 0-9",0
TDEFN:  DB      "ARCADIA",0

; ---- joining (row 10 messages; UFAIL adds a hex code in cols 14-15) ------------
TJOIN:  STRAT   0,4
        DB      1,"JOINING",0
TNOPOW: STRAT   12,0
        DB      "DO NOT POWER OFF",0
EHOSTS: STRAT   10,0
        DB      "HOSTS ERROR",0
EMHOST: STRAT   10,0
        DB      "HOST ERROR",0
EPATH:  STRAT   10,0
        DB      "PATH ERROR",0
EMOUNT: STRAT   10,0
        DB      "MOUNT ERROR",0
ESTAGE: STRAT   10,0
        DB      "STAGE ERROR",0
EBTOOB: STRAT   10,0
        DB      "ROM TOO BIG",0
EBTRNC: STRAT   10,0
        DB      "TRUNCATED",0
ENCART: STRAT   10,0
        DB      "NOT A CART",0
