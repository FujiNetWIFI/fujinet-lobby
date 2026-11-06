#ifdef __TRS80M2__

/* Console bytes go straight to the BIOS jump table: the BDOS console calls
   expand tabs and act on ^S and ^C. P&T's BIOS keeps IX, IY and the
   alternate registers but may change any of the rest. */

#include "con.h"

// (0001) points at WBOOT, base+3: CONST is +3 from there, CONIN +6, CONOUT +9
void con_out(uint8_t c) __z88dk_fastcall __naked
{
#asm
    EXTERN  l_dcal
    ld      c,l
    ld      hl,(1)
    ld      de,9
    add     hl,de
    jp      l_dcal      ; CONOUT
#endasm
}

// The next key, or -1 when none is waiting
int con_in(void) __naked
{
#asm
    EXTERN  l_dcal
    ld      hl,(1)
    ld      de,3
    add     hl,de
    call    l_dcal      ; CONST: A = FF when a key is waiting
    or      a
    ld      hl,-1
    ret     z
    ld      hl,(1)
    ld      de,6
    add     hl,de
    call    l_dcal      ; CONIN
    ld      l,a
    ld      h,0
    ret
#endasm
}

// P&T's clock in 1/100 s since boot (special function 14)
uint32_t con_ticks(void) __naked
{
#asm
    ld      b,14
    call    40h         ; DE = low word, HL = high word
    ex      de,hl
    ret
#endasm
}

static void special(uint8_t n) __z88dk_fastcall __naked
{
#asm
    ld      b,l
    jp      40h
#endasm
}

// Video memory appears at F800h between special functions 21 and 22, with
// no system calls in between
void con_vram_put(uint16_t off, uint8_t c)
{
  special(21);
  ((volatile uint8_t *)0xF800)[off] = c;
  special(22);
}

#endif /* __TRS80M2__ */
