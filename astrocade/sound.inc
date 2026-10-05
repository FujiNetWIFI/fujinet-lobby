; sound.inc -- the lobby's two cues, written straight to the music
; processor.
;
; TONE/GATE/WAITB/PLAY are 5 Card Stud's (fujinet-5cardstud/astrocade/
; sound.inc). Pitch (MAME's astrocde.cpp model): the master oscillator
; divides the 1.7897725 MHz clock by MO+1, each tone divides that by T+1
; and toggles, so f = 1789773 / (2 * (MO+1) * (T+1)); with MO = 20H that
; is 27118 / (T+1). Lengths are ~10 ms units.

TONEA   EQU     11H             ; tone A period (TONMO+1)
SNDVIB  EQU     14H             ; vibrato; kept 0 so tones hold steady
MOHI    EQU     20H             ; master oscillator for every cue

; TONE: tone A at period A for B units, master oscillator C, volume
; (V_VOL). Preserves C, HL; clobbers A,B,DE.
TONE:   PUSH    AF
        LD      A,C
        OUT     (TONMO),A
        XOR     A
        OUT     (SNDVIB),A
        POP     AF
        OUT     (TONEA),A
        LD      A,(V_VOL)
        OUT     (VOLAB),A
        CALL    WAITB
        XOR     A
        OUT     (VOLAB),A
        RET

; WAITB: B (1-255) x ~10 ms of busy wait. Clobbers A,B,DE.
WAITB:  LD      DE,600
WAITB1: DEC     DE
        LD      A,D
        OR      E
        JR      NZ,WAITB1
        DJNZ    WAITB
        RET

; PLAY: HL = notes of (period, length, rest after), ended by a 0 period.
; Clobbers A,BC,DE,HL.
PLAY:   LD      C,MOHI
PLAY1:  LD      A,(HL)
        OR      A
        RET     Z
        INC     HL
        LD      B,(HL)
        INC     HL
        CALL    TONE
        LD      B,(HL)
        INC     HL
        LD      A,B
        OR      A
        CALL    NZ,WAITB
        JR      PLAY1

; SNDCUR: the bar moved -- a quiet 1 kHz tick, 20 ms, so a run of moves
; stays light. Clobbers A,BC,DE.
SNDCUR: LD      A,04H           ; tone A at half the cue volume
        LD      (V_VOL),A
        LD      A,26            ; ~1004 Hz
        LD      B,2
        LD      C,MOHI
        JP      TONE

; SNDOK: a room was chosen -- a rising C major arpeggio, C5 E5 G5 then
; a held C6, the affirmative answer to the trigger. Clobbers everything.
SNDOK:  LD      A,08H
        LD      (V_VOL),A
        LD      HL,FOK
        JR      PLAY

; Periods at MOHI: C5 523 Hz 51, E5 659 40, G5 784 34, C6 1047 25.
FOK:    DB      51,6,1, 40,6,1, 34,6,1, 25,16,0, 0
