#ifndef A78_H
#define A78_H

#include <stdint.h>

/* The lobby's 40 x 24 text screen on the Atari 7800 (src/atari7800).
 *
 * MARIA draws it in 320D: one bit a pixel, eight a byte, from CONFIG's font
 * read through the character map (one byte a character, its ASCII code).
 * Each row is an 8-line zone whose display list has one header per run of
 * cells sharing a palette value P (0-7). In 320D a pixel's colour is its
 * bit plus P1 (even pixels) or P0 (odd ones), from palette 0 or, with P2,
 * palette 4; with MARIA's kangaroo mode on, the zeros are drawn too. So:
 *
 *   P = 0 / 4   C2 of palette 0 / 4 on the background (plain text)
 *   P = 3 / 7   C3 on a C1 bar as wide as the run (a highlight)
 *
 * Palettes 0 and 4 are all 320D shows, so display-list interrupts load
 * other colours into them for four bands of rows (maria.s): rows 0-1,
 * 2-3, 4-20 and 21-23, six colours each.
 *
 * cc65's own 7800 conio needs its crt0's zone counter, so vars.h renames
 * the conio calls to these and none of its modules link. */

/* maria.s */
void a78_init(void);
void a78_flush(void);           /* rebuild the rows whose runs changed */
void a78_vsync(void);           /* wait for a vertical blank, then flush */
void __fastcall__ a78_background(uint8_t c);
extern uint8_t a78_pal;         /* 1 on a PAL console */
extern uint8_t a78_map[];       /* the characters; a row is a78_rowhi/lo */
extern uint8_t a78_dirty[];
extern const uint8_t a78_rowlo[], a78_rowhi[];
#define A78_ATTR 0x400          /* a cell's palette value, from its map byte */

/* conio.c */
void a78_clrscr(void);
void a78_gotoxy(uint8_t x, uint8_t y);
uint8_t a78_wherex(void);
uint8_t a78_wherey(void);
void a78_cputc(char c);
void a78_cputs(const char *s);
void a78_cclear(uint8_t n);
uint8_t a78_revers(uint8_t on);
void a78_screensize(uint8_t *x, uint8_t *y);
uint8_t a78_textcolor(uint8_t role);
uint8_t a78_bgcolor(uint8_t color);
uint8_t a78_bordercolor(uint8_t color);

#endif /* A78_H */
