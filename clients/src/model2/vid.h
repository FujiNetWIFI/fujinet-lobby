#ifndef MODEL2_VID_H
#define MODEL2_VID_H

/* The 80x24 screen, drawn into a buffer and sent by difference. A cell is
   one byte: bit 7 is reverse video, the rest a graphics character (00h-1Fh)
   or ASCII. */

#include <stdint.h>
#include <stdbool.h>

#define VID_COLS 80
#define VID_ROWS 24

#define REV 0x80

// Model II graphics characters, from the Operation Manual's chart
#define G_CORNER_UL   0x00
#define G_CORNER_UR   0x01
#define G_CORNER_LR   0x02
#define G_CORNER_LL   0x03
#define G_TEE_LEFT_THIN  0x09
#define G_TEE_RIGHT_THIN 0x0B
#define G_VLINE       0x14
#define G_HLINE       0x16
#define G_HLINE_THIN  0x17

void vid_init(void);
void vid_exit(void);
void vid_put(uint8_t x, uint8_t y, uint8_t c);
void vid_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t c);
void vid_rev(uint8_t x, uint8_t y, uint8_t w, bool on);

// Writes s from x with attr OR'd in, at most w cells, space padded to w if pad
void vid_text(uint8_t x, uint8_t y, const char *s, uint8_t w, uint8_t attr, bool pad);

void vid_flush(void);
void vid_bell(void);

#endif /* MODEL2_VID_H */
