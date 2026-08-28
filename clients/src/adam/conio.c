#ifdef __ADAM__

/**
 * @brief   cc65 conio compatibility for the Coleco Adam
 *
 * The shared lobby client is written against cc65's conio. z88dk's is close
 * but not identical: revers() and cclear() do not exist at all, and the
 * keyboard entry points do not work on this target -- getk(), which z88dk's
 * cgetc() maps onto, is a dead stub on the Adam, because keys arrive over
 * AdamNet from the EOS keyboard driver rather than from a scanned matrix.
 *
 * Colour: the console runs in GRAPHICS II, where every character cell carries
 * its own foreground/background pair, and textcolor()/textbackground() are
 * applied per cell as it is printed. z88dk maps the conio colour enum onto the
 * TMS9918 palette (BLACK -> 1, BLUE -> 4, LIGHTGREEN -> 3, WHITE -> 15), which
 * lands exactly on the attribute bytes FujiNet CONFIG uses: 0x1F for content
 * rows, 0x13 for the selection bar, 0xF4 for a title row.
 */

#include <conio.h>
#include <eos.h>

#include "vars.h"

/* The pair revers(0) returns to. Content rows are black on white, as in
   fujinet-config's screen.c; banner() flips this to white on blue for the
   title row and back. */
static unsigned char normal_fg = BLACK;
static unsigned char normal_bg = WHITE;

static unsigned char reversed = 0;

static unsigned char pending_key = 0;
static unsigned char kbd_started = 0;

static void apply_colour(void)
{
  if (reversed)
    {
      /* CONFIG's ATTR_BAR: black on light green. */
      textcolor(BLACK);
      textbackground(LIGHTGREEN);
    }
  else
    {
      textcolor(normal_fg);
      textbackground(normal_bg);
    }
}

void adam_set_normal(unsigned char fg, unsigned char bg)
{
  normal_fg = fg;
  normal_bg = bg;
  apply_colour();
}

/* Enable/disable reverse character display. */
unsigned char revers(unsigned char onoff)
{
  unsigned char prev = reversed;

  reversed = onoff;
  apply_colour();

  return prev;
}

/* Clear part of a line (write length spaces). */
void cclear(unsigned char length)
{
  while (length--)
    cputc(' ');
}

/**
 * @brief Has a key arrived?
 *
 * EOS reads the keyboard asynchronously: eos_start_read_keyboard() arms a
 * read, and eos_end_read_keyboard() returns the key once it is > 1. The key is
 * held in pending_key so that a kbhit() which reports true is not immediately
 * followed by a blocking read that misses it.
 */
unsigned char adam_kbhit(void)
{
  unsigned char k;

  if (!kbd_started)
    {
      eos_start_read_keyboard();
      kbd_started = 1;
    }

  if (pending_key)
    return 1;

  k = eos_end_read_keyboard();
  if (k > 1)
    {
      pending_key = k;
      eos_start_read_keyboard();
      return 1;
    }

  return 0;
}

/// @brief Block until a key is pressed, and return it.
unsigned char adam_cgetc(void)
{
  unsigned char k;

  while (!adam_kbhit())
    ;

  k = pending_key;
  pending_key = 0;

  return k;
}

#endif /* __ADAM__ */
