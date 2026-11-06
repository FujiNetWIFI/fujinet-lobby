#ifdef __TRS80M2__

/* want is what the lobby draws, have what the screen shows. vid_flush()
   sends the difference through P&T's video driver, about half a second for
   a whole screen.

   P&T's control codes: ESC Y row+32 col+32 places the cursor, ^N/^O turn
   reverse on and off, ^Q/^T graphics. In graphics mode graphics character
   n is sent as 40h+n. */

#include <string.h>
#include "con.h"
#include "vid.h"

#define CTL_BELL      0x07
#define CTL_CLEAR_EOL 0x01
#define CTL_CLEAR     0x0C
#define CTL_REV_ON    0x0E
#define CTL_REV_OFF   0x0F
#define CTL_GFX_ON    0x11
#define CTL_GFX_OFF   0x14
#define CTL_CURS_OFF  0x19
#define CTL_CURS_ON   0x1A
#define CTL_ESC       0x1B

#define UNKNOWN 0xFF
#define STALE_FOR_ERASE 4

static uint8_t want[VID_COLS * VID_ROWS];
static uint8_t have[VID_COLS * VID_ROWS];
static uint8_t dirty[VID_ROWS];

// The driver's cursor and modes; UNKNOWN makes the next cell set it
static uint8_t cur_x, cur_y, cur_rev, cur_gfx;
static bool active;

static void modes_off(void)
{
  con_out(CTL_GFX_OFF);
  con_out(CTL_REV_OFF);
}

void vid_init(void)
{
  modes_off();
  con_out(CTL_CURS_OFF);
  con_out(CTL_CLEAR);

  memset(want, ' ', sizeof(want));
  memset(have, ' ', sizeof(have));
  memset(dirty, 0, sizeof(dirty));
  cur_x = cur_y = UNKNOWN;
  cur_rev = cur_gfx = 0;
  active = true;
}

void vid_exit(void)
{
  if (!active)
    return;
  active = false;

  modes_off();
  con_out(CTL_CLEAR);
  con_out(CTL_CURS_ON);
}

void vid_put(uint8_t x, uint8_t y, uint8_t c)
{
  if (x >= VID_COLS || y >= VID_ROWS)
    return;
  want[(uint16_t)y * VID_COLS + x] = c;
  dirty[y] = 1;
}

void vid_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t c)
{
  if (y >= VID_ROWS || x >= VID_COLS)
    return;
  if (w > VID_COLS - x)
    w = VID_COLS - x;
  memset(&want[(uint16_t)y * VID_COLS + x], c, w);
  dirty[y] = 1;
}

void vid_rev(uint8_t x, uint8_t y, uint8_t w, bool on)
{
  static uint8_t *p;

  if (y >= VID_ROWS || x >= VID_COLS)
    return;
  if (w > VID_COLS - x)
    w = VID_COLS - x;
  p = &want[(uint16_t)y * VID_COLS + x];
  dirty[y] = 1;
  while (w--)
  {
    *p = on ? (*p | REV) : (*p & 0x7F);
    p++;
  }
}

void vid_text(uint8_t x, uint8_t y, const char *s, uint8_t w, uint8_t attr, bool pad)
{
  static uint8_t *p;
  static uint8_t c;

  if (y >= VID_ROWS || x >= VID_COLS)
    return;
  if (w > VID_COLS - x)
    w = VID_COLS - x;

  p = &want[(uint16_t)y * VID_COLS + x];
  dirty[y] = 1;

  while (w && *s)
  {
    c = (uint8_t)*s++;
    if (c < 0x20 || c > 0x7E)
      c = '?';
    *p++ = c | attr;
    w--;
  }

  if (pad)
    while (w--)
      *p++ = ' ' | attr;
}

void vid_bell(void)
{
  con_out(CTL_BELL);
}

static uint8_t is_gfx(uint8_t code)
{
  return (code & 0x7F) < 0x20;
}

// Send one cell where the cursor is
static void send(uint8_t code)
{
  static uint8_t m;

  if ((code & REV) != cur_rev)
  {
    cur_rev = code & REV;
    con_out(cur_rev ? CTL_REV_ON : CTL_REV_OFF);
  }

  m = is_gfx(code);
  if (m != cur_gfx)
  {
    cur_gfx = m;
    con_out(m ? CTL_GFX_ON : CTL_GFX_OFF);
  }

  code &= 0x7F;
  con_out(m ? 0x40 + code : code);

  // Past the last column the driver wraps or stays put, as set up in P&T
  cur_x = (cur_x == VID_COLS - 1) ? UNKNOWN : cur_x + 1;
}

/* Move the cursor to x on line y. A short hop along the line resends the
   cells in between when that is no longer than ESC Y and keeps the modes. */
static void go(uint8_t x, uint8_t y, const uint8_t *row)
{
  static uint8_t i, c;

  if (cur_y == y && cur_x != UNKNOWN && cur_x < x && x - cur_x <= 3)
  {
    for (i = cur_x; i < x; i++)
    {
      c = row[i];
      if ((c & REV) != cur_rev || is_gfx(c) != cur_gfx)
        break;
    }
    if (i == x)
    {
      for (i = cur_x; i < x; i++)
        send(row[i]);
      return;
    }
  }

  if (cur_x != x || cur_y != y)
  {
    con_out(CTL_ESC);
    con_out('Y');
    con_out(32 + y);
    con_out(32 + x);
    cur_x = x;
    cur_y = y;
  }
}

static void flush_row(uint8_t y)
{
  static uint8_t *row, *hrow;
  static uint8_t x, e, last, stale, i, end;

  row = &want[(uint16_t)y * VID_COLS];
  hrow = &have[(uint16_t)y * VID_COLS];

  /* Printing the bottom right cell scrolls the screen in wrap mode, which
     only SETMISC knows about; it goes straight into video memory instead */
  end = (y == VID_ROWS - 1) ? VID_COLS - 1 : VID_COLS;
  if (end < VID_COLS && hrow[end] != row[end])
  {
    con_vram_put((uint16_t)y * VID_COLS + end, row[end]);
    hrow[end] = row[end];
  }

  /* The last column before the right edge that is not blank: past it, a
     line with enough old text left on it is cleared with one ^A. Not on the
     bottom line, where ^A would also erase the corner cell. */
  last = UNKNOWN;
  for (x = 0; x < VID_COLS - 1; x++)
    if (row[x] != ' ')
      last = x;

  for (x = 0; x < end; x++)
  {
    e = row[x];
    if (hrow[x] == e)
      continue;

    if (e == ' ' && (last == UNKNOWN || x > last) && end == VID_COLS)
    {
      stale = 0;
      for (i = x; i < VID_COLS - 1; i++)
        if (hrow[i] != ' ')
          stale++;
      if (stale >= STALE_FOR_ERASE)
      {
        go(x, y, row);
        if (cur_rev)
        {
          cur_rev = 0;
          con_out(CTL_REV_OFF);
        }
        con_out(CTL_CLEAR_EOL);
        memset(&hrow[x], ' ', VID_COLS - x);
        continue;
      }
    }

    go(x, y, row);
    send(e);
    hrow[x] = e;
  }

  dirty[y] = 0;
}

void vid_flush(void)
{
  static uint8_t y;

  if (!active)
    return;

  for (y = 0; y < VID_ROWS; y++)
    if (dirty[y])
      flush_row(y);
}

#endif /* __TRS80M2__ */
