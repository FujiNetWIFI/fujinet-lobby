#ifndef A52_H
#define A52_H

#include <stdint.h>

/* The lobby's own 40x24 text screen (src/atari5200/conio.c). cc65's 5200
 * conio is 20 columns of uppercase with no reverse video, so vars.h renames
 * the conio calls to these and none of its modules link. */
void a52_screen_init(void);
void a52_clrscr(void);
void a52_gotoxy(uint8_t x, uint8_t y);
uint8_t a52_wherex(void);
uint8_t a52_wherey(void);
void a52_cputc(char c);
void a52_cputs(const char *s);
void a52_cclear(uint8_t n);
uint8_t a52_revers(uint8_t on);
void a52_screensize(uint8_t *x, uint8_t *y);
uint8_t a52_textcolor(uint8_t color);
uint8_t a52_bgcolor(uint8_t color);
uint8_t a52_bordercolor(uint8_t color);

#endif /* A52_H */
