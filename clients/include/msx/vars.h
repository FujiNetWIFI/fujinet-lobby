#ifdef __MSX__

#ifndef VARS_H
#define VARS_H

#include <stdbool.h>

/**
 * What a piece of text is: textcolor() takes one of these, and where it lands
 * (inside CONFIG's card or outside it) gives it its colour. revers(1) draws it
 * on the selection bar instead (src/msx/conio.c).
 */
#define R_TEXT    0     /* anything else */
#define R_TITLE   1     /* the title bar */
#define R_USER    2     /* the name on the title bar */
#define R_HEAD    3     /* a list's headings */
#define R_RULE    4     /* the dashed rules */
#define R_NAME    5     /* a game or table */
#define R_COUNT   6     /* its players */
#define R_HINT    7     /* the footer; reversed, a key */
#define R_STATUS  8     /* what the lobby is doing */
#define R_ERROR   9     /* what went wrong */
#define R_NOTE    10    /* the QA notice, the name prompt and its help */

/**
 * Keys cgetc() returns: MSX BIOS codes. The _2 and _3 alternates are
 * punctuation so the arrows still work from a stick-less keyboard, and none of
 * them is a character a user name can hold.
 */
#define KEY_LEFT_ARROW      0x1D
#define KEY_LEFT_ARROW_2    0x2C // ,
#define KEY_LEFT_ARROW_3    0x3C // <

#define KEY_RIGHT_ARROW     0x1C
#define KEY_RIGHT_ARROW_2   0x2E // .
#define KEY_RIGHT_ARROW_3   0x3E // >

#define KEY_UP_ARROW        0x1E
#define KEY_UP_ARROW_2      0x2D // -
#define KEY_UP_ARROW_3      0x5F // _

#define KEY_DOWN_ARROW      0x1F
#define KEY_DOWN_ARROW_2    0x3D // =
#define KEY_DOWN_ARROW_3    0x2B // +

#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x1B
#define KEY_BACKSPACE    0x08

#define KEY_SPACEBAR     0x20

#define CHAR_CURSOR      '_'

/**
 * conio compatibility.
 *
 * The screen is GRAPHICS II drawn by src/msx/conio.c, not z88dk's console, so
 * its names are redirected. This header is reached from platform.h, which
 * main.c and io.c include *after* <conio.h>, so these land on every call site.
 */
#undef gotoxy
#define gotoxy(x, y)  msx_gotoxy(x, y)
#undef cputc
#define cputc(c)      msx_cputc(c)
#undef cputs
#define cputs(s)      msx_cputs(s)
#undef cclear
#define cclear(n)     msx_cclear(n)
#undef clrscr
#define clrscr()      msx_clrscr()
#undef revers
#define revers(o)     msx_revers(o)
#undef textcolor
#define textcolor(r)  msx_textcolor(r)
#undef cgetc
#define cgetc()       msx_cgetc()
#undef kbhit
#define kbhit()       msx_kbhit()

/* Row 23 is CONFIG's function key menu, so it is not reported: the shared
   client's bottom panel then lands on rows 20-22. */
#undef screensize
#define screensize(px, py) do { *(px) = 32; *(py) = 23; } while (0)

void msx_gotoxy(unsigned char x, unsigned char y);
void msx_cputc(unsigned char c);
void msx_cputs(const char *s);
void msx_cclear(unsigned char n);
void msx_clrscr(void);
unsigned char msx_revers(unsigned char onoff);
unsigned char msx_textcolor(unsigned char role);
unsigned char msx_cgetc(void);
unsigned char msx_kbhit(void);

/* CONFIG's pill caps, for the selection bar's sprites (platform.c) */
extern const unsigned char msx_pill_left[8];
extern const unsigned char msx_pill_right[8];

/* The card's rows: the title tab on the top edge, the list's headings on the
   first interior row */
/* The cartridge's mode register: ROM_MODE_CMD (04h) | UNAPI (10h) | expanded
   (20h), with neither user ROM (01h, 40h) nor CONFIG's disable bit (08h), so
   subslot 0 shows CONFIG. fujinet-lib only names the user ROM and BASIC modes. */
#define MSX_CART_MODE_CONFIG  0x34

#define MSX_CARD_TOP     1
#define MSX_CARD_BOTTOM  19
#define MSX_HEAD_ROW     2

/** @brief CONFIG's card on rows 1-19, with a title in its tab */
void msx_card(const char *title);

/** @brief CONFIG's logo, white on black, at (12,9) */
void msx_logo(void);

/** @brief Clear to the logo, with msg (if any) centred on row 21 */
void msx_splash(const char *msg);

/** @brief The pill caps either side of screen row `row`; 0 hides them */
void msx_bar(unsigned char row);

/** @brief The status line (row 21) and CONFIG's function key menu (row 23) */
void msx_footer(bool in_game, bool any);

/** @brief The footer's status line, without its menu */
void msx_status(bool in_game, bool any);

/** @brief Ask msg in place of the status line; true for Y. Leaves the line
    clear for msx_status() to redraw. */
bool msx_confirm(const char *msg);

/** @brief Map the menu's letters onto the keys the shared loop understands */
void msx_menu_key(void);

#endif /* VARS_H */

#endif /* __MSX__ */
