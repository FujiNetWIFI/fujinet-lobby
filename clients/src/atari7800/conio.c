#ifdef __ATARI7800__

/**
 * @brief   40x24 colour text screen for the Atari 7800 (MARIA 320D)
 *
 * The conio calls the shared client uses, on maria.s's screen. A cell's
 * character goes straight into the map, which MARIA reads every line; its
 * palette value comes from the role textcolor() set and revers(), and a
 * change to it, or text where a row had only plain spaces, marks the row.
 * Marked rows get new display lists at the end of each call that writes,
 * when the cursor leaves the row, and in waitvsync(), so text is in its
 * colours before the client blocks on the network.
 */

#include <stdint.h>

#include "atari7800/vars.h"

#define COLS     40
#define ROWS     24

/* Each role's palette value, plain and reversed: 0 and 4 are text on the
 * background, 3 and 7 text on a bar. The band a row is in picks the colours
 * (maria.s bandrom). */
static const uint8_t style[][2] = {
  { 0, 3 },     /* R_TEXT */
  { 3, 3 },     /* R_TITLE: white on the title bar */
  { 7, 7 },     /* R_USER: gold on the title bar */
  { 0, 3 },     /* R_HEAD: cyan */
  { 4, 4 },     /* R_RULE: green */
  { 0, 3 },     /* R_NAME: white; selected, white on blue */
  { 4, 7 },     /* R_COUNT: green; selected, light green on blue */
  { 0, 3 },     /* R_HINT: grey; a key, black on gold */
  { 4, 7 },     /* R_STATUS: yellow */
  { 7, 7 },     /* R_ERROR: white on red */
  { 4, 7 },     /* R_NOTE: green */
};

static uint8_t cx, cy, rev, role;
static uint8_t *row;

static void at_row(uint8_t y)
{
  cy = y;
  row = (uint8_t *)(((uint16_t)a78_rowhi[y] << 8) | a78_rowlo[y]);
}

void a78_clrscr(void)
{
  uint8_t y, x;

  for (y = 0; y < ROWS; y++) {
    at_row(y);
    for (x = 0; x < COLS; x++) {
      row[x] = ' ';
      row[x + A78_ATTR] = 0;
    }
    a78_dirty[y] = 1;
  }
  a78_flush();
  cx = 0;
  at_row(0);
}

void a78_gotoxy(uint8_t x, uint8_t y)
{
  if (y >= ROWS)
    y = ROWS - 1;
  if (y != cy)
    a78_flush();
  cx = x < COLS ? x : COLS - 1;
  at_row(y);
}

uint8_t a78_wherex(void) { return cx; }
uint8_t a78_wherey(void) { return cy; }

void a78_cputc(char c)
{
  uint8_t a, *p;

  if (c == '\r') {
    cx = 0;
    return;
  }
  if (c == '\n') {
    if (cy < ROWS - 1)
      a78_gotoxy(cx, cy + 1);
    return;
  }
  if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7E)
    c = ' ';

  p = row + cx;
  a = style[role][rev];
  if (p[A78_ATTR] != a || ((*p == ' ') != (c == ' '))) {
    p[A78_ATTR] = a;
    a78_dirty[cy] = 1;
  }
  *p = c;
  if (++cx >= COLS) {
    cx = 0;
    if (cy < ROWS - 1)
      a78_gotoxy(0, cy + 1);
  }
}

void a78_cputs(const char *s)
{
  while (*s)
    a78_cputc(*s++);
  a78_flush();
}

void a78_cclear(uint8_t n)
{
  while (n--)
    a78_cputc(' ');
  a78_flush();
}

uint8_t a78_revers(uint8_t on)
{
  uint8_t old = rev;

  rev = on ? 1 : 0;
  return old;
}

void a78_screensize(uint8_t *x, uint8_t *y)
{
  *x = COLS;
  *y = ROWS;
}

uint8_t a78_textcolor(uint8_t r)
{
  uint8_t old = role;

  if (r < sizeof(style) / sizeof(style[0]))
    role = r;
  return old;
}

uint8_t a78_bgcolor(uint8_t color)
{
  a78_background(color);
  return 0;
}

uint8_t a78_bordercolor(uint8_t color)
{
  (void)color;                  /* MARIA's border is the background */
  return 0;
}

#endif /* __ATARI7800__ */
