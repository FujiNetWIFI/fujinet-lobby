#ifdef __ADAM__

#ifndef VARS_H
#define VARS_H

/**
 * Platform specific key map for common input.
 *
 * Keys arrive over AdamNet from the EOS keyboard driver, so these are EOS
 * codes rather than ASCII for anything above 0x7F.
 */

#define KEY_LEFT_ARROW      0xA3
#define KEY_LEFT_ARROW_2    0x9D
#define KEY_LEFT_ARROW_3    0x2C // ,

#define KEY_RIGHT_ARROW     0xA1
#define KEY_RIGHT_ARROW_2   0x1D
#define KEY_RIGHT_ARROW_3   0x2E // .

#define KEY_UP_ARROW        0xA0
#define KEY_UP_ARROW_2      0x91
#define KEY_UP_ARROW_3      0x2D // -

#define KEY_DOWN_ARROW      0xA2
#define KEY_DOWN_ARROW_2    0x11
#define KEY_DOWN_ARROW_3    0x3D // =

/* These three are also defined, identically, by <smartkeys.h>. Identical
   object-like redefinition is legal, so both headers can be included. */
#define KEY_RETURN       0x0D
#define KEY_ESCAPE       0x1B
#define KEY_BACKSPACE    0x08

#define KEY_SPACEBAR     0x20

/* Underscore in the generic z88dk console font. */
#define CHAR_CURSOR      0x5F

/**
 * Screen geometry.
 *
 * smartkeys_set_mode() puts the VDP in GRAPHICS II, which the z88dk console
 * driver drives as a 32x24 character grid. smartkeys_status() draws at pixel
 * y=168, so the SmartKeys strip owns rows 21-23 and the lobby owns rows 0-20.
 * Reporting the full 24 rows puts BOTTOM_PANEL_Y on 21, which is exactly where
 * the strip starts -- see the panel_* helpers in main.c.
 */
#define ADAM_SCREEN_W  32
#define ADAM_SCREEN_H  24

/**
 * conio compatibility.
 *
 * The shared client is written against cc65's conio. z88dk's is close but not
 * identical, so the gaps are patched here. This header is reached from
 * platform.h, which main.c and io.c both include *after* <conio.h>, so these
 * macros land on every call site without the shared sources having to know.
 */

/* z88dk's screensize() takes unsigned int*; callers pass uint8_t*. */
#undef screensize
#define screensize(px, py) do { *(px) = ADAM_SCREEN_W; *(py) = ADAM_SCREEN_H; } while (0)

/* z88dk maps cgetc() onto getch()/getk(), a dead stub on this target, and its
   kbhit() consumes the key. Both are reimplemented over the asynchronous EOS
   keyboard read in conio.c. Redirected rather than redefined so there is no
   clash with the prototypes and symbols already in the z88dk library.
   Unsigned because sccz80 chars are signed and the Adam's arrow and SmartKey
   codes are all >= 0x80. */
#undef cgetc
#define cgetc() adam_cgetc()
#undef kbhit
#define kbhit() adam_kbhit()

unsigned char adam_cgetc(void);
unsigned char adam_kbhit(void);

/* Absent from z88dk's conio entirely. */
unsigned char revers(unsigned char onoff);
void cclear(unsigned char length);

/* Sets the attribute pair revers(0) returns to, so the banner can print
   white-on-blue while the list prints black-on-white. */
void adam_set_normal(unsigned char fg, unsigned char bg);

#endif /* VARS_H */

#endif /* __ADAM__ */
