#ifdef __ATARI__

#include <stdint.h>
#include <conio.h>
#include <string.h>
#include <atari.h>
#include <joystick.h>

#include "qr.h"

void initialize() {
  OS.soundr=0; // Silent noisy SIO
}

uint8_t readJoystick() {
  return 15-OS.stick0 + (OS.strig0==0)*JOY_BTN_1_MASK;
}

void reboot() {
  OS.rtclok[2]=0;
  while (OS.rtclok[2] < 1);
  asm("JMP $E477");
}

/*
 * QR rendering: one module per character cell, in GR.0.
 *
 * A character's pixels are drawn in COLOR1's luminance over COLOR2, so an
 * inverse space is a solid block and a normal space is bare background. That
 * gives black-on-white with no custom character set at all, and modules a full
 * 8x8 pixels -- twice the size of packing 2x2 modules per cell, which would
 * also need two glyphs the ROM font does not have (the checkerboard pair).
 *
 * The cost is the vertical quiet zone: 21 modules plus the 4-module margin a
 * scanner wants is 29 rows, and the screen has 24. The border is set to the
 * same white so the margin continues into overscan, which is what the
 * Intellivision colored-squares experiment relied on and zbar decoded happily.
 * Worth checking on real hardware before trusting it.
 */

#define QR_SCREEN_COLS  40
#define QR_SCREEN_ROWS  24
#define QR_ORIGIN_X     ((QR_SCREEN_COLS - QR_MODULES) / 2)  /* 9 */
#define QR_ORIGIN_Y     0
#define QR_PROMPT_ROW   (QR_SCREEN_ROWS - 1)

/* Internal (screen) codes, not ATASCII: a bare space and an inverse space. */
#define QR_CELL_LIGHT   0x00
#define QR_CELL_DARK    0x80

static uint8_t qr_saved_color1, qr_saved_color2, qr_saved_color4;

void qr_draw() {
  uint8_t x, y;
  uint8_t *scr;

  qr_saved_color1 = OS.color1;
  qr_saved_color2 = OS.color2;
  qr_saved_color4 = OS.color4;

  clrscr();

  OS.color1 = 0x00;  /* luminance 0: black modules and black prompt text */
  OS.color2 = 0x0F;  /* white page */
  OS.color4 = 0x0F;  /* white border, extending the quiet zone into overscan */

  scr = OS.savmsc;
  for (y = 0; y < QR_MODULES; y++)
    for (x = 0; x < QR_MODULES; x++)
      scr[(QR_ORIGIN_Y + y) * QR_SCREEN_COLS + QR_ORIGIN_X + x] =
        qr_get(x, y) ? QR_CELL_DARK : QR_CELL_LIGHT;

  /* Two blank rows sit between the symbol and this, so the text stays clear of
     the quiet zone. */
  cputsxy((QR_SCREEN_COLS - 22) / 2, QR_PROMPT_ROW, "SCAN TO CHAT - ANY KEY");
}

void qr_restore() {
  OS.color1 = qr_saved_color1;
  OS.color2 = qr_saved_color2;
  OS.color4 = qr_saved_color4;
  clrscr();
}

char qr_wait_key() {
  return cgetc();
}
#endif /* __ATARI__ */