#ifdef __MSX__

/**
 * @brief   32x24 GRAPHICS II console for the MSX, in FujiNet CONFIG's look
 *
 * The conio calls the shared client uses, drawn straight into the VDP: a cell
 * (x,y) has its 8 pattern bytes at y*256 + x*8 and its 8 colour bytes 0x2000
 * above that. The colours come from the role textcolor() set, revers(), and
 * where the cell is: inside CONFIG's card (see msx_card) text sits on its
 * striped blue band, outside it on black.
 */

#include <stdbool.h>
#include <string.h>
#include <video/tms99x8.h>

#include "msx/vars.h"

extern const unsigned char msx_font[96*8];  /* font.c */

#define COLS  32
#define ROWS  24

/* CONFIG's CH_BOX_UL ... CH_TAB_R (gfxutil.h) */
#define CH_BOX_UL   0x93
#define CH_BOX_U    0x94
#define CH_BOX_UR   0x95
#define CH_BOX_L    0x96
#define CH_BOX_R    0x97
#define CH_BOX_BL   0x98
#define CH_BOX_B    0x99
#define CH_BOX_BR   0x9A
#define CH_TAB_L    0x9B
#define CH_TAB_R    0x9C
#define CH_LAST     0xA7    /* the logo's arcs and lines, see msx_logo */

/* gfxutil.c's udg, 0x93-0xA7 */
static const unsigned char frame_glyphs[21*8] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x03,  /* Box UL */
  0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,  /* Box U  */
  0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xC0,  /* Box UR */
  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,  /* Box L  */
  0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,  /* Box R  */
  0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,  /* Box BL */
  0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,  /* Box B  */
  0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,  /* Box BR */
  0x0F,0x1F,0x1F,0x1F,0x1F,0x3F,0xFF,0xFF,  /* Tab L  */
  0x80,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,  /* Tab R  */
  0x07,0x18,0x20,0x40,0x40,0x80,0x80,0x80,  /* NW arc empty  */
  0xE0,0x18,0x04,0x02,0x02,0x01,0x01,0x01,  /* NE arc empty  */
  0x80,0x80,0x80,0x40,0x40,0x20,0x18,0x07,  /* SW arc empty  */
  0x01,0x01,0x01,0x02,0x02,0x04,0x18,0xE0,  /* SE arc empty  */
  0x07,0x1F,0x3F,0x7F,0x7F,0xFF,0xFF,0xFF,  /* NW arc filled */
  0xE0,0xF8,0xFC,0xFE,0xFE,0xFF,0xFF,0xFF,  /* NE arc filled */
  0xFF,0xFF,0xFF,0x7F,0x7F,0x3F,0x1F,0x07,  /* SW arc filled */
  0xFF,0xFF,0xFF,0xFE,0xFE,0xFC,0xF8,0xE0,  /* SE arc filled */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,  /* H line */
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,  /* V line */
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0xFF,  /* Corner */
};

const unsigned char msx_pill_left[8]  = {0x03,0x07,0x0F,0x0F,0x0F,0x0F,0x07,0x03};
const unsigned char msx_pill_right[8] = {0xC0,0xE0,0xF0,0xF0,0xF0,0xF0,0xE0,0xC0};

static const unsigned char blank_glyph[8] = {0,0,0,0,0,0,0,0};

/* The foreground nibble per role; the background comes from where the cell is.
   Inside the card: light blue/dark blue stripes. Outside: black. */
static const unsigned char role_fg[] = {
  0xF,  /* R_TEXT   white */
  0xF,  /* R_TITLE */
  0xF,  /* R_USER */
  0x7,  /* R_HEAD   cyan */
  0xF,  /* R_RULE */
  0xF,  /* R_NAME */
  0xF,  /* R_COUNT */
  0x7,  /* R_HINT   cyan */
  0xF,  /* R_STATUS */
  0x9,  /* R_ERROR  light red */
  0x7,  /* R_NOTE   cyan; light yellow (0xB) inside the card */
};

static unsigned char cx, cy, rev, role;
static unsigned char card_on;   /* the card is on screen */
static unsigned char col[8];    /* the cell's colour bytes */

unsigned char msx_revers(unsigned char onoff)
{
  unsigned char old = rev;
  rev = onoff;
  return old;
}

unsigned char msx_textcolor(unsigned char r)
{
  unsigned char old = role;
  role = r;
  return old;
}

void msx_gotoxy(unsigned char x, unsigned char y)
{
  cx = x;
  cy = y;
}

/* Fill col[] for the cell under the cursor */
static void pick_colour(void)
{
  unsigned char fg, i;

  if (card_on && cy > MSX_CARD_TOP && cy < MSX_CARD_BOTTOM && cx >= 1 && cx <= 30) {
    if (rev) {
      /* CONFIG's selection bar: white on dark red */
      memset(col, 0xF6, 8);
      return;
    }
    if (cy == MSX_HEAD_ROW) {
      memset(col, 0xF1, 8);   /* the headings: white on solid black */
      return;
    }
    fg = role_fg[role];
    if (role == R_NOTE)
      fg = 0xB;
    fg <<= 4;
    for (i = 0; i < 8; i += 2) {
      col[i] = fg | 5;      /* the band's light and dark scanlines */
      col[i+1] = fg | 4;
    }
    return;
  }

  if (rev)
    fg = 0x1F;              /* key names: black on white */
  else
    fg = (role_fg[role] << 4) | 1;
  memset(col, fg, 8);
}

void msx_cputc(unsigned char c)
{
  const unsigned char *g;
  unsigned int addr;

  if (c == '\r') {
    cx = 0;
    return;
  }
  if (c == '\n') {
    cy++;
    return;
  }

  if (cx >= COLS) {
    cx = 0;
    cy++;
  }

  if (cy < ROWS) {
    if (c >= 0x20 && c <= 0x7F)
      g = msx_font + ((unsigned int)(c - 0x20) << 3);
    else if (c >= CH_BOX_UL && c <= CH_LAST)
      g = frame_glyphs + ((unsigned int)(c - CH_BOX_UL) << 3);
    else
      g = blank_glyph;

    pick_colour();
    addr = ((unsigned int)cy << 8) + ((unsigned int)cx << 3);
    vdp_vwrite((void *)g, addr, 8);
    vdp_vwrite(col, MODE2_ATTR + addr, 8);
  }
  cx++;
}

void msx_cputs(const char *s)
{
  while (*s)
    msx_cputc(*s++);
}

void msx_cclear(unsigned char n)
{
  while (n--)
    msx_cputc(' ');
}

void msx_clrscr(void)
{
  vdp_vfill(0, 0, 0x1800);
  vdp_vfill(MODE2_ATTR, 0xF1, 0x1800);
  msx_bar(0);
  card_on = 0;
  cx = cy = 0;
}

/* One interior row's colour bytes: the band's light and dark scanlines */
static unsigned char stripes[30*8];

void msx_card(const char *title)
{
  unsigned char i, t;
  unsigned char old_rev = rev, old_role = role;

  /* z88dk's naming is inverted: noblank() turns the display off */
  vdp_noblank();
  rev = 0;
  role = R_TEXT;
  card_on = 1;

  t = strlen(title);

  /* Top edge: corner, run, tab with the title on white, tab */
  gotoxy(0, MSX_CARD_TOP);
  cputc(CH_BOX_UL);
  for (i = 0; i < COLS - t - 3; i++)
    cputc(CH_BOX_U);
  cputc(CH_TAB_L);
  rev = 1;
  cputs(title);
  rev = 0;
  cputc(CH_TAB_R);

  /* Sides */
  for (i = MSX_CARD_TOP + 1; i < MSX_CARD_BOTTOM; i++) {
    gotoxy(0, i);
    cputc(CH_BOX_L);
    gotoxy(31, i);
    cputc(CH_BOX_R);
  }

  /* Bottom edge */
  gotoxy(0, MSX_CARD_BOTTOM);
  cputc(CH_BOX_BL);
  for (i = 0; i < 30; i++)
    cputc(CH_BOX_B);
  cputc(CH_BOX_BR);

  /* The blue band behind the interior, so empty cells show it */
  for (i = 0; i < sizeof(stripes); i += 2) {
    stripes[i] = 0xF5;
    stripes[i+1] = 0xF4;
  }
  for (i = MSX_CARD_TOP + 1; i < MSX_CARD_BOTTOM; i++)
    vdp_vwrite(stripes, MODE2_ATTR + ((unsigned int)i << 8) + 8, sizeof(stripes));
  /* ...except the headings' row, which is solid black */
  vdp_vfill(MODE2_ATTR + ((unsigned int)MSX_HEAD_ROW << 8) + 8, 0xF1, sizeof(stripes));

  rev = old_rev;
  role = old_role;
  vdp_blank();
}

/* CONFIG's logo: each row's column offset and glyphs (screen.c) */
static const unsigned char logo_x[8] = { 3, 1, 1, 0, 1, 1, 1, 3 };
static const char * const logo_rows[8] = {
  "\xA6",
  "\xA7\xA5\xA1\xA2\xA7\xA5",
  "\xA6 \xA3\xA4\xA6",
  "\xA5\xA1\xA2\x9D\x9E\xA1\xA2\xA5",
  "\xA3\xA4\x9F\xA0\xA3\xA4",
  "\xA7\xA5\xA7\xA5\xA1\xA2\xA5",
  "\xA6 \xA6 \xA3\xA4",
  "\xA6 \xA6",
};

void msx_logo(void)
{
  unsigned char i;

  for (i = 0; i < 8; i++) {
    gotoxy(12 + logo_x[i], 9 + i);
    cputs(logo_rows[i]);
  }
}

void msx_splash(const char *msg)
{
  rev = 0;
  role = R_TEXT;
  clrscr();
  msx_logo();
  if (msg && msg[0]) {
    /* panel_status(1, ...)'s row, so "Retrieving Games.." replaces it */
    gotoxy((COLS - strlen(msg)) / 2, 21);
    cputs(msg);
  }
}

void msx_bar(unsigned char row)
{
  /* vdp_put_sprite_8() takes off the line the VDP adds itself, as CONFIG's
     bar_update() relies on; 0xD1 lands as 0xD0, which ends the sprite list */
  unsigned char y = row ? row << 3 : 0xD1;

  vdp_put_sprite_8(0, 1 << 3, y, 0, VDP_INK_DARK_RED);
  vdp_put_sprite_8(1, 30 << 3, y, 1, VDP_INK_DARK_RED);
}

#define FNKSTR  ((unsigned char *)0xF87F)   /* BIOS function key strings */

static void menu_item(unsigned char *x, unsigned char fkey, unsigned char key, const char *label)
{
  unsigned char len = 0, hot = 0, c;

  FNKSTR[fkey << 4] = key;
  FNKSTR[(fkey << 4) + 1] = 0;
  if (!key) {
    *x = 1 + (fkey + 1) * 6;    /* a blank item is still a tab stop */
    return;
  }

  gotoxy(*x, 23);
  role = R_HINT;                /* cyan */
  while ((c = *label++) != 0) {
    if (c == '_') {
      hot = 1;
      continue;
    }
    if (hot) {
      role = R_TEXT;            /* the key itself in white */
      cputc(c);
      role = R_HINT;
      hot = 0;
    } else
      cputc(c);
    len++;
  }
  *x += len > 5 ? len + 1 : 6;
}

void msx_status(bool in_game, bool any)
{
  const char *msg;

  if (any) {
    msg = in_game ? "Select a table to play" : "Select a game";
    gotoxy((COLS - strlen(msg)) / 2, 21);
    cputs(msg);
  }
}

void msx_footer(bool in_game, bool any)
{
  unsigned char x = 1;
  unsigned char old_rev = rev, old_role = role;

  rev = 0;
  role = R_TEXT;

  msx_status(in_game, any);

  vdp_vfill(23 << 8, 0, 256);
  vdp_vfill(MODE2_ATTR + (23 << 8), 0xF1, 256);

  menu_item(&x, 0, any ? (in_game ? 'p' : 'v') : 0, in_game ? "_play" : "_view");
  menu_item(&x, 1, 'r', "_refresh");
  menu_item(&x, 2, 'q', "_qa");
  menu_item(&x, 3, 'n', "_name");
  menu_item(&x, 4, in_game ? 'b' : 'x', in_game ? "_back" : "e_xit");

  rev = old_rev;
  role = old_role;
}

/* The status rows, 20-22; the menu under them stays */
static void clear_panel(void)
{
  vdp_vfill(20 << 8, 0, 3 << 8);
  vdp_vfill(MODE2_ATTR + (20 << 8), 0xF1, 3 << 8);
}

bool msx_confirm(const char *msg)
{
  unsigned char c, old_rev = rev, old_role = role;

  clear_panel();
  rev = 0;
  role = R_TEXT;
  gotoxy((COLS - strlen(msg)) / 2, 21);
  cputs(msg);

  /* Y or N, either case; ESC cancels too */
  do
    c = cgetc() | 0x20;
  while (c != 'y' && c != 'n' && c != (0x1B | 0x20));

  clear_panel();
  rev = old_rev;
  role = old_role;
  return c == 'y';
}

#endif /* __MSX__ */
