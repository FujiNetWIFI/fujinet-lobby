#ifdef __C64__

#include <stdint.h>
#include <conio.h>
#include <peekpoke.h>
#include <c64.h>

#include "qr.h"

void initialize() {
  // Nothing for C64
}

uint8_t readJoystick() {
  return 127-PEEK(0xDC00); // joystick 2
}

void reboot() {
  // TBD

  // Soft reboot
  //__asm__("JMP $FCE2");
  
  // instead of boot, load the program?
  //cbm_load(...);
}

/*
 * QR rendering: one module per character cell.
 *
 * A reversed space is a solid block in the cell's colour, so with a white
 * screen and black colour RAM this draws black-on-white with no custom
 * character set. Modules are a full 8x8 pixels.
 *
 * Screen codes are written straight to $0400 rather than through cputc(),
 * which would translate PETSCII and turn $A0 into something else entirely.
 *
 * As on the Atari, 21 modules plus a 4-module margin is 29 rows against a
 * 25-row screen, so the border is set white to carry the top and bottom quiet
 * zone into overscan.
 */

#define QR_SCREEN_COLS  40
#define QR_SCREEN_ROWS  25
#define QR_SCREEN_RAM   ((uint8_t *) 0x0400)
#define QR_COLOR_RAM    ((uint8_t *) 0xD800)
#define QR_ORIGIN_X     ((QR_SCREEN_COLS - QR_MODULES) / 2)  /* 9 */
#define QR_ORIGIN_Y     0
#define QR_PROMPT_ROW   (QR_SCREEN_ROWS - 1)

/* Screen codes, not PETSCII: a space and a reversed space. */
#define QR_CELL_LIGHT   0x20
#define QR_CELL_DARK    0xA0

static uint8_t qr_saved_border, qr_saved_bg;

void qr_draw() {
  uint8_t x, y;
  uint16_t i;

  qr_saved_border = VIC.bordercolor;
  qr_saved_bg = VIC.bgcolor0;

  bgcolor(COLOR_WHITE);
  bordercolor(COLOR_WHITE);   /* extends the quiet zone into overscan */
  clrscr();

  /* Every cell black, so a reversed space reads as a black module and the
     prompt text is legible on the same white page. */
  for (i = 0; i < (uint16_t) QR_SCREEN_COLS * QR_SCREEN_ROWS; i++)
    QR_COLOR_RAM[i] = COLOR_BLACK;

  for (y = 0; y < QR_MODULES; y++)
    for (x = 0; x < QR_MODULES; x++)
      QR_SCREEN_RAM[(QR_ORIGIN_Y + y) * QR_SCREEN_COLS + QR_ORIGIN_X + x] =
        qr_get(x, y) ? QR_CELL_DARK : QR_CELL_LIGHT;

  cputsxy((QR_SCREEN_COLS - 22) / 2, QR_PROMPT_ROW, "SCAN TO CHAT - ANY KEY");
}

void qr_restore() {
  bgcolor(qr_saved_bg);
  bordercolor(qr_saved_border);
  clrscr();
}

char qr_wait_key() {
  return cgetc();
}
#endif /* __C64__ */