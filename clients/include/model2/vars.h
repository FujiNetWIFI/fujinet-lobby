#ifdef __TRS80M2__

#ifndef VARS_H
#define VARS_H

// P&T CP/M key codes; F1 and F2 are spare up and down keys
#define KEY_LEFT_ARROW      0x1C
#define KEY_LEFT_ARROW_2    ','
#define KEY_LEFT_ARROW_3    '<'

#define KEY_RIGHT_ARROW     0x1D
#define KEY_RIGHT_ARROW_2   '.'
#define KEY_RIGHT_ARROW_3   '>'

#define KEY_UP_ARROW        0x1E
#define KEY_UP_ARROW_2      '-'
#define KEY_UP_ARROW_3      0x01

#define KEY_DOWN_ARROW      0x1F
#define KEY_DOWN_ARROW_2    '='
#define KEY_DOWN_ARROW_3    0x02

#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x1B
#define KEY_SPACEBAR     0x20
#define KEY_BACKSPACE    0x08

#define CHAR_CURSOR      '_'

/* conio onto vid.c's buffer, with keys from the raw BIOS console so BREAK
   can't end the program. Must follow <conio.h>. */
#undef gotoxy
#undef cputc
#undef cgetc
#undef clrscr
#undef screensize
#define gotoxy(x,y)        m2_gotoxy(x,y)
#define cputs(s)           m2_cputs(s)
#define cputc(c)           m2_cputc(c)
#define clrscr()           m2_clrscr()
#define cgetc()            m2_cgetc()
#define kbhit()            m2_kbhit()
// The bottom row is the frame's
#define screensize(px,py)  do { *(px) = 80; *(py) = 23; } while (0)
#define setBoldMode(on)

void m2_gotoxy(unsigned char x, unsigned char y);
void m2_cputs(const char *s);
void m2_cputc(char c);
void m2_clrscr(void);
unsigned char m2_cgetc(void);
unsigned char m2_kbhit(void);
unsigned char revers(unsigned char onoff);
void cclear(unsigned char length);
void draw_frame(const char *title, const char *user, unsigned char rule_y);

void quit(void);

#endif /* VARS_H */

#endif /* __TRS80M2__ */
