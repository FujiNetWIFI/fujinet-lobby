#ifndef CONIO_H
#define CONIO_H

#include <hirestxt.h>

#define SCREEN_COLS 42
#define SCREEN_ROWS 24

unsigned char kbhit (void);
char cgetc (void);
void gotoxy (unsigned char x, unsigned char y);
void  cputc (char c);
void  cputs (const char* s);

/* Enable/disable reverse character display. This may not be supported by
** the output device. Return the old setting.
*/
unsigned char revers (unsigned char onoff);

/* Clear part of a line (write length spaces). */
void  cclear (unsigned char length);

/* Return the current screen size. */
void  screensize (unsigned char* x, unsigned char* y);

/* Set up / tear down the 42x24 hi-res text screen. */
void hirestxt_init (void);
void hirestxt_close (void);

/* Cycle black/green -> green/black -> black/white -> white/black. */
void cycle_colorset (void);

/* Full-screen border, with title and user inset into the top edge and a
** horizontal rule across rule_y.
*/
void draw_frame (const char* title, const char* user, unsigned char rule_y);

#endif
