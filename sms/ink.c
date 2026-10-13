/* ink.c -- see ink.h. */

#include <arch/sms.h>

#include "fujidisp.h"
#include "constants.h"
#include "ink.h"

/* build/font_data.c, from font.txt: 1bpp rows and a colour per tile. */
extern const unsigned char font_rows[128 * 8];

/* The printable half of the font, 1bpp spread into the one plane-set that
 * colour index `ci` selects -- fujidisp.c's load_cfg_tiles(), at another
 * tile and in another colour. */
static void load_copy(unsigned int tile, unsigned char ci)
{
    const unsigned char *src = font_rows + 32 * 8;
    unsigned char t, r, b;

    disp_vram_addr(tile * 32);
    for (t = 32; t < 128; t++) {
        for (r = 0; r < 8; r++) {
            b = *src++;
            IO_VDP_DATA = (ci & 1) ? b : 0;
            IO_VDP_DATA = (ci & 2) ? b : 0;
            IO_VDP_DATA = (ci & 4) ? b : 0;
            IO_VDP_DATA = (ci & 8) ? b : 0;
        }
    }
}

void ink_init(void)
{
    /* Display off while VRAM fills, so no write spacing; the screen is
     * blank this soon after disp_init() anyway. */
    disp_vdp_reg(1, 0x80);
    load_copy(160, PAL_GAME);
    load_copy(288, PAL_DIM);
    disp_vdp_reg(1, 0xC0);

    pal_set(PAL_GAME, COL_GAME);
    pal_set(PAL_GAME + 16, COL_GAME);
    pal_set(PAL_DIM, COL_DIM);
    pal_set(PAL_DIM + 16, COL_DIM);
}

void ink_char(unsigned char col, unsigned char row, char c, unsigned char ink)
{
    unsigned char t = (unsigned char)c;

    if (col > TEXT_R)
        return;
    if (t < 32 || t > 127)
        t = '?';
    disp_tile(col, row, (unsigned char)(t | (ink & 0x80)),
              (unsigned char)(ink & 0x01));
}

void ink_at(unsigned char col, unsigned char row, const char *s,
            unsigned char n, unsigned char ink)
{
    while (n-- && *s && col <= TEXT_R)
        ink_char(col++, row, *s++, ink);
}
