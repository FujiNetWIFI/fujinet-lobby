/* lobby.c -- FujiNet Game Lobby for the ColecoVision: entry point, the state
 * dispatcher, and the small drawing helpers every screen shares.
 *
 * A standalone port in the shape of ../intv/ and fujinet-config/coleco/:
 * resolve a username, show the lobby's paged server list, boot the chosen
 * room's game client. What makes it possible on a machine with ~700 usable
 * bytes of RAM is the cartridge's reply window -- see constants.h's RAM
 * RULES.
 */

#include <os7.h>

#include "fujidisp.h"
#include "fujisnd.h"
#include "fujiin.h"
#include "fujisplash.h"
#include "state.h"

unsigned char state;

unsigned char cur;
unsigned char nrows;
unsigned char page_full;
unsigned int  cur_off;

char username[NAME_MAX + 1];

void status_line(const char *s)
{
    disp_row_clear(STATUS_ROW);
    disp_at(1, STATUS_ROW, s);
}

void legend_line(const char *s)
{
    disp_row_clear(LEGEND_ROW);
    disp_at(1, LEGEND_ROW, s);
}

void fail(const char *what)
{
    disp_row_clear(STATUS_ROW);
    disp_at(1, STATUS_ROW, what);
    disp_at_hex8(28, STATUS_ROW, FN_ERRCODE);
}

void draw_frame(const char *subtitle)
{
    disp_cls();
    disp_at(1, 1, "FUJINET LOBBY");
    if (subtitle)
        disp_at(1, 3, subtitle);
}

void wait_frames(unsigned char n)
{
    unsigned char start = in_frames();

    while ((unsigned char)(in_frames() - start) < n)
        ;
}

void main(void)
{
    snd_init();                 /* the PSG powers up buzzing; see fujisnd.h */

    /* Splash first: it owns the VDP, loading the logo glyphs over the pattern
     * range disp_init() wants for its inverse charset. disp_init() afterwards
     * puts both back. */
    splash_show();
    in_init();
    {
        unsigned char start = in_frames();

        while ((unsigned char)(in_frames() - start) < 120)
            if (in_read() == IN_FIRE)
                break;
    }

    disp_init();

    if (!fuji_coleco_present()) {
        disp_at(4, 8, "NO FUJINET CART");
        for (;;)
            ;
    }

    name_resolve();

    state = ST_LIST;
    for (;;) {
        switch (state) {
        case ST_BOOT:
            st_boot();
            break;
        default:
            st_list();
            break;
        }
    }
}
