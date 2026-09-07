#if _CMOC_VERSION_

#include <stdbool.h>
#include <coco.h>
#include <cmoc.h>
#include <hirestxt.h>
#include "conio.h"

#define SCREEN_BUFFER (byte *)0xA00

#define BOX_TL 0xA0
#define BOX_TR 0xA1
#define BOX_BL 0xA2
#define BOX_BR 0xA3
#define BOX_RT 0xA4
#define BOX_LT 0xA5
#define BOX_V  0xA8
#define BOX_H  0xA9

static bool reverse = 0;

/* bit 1 = PMODE 4 colorset, bit 0 = inverted.
   0 black/green  1 green/black  2 black/white  3 white/black */
static byte colorset = 1;

void hirestxt_init(void)
{
  struct HiResTextScreenInit init =
    {
      SCREEN_COLS,
      writeCharAt_42cols,
      SCREEN_BUFFER,
      TRUE,
      (word *)0x112,
      0,
      NULL,
      NULL,
    };

  width(32);
  pmode(4, (byte *)init.textScreenBuffer);
  pcls(255);
  screen(1, (colorset >> 1) & 1);
  initHiResTextScreen(&init);
  setScreenInverted(colorset & 1);
}

void cycle_colorset(void)
{
  colorset = (colorset + 1) & 3;
  screen(1, (colorset >> 1) & 1);
  setScreenInverted(colorset & 1);
}

void hirestxt_close(void)
{
  closeHiResTextScreen();
  width(32);
  pmode(0, 0);
  screen(0, 0);
}

unsigned char kbhit(void)
{
  return (unsigned char)inkey();
}

char cgetc(void)
{
  return (char)waitkey(0);
}

void gotoxy(unsigned char x, unsigned char y)
{
  moveCursor(x, y);
}

void cputc(char c)
{
  writeChar((byte)c);
}

void cputs(const char *s)
{
  writeString(s);
}

unsigned char revers(unsigned char onoff)
{
  unsigned char was = reverse;

  reverse = onoff;
  setInverseVideoMode((BOOL)onoff);
  return was;
}

void cclear(unsigned char length)
{
  while (length--)
    writeChar(' ');
}

void screensize(unsigned char *x, unsigned char *y)
{
  *x = SCREEN_COLS;
  *y = SCREEN_ROWS - 1;   /* bottom row belongs to the frame */
}

/* Break the top border to hold a spaced label. */
static void inset(unsigned char x, const char *s)
{
  writeCharAt_42cols(x++, 0, BOX_RT);
  writeCharAt_42cols(x++, 0, ' ');
  while (*s)
    writeCharAt_42cols(x++, 0, (byte)*s++);
  writeCharAt_42cols(x++, 0, ' ');
  writeCharAt_42cols(x, 0, BOX_LT);
}

void draw_frame(const char *title, const char *user, unsigned char rule_y)
{
  unsigned char i;

  for (i = 1; i < SCREEN_COLS - 1; i++) {
    writeCharAt_42cols(i, 0, BOX_H);
    writeCharAt_42cols(i, SCREEN_ROWS - 1, BOX_H);
    writeCharAt_42cols(i, rule_y, BOX_H);
  }

  for (i = 1; i < SCREEN_ROWS - 1; i++) {
    writeCharAt_42cols(0, i, BOX_V);
    writeCharAt_42cols(SCREEN_COLS - 1, i, BOX_V);
  }

  writeCharAt_42cols(0, 0, BOX_TL);
  writeCharAt_42cols(SCREEN_COLS - 1, 0, BOX_TR);
  writeCharAt_42cols(0, SCREEN_ROWS - 1, BOX_BL);
  writeCharAt_42cols(SCREEN_COLS - 1, SCREEN_ROWS - 1, BOX_BR);
  writeCharAt_42cols(0, rule_y, BOX_LT);
  writeCharAt_42cols(SCREEN_COLS - 1, rule_y, BOX_RT);

  if (title)
    inset(2, title);
  if (user && *user)
    inset((unsigned char)(SCREEN_COLS - 6 - strlen(user)), user);
}

#endif
