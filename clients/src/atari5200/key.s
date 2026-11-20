; key.s -- the keypad's IRQ.
;
; The keypad is scanned by POKEY and only shows up as a keyboard IRQ (SKSTAT's
; key-down bit is almost never seen low), so the BIOS's IRQ and key-code
; table do the work and this is VKYBDF, the vector the BIOS jumps through
; with the key number in A: 0-9, $0A '*', $0B '#', $0C START, $0D PAUSE,
; $0E RESET. The stack holds the interrupted A, X and Y.

        .export _a52_key_init, _a52_key

VKYBDF  = $020A
POKMSK  = $00
IRQEN   = $E80E

        .bss
; The latest key + 1, 0 when none has come since platform.c cleared it. A
; held key interrupts on every scan, so this refreshes while it is down.
_a52_key:       .res    1

        .code
_a52_key_init:
        sei
        lda     #<key_irq
        sta     VKYBDF
        lda     #>key_irq
        sta     VKYBDF+1
        lda     #0
        sta     _a52_key
        lda     #$40                    ; the keyboard IRQ, alone
        sta     POKMSK
        sta     IRQEN
        cli
        rts

key_irq:
        cmp     #$0F
        bcs     @done
        clc
        adc     #1
        sta     _a52_key
@done:  pla
        tay
        pla
        tax
        pla
        rti
