#ifdef __TRS80M2__

#include <stdint.h>
#include <string.h>
#include "con.h"
#include "vid.h"

static uint8_t cx, cy, attr;
static int pending = -1;

void m2_gotoxy(unsigned char x, unsigned char y)
{
  cx = x;
  cy = y;
}

void m2_cputc(char c)
{
  if (c == '\r')
    cx = 1;   // column 0 is the frame
  else if (c == '\n')
    cy++;
  else
    vid_put(cx++, cy, (c < 0x20 || c > 0x7E ? '?' : (uint8_t)c) | attr);
}

void m2_cputs(const char *s)
{
  while (*s)
    m2_cputc(*s++);
}

void m2_clrscr(void)
{
  static uint8_t y;

  for (y = 0; y < VID_ROWS; y++)
    vid_fill(0, y, VID_COLS, ' ');
  cx = cy = 0;
}

void cclear(unsigned char length)
{
  while (length--)
    m2_cputc(' ');
}

unsigned char revers(unsigned char onoff)
{
  static unsigned char prev;

  prev = attr ? 1 : 0;
  attr = onoff ? REV : 0;
  return prev;
}

// The screen is sent whenever the lobby looks for a key
unsigned char m2_kbhit(void)
{
  vid_flush();
  if (pending < 0)
    pending = con_in();
  return pending >= 0;
}

unsigned char m2_cgetc(void)
{
  static unsigned char c;

  while (!m2_kbhit())
    ;
  c = (unsigned char)pending;
  pending = -1;
  return c;
}

static void rule(uint8_t y, uint8_t left, uint8_t fill, uint8_t right)
{
  vid_put(0, y, left);
  vid_fill(1, y, VID_COLS - 2, fill);
  vid_put(VID_COLS - 1, y, right);
}

static void inset(uint8_t x, const char *s)
{
  static uint8_t n;

  n = (uint8_t)strlen(s);
  vid_put(x, 0, ' ');
  vid_text(x + 1, 0, s, n, 0, false);
  vid_put(x + 1 + n, 0, ' ');
}

/// @brief CONFIG's frame: title and user in the top border, a divider above the panel
void draw_frame(const char *title, const char *user, unsigned char rule_y)
{
  static uint8_t y;

  rule(0, G_CORNER_UL, G_HLINE, G_CORNER_UR);
  for (y = 1; y < VID_ROWS - 1; y++)
  {
    vid_put(0, y, G_VLINE);
    vid_put(VID_COLS - 1, y, G_VLINE);
  }
  rule(rule_y, G_TEE_RIGHT_THIN, G_HLINE_THIN, G_TEE_LEFT_THIN);
  rule(VID_ROWS - 1, G_CORNER_LL, G_HLINE, G_CORNER_LR);

  if (title)
    inset(2, title);
  if (user && *user)
    inset(VID_COLS - 4 - (uint8_t)strlen(user), user);
}

#endif /* __TRS80M2__ */
