#ifdef __ATARI7800__

#ifndef VARS_H
#define VARS_H

#include "atari7800/a78.h"

#undef clrscr
#define clrscr  a78_clrscr
#undef gotoxy
#define gotoxy  a78_gotoxy
#undef wherex
#define wherex  a78_wherex
#undef wherey
#define wherey  a78_wherey
#undef cputc
#define cputc  a78_cputc
#undef cputs
#define cputs  a78_cputs
#undef cclear
#define cclear  a78_cclear
#undef revers
#define revers  a78_revers
#undef screensize
#define screensize  a78_screensize
#undef textcolor
#define textcolor  a78_textcolor
#undef bgcolor
#define bgcolor  a78_bgcolor
#undef bordercolor
#define bordercolor  a78_bordercolor

/**
 * What a piece of text is: textcolor() takes one of these, and the band the
 * row is in (src/atari7800/maria.s) gives it its colour. revers(1) draws it
 * on a bar instead (src/atari7800/conio.c).
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
 * Keys cgetc() returns, from the console switches and the right button
 * (platform.c)
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

#endif /* __ATARI7800__ */
