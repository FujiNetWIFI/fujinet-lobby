#ifdef __ATARI5200__

/**
 * @brief   40x24 text screen for the Atari 5200 (ANTIC mode 2)
 *
 * The shared lobby client is written against cc65's conio, whose 5200 screen
 * is 20 columns of uppercase with no reverse video. This is the screen
 * CONFIG uses instead (fujinet-config atari5200/antic.c): the top 1K of RAM
 * ($3C00, reserved by the link) holds the characters, the display list and
 * the font are in ROM, and bit 7 of a character is reverse video.
 */

#include <stdint.h>

#include "atari5200/a52.h"

#define REG(a)   (*(volatile uint8_t *)(a))
#define SDLSTL   REG(0x05)
#define SDLSTH   REG(0x06)
#define SDMCTL   REG(0x07)
#define COLOR1   REG(0x0D)      /* mode 2: the text's luminance */
#define COLOR2   REG(0x0E)      /* mode 2: the background */
#define COLOR4   REG(0x10)      /* the border */
#define CHACTL   REG(0xD401)
#define CHBASE   REG(0xD409)

#define SCREEN   ((uint8_t *)0x3C00)
#define COLS     40
#define ROWS     24

extern const uint8_t a52_dlist[];       /* dl.s */
extern const uint8_t a52_font[];

static uint8_t cx, cy, rev;

/* ASCII -> ANTIC internal code */
static uint8_t code(uint8_t c)
{
  if (c < 0x20 || c > 0x7E)
    c = ' ';
  return c < 0x60 ? c - 0x20 : c;
}

void a52_clrscr(void)
{
  uint8_t *p = SCREEN;
  uint16_t n = COLS * ROWS;

  while (n--)
    *p++ = 0;
  cx = cy = 0;
}

void a52_gotoxy(uint8_t x, uint8_t y)
{
  cx = x < COLS ? x : COLS - 1;
  cy = y < ROWS ? y : ROWS - 1;
}

uint8_t a52_wherex(void) { return cx; }
uint8_t a52_wherey(void) { return cy; }

void a52_cputc(char c)
{
  if (c == '\r') {
    cx = 0;
    return;
  }
  if (c == '\n') {
    if (cy < ROWS - 1)
      cy++;
    return;
  }
  SCREEN[cy * COLS + cx] = code(c) | (rev ? 0x80 : 0);
  if (++cx >= COLS) {
    cx = 0;
    if (cy < ROWS - 1)
      cy++;
  }
}

void a52_cputs(const char *s)
{
  while (*s)
    a52_cputc(*s++);
}

void a52_cclear(uint8_t n)
{
  while (n--)
    a52_cputc(' ');
}

uint8_t a52_revers(uint8_t on)
{
  uint8_t old = rev;

  rev = on;
  return old;
}

void a52_screensize(uint8_t *x, uint8_t *y)
{
  *x = COLS;
  *y = ROWS;
}

uint8_t a52_textcolor(uint8_t color)
{
  uint8_t old = COLOR1;

  COLOR1 = color;
  return old;
}

uint8_t a52_bgcolor(uint8_t color)
{
  uint8_t old = COLOR2;

  COLOR2 = color;
  return old;
}

uint8_t a52_bordercolor(uint8_t color)
{
  uint8_t old = COLOR4;

  COLOR4 = color;
  return old;
}

/* Called from initialize() */
void a52_screen_init(void)
{
  a52_clrscr();
  CHBASE = (uint8_t)((uint16_t)a52_font >> 8);
  CHACTL = 2;                   /* code + $80 is reverse video */
  SDLSTL = (uint8_t)(uint16_t)a52_dlist;
  SDLSTH = (uint8_t)((uint16_t)a52_dlist >> 8);
  SDMCTL = 0x22;                /* normal playfield, display-list DMA */
}

#endif /* __ATARI5200__ */
