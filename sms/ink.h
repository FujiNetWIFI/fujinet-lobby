/* ink.h -- coloured text for the room list.
 *
 * The SMS CONFIG's font (fujidisp.c, synced) is one 1bpp copy at tiles
 * 32-127 in palette colour 1, white, and every cell it draws carries attr 0.
 * Mode 4 has no per-cell colour beyond the palette-select bit, which CONFIG
 * spends on its invisible "selected" marker, so a second colour needs a
 * second copy of the font. ink_init() makes two, where the harness already
 * expects them (fn-sms pico/sms/emu/smstext.lua reads tile & 0x7F):
 *
 *   INK_WHITE  tiles  32-127   name byte c,       attr 0     CONFIG's own
 *   INK_GAME   tiles 160-255   name byte c + 128, attr 0     PAL_GAME
 *   INK_DIM    tiles 288-383   name byte c,       attr 0x01  PAL_DIM
 *
 * All three sit below the name table at $3800 (tile 448), and a fourth full
 * copy would not. The splash's logo glyphs at 128-223 are gone afterwards,
 * which is fine: they are only drawn before main()'s disp_init().
 *
 * The cells go through disp_tile(), so the name-table shadow keeps them and
 * disp_row_attr() (the selection marker), disp_row_blank() and a pop-up's
 * close all treat them like any other text.
 */

#ifndef INK_H
#define INK_H

/* An ink is the name-table bits that pick its copy: 0x80 lands in the tile
 * byte (c + 128), 0x01 in the attribute byte (tile bit 8). */
#define INK_WHITE 0x00
#define INK_GAME  0x80
#define INK_DIM   0x01

/* After every disp_init(): load the two copies, set their colours. */
void ink_init(void);

/* Text in an ink, at most `n` characters and never past TEXT_R. Anything
 * outside $20-$7F draws as '?', as disp_at() does. */
void ink_at(unsigned char col, unsigned char row, const char *s,
            unsigned char n, unsigned char ink);
void ink_char(unsigned char col, unsigned char row, char c, unsigned char ink);

#endif /* INK_H */
