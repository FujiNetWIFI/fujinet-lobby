#ifndef MSDOS_CONIO_H
#define MSDOS_CONIO_H

/* -----------------------------------------------------------------------
 * conio.h  --  MS-DOS console I/O for the FujiNet Stocks application.
 *
 * Open Watcom's system <conio.h> only provides getch()/kbhit()/etc.
 * It does NOT provide gotoxy(), clrscr(), cgetc(), or revers().
 * This header (picked up first via -Iinclude/msdos) adds those missing
 * prototypes.  Implementations are in src/msdos/conio.c.
 * ----------------------------------------------------------------------- */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VIDEO_MODE_40COL,
    VIDEO_MODE_80COL
} VideoMode;

void set_video_mode(VideoMode mode);
void set_screen_bg_blue(void);
void restore_screen_bg(void);
int kbhit(void);
void gotoxy(int x, int y);
void clrscr(void);
char cgetc(void);
void revers(uint8_t on);
void cursor(bool on);
int cputs(const char *s);
void cputc(char c);
void cclear(int n);
void screensize(uint8_t *w, uint8_t *h);
void draw_box(int x, int y, int w, int h);
#define cputcxy(x, y, c) do { gotoxy((x), (y)); cputc(c); } while(0)

#include <vars.h>

#endif /* MSDOS_CONIO_H */
