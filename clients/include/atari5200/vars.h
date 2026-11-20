#ifdef __ATARI5200__

#ifndef VARS_H
#define VARS_H

#include "atari5200/a52.h"

#undef clrscr
#define clrscr  a52_clrscr
#undef gotoxy
#define gotoxy  a52_gotoxy
#undef wherex
#define wherex  a52_wherex
#undef wherey
#define wherey  a52_wherey
#undef cputc
#define cputc  a52_cputc
#undef cputs
#define cputs  a52_cputs
#undef cclear
#define cclear  a52_cclear
#undef revers
#define revers  a52_revers
#undef screensize
#define screensize  a52_screensize
#undef textcolor
#define textcolor  a52_textcolor
#undef bgcolor
#define bgcolor  a52_bgcolor
#undef bordercolor
#define bordercolor  a52_bordercolor

/**
 * Keys cgetc() returns, from the keypad and the lower fire button (platform.c)
 */
#define KEY_LEFT_ARROW      0x1C
#define KEY_LEFT_ARROW_2    0x11
#define KEY_LEFT_ARROW_3    0x12

#define KEY_RIGHT_ARROW     0x1D
#define KEY_RIGHT_ARROW_2   0x13
#define KEY_RIGHT_ARROW_3   0x14

#define KEY_UP_ARROW        0x1E
#define KEY_UP_ARROW_2      0x15
#define KEY_UP_ARROW_3      0x16

#define KEY_DOWN_ARROW      0x1F
#define KEY_DOWN_ARROW_2    0x17
#define KEY_DOWN_ARROW_3    0x18

#define CH_ESC           0x1B
#define KEY_ESCAPE       CH_ESC
#define KEY_ESCAPE_ALT   0x01

#define KEY_SPACEBAR     0x20
#define KEY_BACKSPACE    0x7F

#undef KEY_RETURN
#define KEY_RETURN       0x0D

#define CHAR_CURSOR      '_'

#endif /* VARS_H */

#endif /* __ATARI5200__ */
