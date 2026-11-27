#ifdef __MSX__

/**
 * @brief   Platform back-end for the MSX cartridge
 *
 * Everything here is either something src/platform.h requires, or a thin
 * bridge onto the MSX BIOS and FujiNet's user ROM boot.
 */

#include <stdint.h>
#include <video/tms99x8.h>
#include <fujinet-fuji.h>

#include "msx/vars.h"
#include "../io.h"

/* BIOS entry points. IX is saved because some BIOS routines trash it, and
   sccz80 keeps its frame pointer there. */
#define CHSNS   0x009C
#define CHGET   0x009F
#define GTSTCK  0x00D5
#define GTTRIG  0x00D8

#define CLIKSW  ((unsigned char *)0xF3DB)           /* keyboard click */
#define SCRMOD  ((unsigned char *)0xFCAF)           /* BIOS screen mode */

/* The key is left in the buffer; Z set means there is none */
unsigned char msx_kbhit(void) __naked
{
#asm
    push    ix
    call    CHSNS
    pop     ix
    ld      hl,0
    ret     z
    inc     l
    ret
#endasm
}

unsigned char msx_cgetc(void) __naked
{
#asm
    push    ix
    call    CHGET
    pop     ix
    ld      l,a
    ld      h,0
    ret
#endasm
}

static unsigned char gtstck(unsigned char dev) __z88dk_fastcall __naked
{
#asm
    push    ix
    ld      a,l
    call    GTSTCK
    pop     ix
    ld      l,a
    ld      h,0
    ret
#endasm
}

static unsigned char gttrig(unsigned char dev) __z88dk_fastcall __naked
{
#asm
    push    ix
    ld      a,l
    call    GTTRIG
    pop     ix
    ld      l,a
    ld      h,0
    ret
#endasm
}

/* GTSTCK's 1=up 2=up-right 3=right ... 8=up-left, in the JOY_* layout */
static const unsigned char dirs[9] = {
  0, 1, 1|8, 8, 2|8, 2, 2|4, 4, 1|4
};

/**
 * @brief Read joystick port 1
 *
 * The cursor keys are left to the keyboard path (cgetc), so this does not
 * read device 0.
 */
unsigned char readJoystick(void)
{
  unsigned char d, value;

  d = gtstck(1);
  value = d < 9 ? dirs[d] : 0;
  if (gttrig(1))
    value |= 16;

  return value;
}

/**
 * @brief Platform specific initialization
 *
 * Enters GRAPHICS II the way fujinet-config's screen_init() does, with a black
 * backdrop, then loads the pill caps into sprites 0 and 1.
 */
void initialize(void)
{
  *CLIKSW = 0;

  vdp_set_reg(7, (VDP_INK_WHITE << 4) | VDP_INK_BLACK);
  vdp_noblank();
  vdp_color(VDP_INK_WHITE, VDP_INK_BLACK, VDP_INK_BLACK);
  vdp_set_mode(2);
  vdp_color(VDP_INK_WHITE, VDP_INK_BLACK, VDP_INK_BLACK);

  /* vdp_set_mode() leaves the BIOS thinking this is SCREEN 1, so CHGET
     would draw its cursor into the name table, which must stay identity */
  *SCRMOD = 2;

  vdp_set_sprite_8(0, msx_pill_left);
  vdp_set_sprite_8(1, msx_pill_right);

  msx_clrscr();       /* hides the sprites too */
  vdp_blank();
}

/**
 * @brief Wait for vertical sync
 *
 * FujiNet calls go through CALSLT, which comes back with interrupts off, so
 * turn them back on and sleep until the VDP's frame interrupt. The BIOS
 * handler it runs also scans the keyboard for CHSNS/CHGET.
 */
void waitvsync(void) __naked
{
#asm
  ei
  halt
  ret
#endasm
}

/// @brief Boot the mounted ROM (never returns)
void reboot(void)
{
  fuji_enable_user_rom();
}

/**
 * @brief The menu's letters onto what the shared event loop understands
 */
void msx_menu_key(void)
{
  switch (input.key) {
    case 'v': case 'V':
    case 'p': case 'P':
      input.key = 0;
      input.trigger = true;
      break;
    case 'n': case 'N':
      input.key = 'c';
      break;
    case 'b': case 'B':
    case 'x': case 'X':
      input.key = 0x1B;
      break;
  }
}

#endif /* __MSX__ */
