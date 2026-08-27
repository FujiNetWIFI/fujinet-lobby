#ifdef __APPLE2__

#include <stdint.h>
#include <string.h>
#include <apple2.h>
#include <joystick.h>
#include <peekpoke.h>
#include <conio.h>

#include "qr.h"

static uint8_t installedDriver = 0, canReadJoystick=0;

unsigned char readJoystick() {

  if (!installedDriver) {
    installedDriver=1;

    if (joy_install(joy_static_stddrv) == JOY_ERR_OK) {
      canReadJoystick = joy_read(JOY_1) == 0;
    }
  }
  
  return canReadJoystick ? joy_read(JOY_1) : 0;
}


void initialize() {
  // Nothing for Apple II
}

void waitvsync() {
  static uint16_t i;
  // Aproximate a jiffy for the timer countdown
  for ( i=0;i<628;i++);
}


// Copied from https://github.com/FujiNetWIFI/fujinet-config/blob/main/src/apple2/io.c
// on 2024-10-2
void reboot(void)
{
  char ostype;
  int i;

  ostype = get_ostype() & 0xF0;
  
  if (ostype == APPLE_II ||
    ostype == APPLE_IIPLUS ||
    ostype == APPLE_IIE ||
    ostype == APPLE_IIEENH)
  {
    // Wait for fujinet disk ii states to be ready
    for (i = 0; i < 2000; i++)
    {
      if (i % 250 == 0)
      {
        cputs(".");
      }
    }
  }

  if (ostype == APPLE_IIIEM)
  {
    asm("STA $C082");  // disable language card (Titan3+2)
    asm("LDA #$77");   // enable A3 primary rom
    asm("STA $FFDF");
    asm("JMP $F4EE");  // jmp to A3 reset entry
  }
  else  // Massive brute force hack that takes advantage of MMU quirk. Thank you xot.
  {
    POKE(0xC00E,0); // CLRALTCHAR

    // Make the simulated 6502 RESET result in a cold start.
    // INC $03F4
    POKE(0x100,0xEE);
    POKE(0x101,0xF4);
    POKE(0x102,0x03);

    // Make sure to not get disturbed.
    // SEI
    POKE(0x103,0x78);

    // Disable Language Card (which is enabled for all cc65 programs).
    // LDA $C082
    POKE(0x104,0xAD);
    POKE(0x105,0x82);
    POKE(0x106,0xC0);

    // Simulate a 6502 RESET, additionally do it from the stack page to make the MMU
    // see the 6502 memory access pattern which is characteristic for a 6502 RESET.
    // JMP ($FFFC)
    POKE(0x107,0x6C);
    POKE(0x108,0xFC);
    POKE(0x109,0xFF);

    asm("JMP $0100");
  }
}

/*
 * QR rendering: lo-res graphics, mixed mode.
 *
 * Text mode is no good here. There is no way to set a background colour, so a
 * symbol drawn with inverse spaces would sit on a black page with no quiet
 * zone; and 21 modules plus a 4-module margin is 29 rows against a 24-row
 * screen. Lo-res gives 40x40 blocks above four text lines, which fits 29 with
 * room to spare on every side.
 *
 * Lo-res reuses the text page rather than needing its own buffer, which matters
 * because LobbyResponse is already several KB and the hi-res page at $2000
 * would collide with the program image.
 *
 * Each byte holds two vertically stacked blocks: the low nibble is the even
 * row, the high nibble the odd one. Blocks are 7 pixels by 4 scanlines, so
 * modules come out stretched roughly 1.6:1 horizontally -- recoverable by any
 * decoder, and a guaranteed quiet zone is the better trade.
 */

#define QR_LORES_COLS   40
#define QR_LORES_ROWS   40      /* mixed mode: 40 block rows, then 4 text rows */
#define QR_ORIGIN_X     ((QR_LORES_COLS - QR_MODULES) / 2)   /* 9 */
#define QR_ORIGIN_Y     ((QR_LORES_ROWS - QR_MODULES) / 2)   /* 9 */

#define QR_LORES_WHITE  15
#define QR_LORES_BLACK  0

/* Soft switches. Read through a volatile pointer: cc65's PEEK() is a plain
   dereference, so the compiler discards it as a statement with no effect and
   the mode never changes. */
#define QR_SOFT_SWITCH(addr) (*(volatile uint8_t *) (addr))

#define QR_SW_GRAPHICS  0xC050
#define QR_SW_TEXT      0xC051
#define QR_SW_MIXED     0xC053
#define QR_SW_PAGE1     0xC054
#define QR_SW_LORES     0xC056

/* Address of the byte holding lo-res block (x, y). Two block rows share a
   byte, on the text page's interleaved row bases. */
static uint8_t *qr_lores_addr(uint8_t x, uint8_t y) {
  uint8_t row = y >> 1;
  return (uint8_t *) (0x0400 + ((row & 7) << 7) + ((row >> 3) * 40) + x);
}

static void qr_lores_set(uint8_t x, uint8_t y, uint8_t colour) {
  uint8_t *p = qr_lores_addr(x, y);

  if (y & 1)
    *p = (*p & 0x0F) | (colour << 4);
  else
    *p = (*p & 0xF0) | colour;
}

void qr_draw() {
  uint8_t x, y;

  clrscr();

  QR_SOFT_SWITCH(QR_SW_GRAPHICS);
  QR_SOFT_SWITCH(QR_SW_MIXED);
  QR_SOFT_SWITCH(QR_SW_LORES);
  QR_SOFT_SWITCH(QR_SW_PAGE1);

  for (y = 0; y < QR_LORES_ROWS; y++)
    for (x = 0; x < QR_LORES_COLS; x++)
      qr_lores_set(x, y, QR_LORES_WHITE);

  for (y = 0; y < QR_MODULES; y++)
    for (x = 0; x < QR_MODULES; x++)
      if (qr_get(x, y))
        qr_lores_set(QR_ORIGIN_X + x, QR_ORIGIN_Y + y, QR_LORES_BLACK);

  /* The four mixed-mode text rows are 21 and 22 in text coordinates; the
     symbol ends 10 block rows above them, well clear of the quiet zone. */
  gotoxy(9, 22);
  cputs("SCAN TO CHAT - ANY KEY");
}

void qr_restore() {
  QR_SOFT_SWITCH(QR_SW_TEXT);
  clrscr();
}

char qr_wait_key() {
  return cgetc();
}

#endif /* __APPLE2__ */