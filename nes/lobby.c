/* lobby.c -- FujiNet Game Lobby for the NES: entry point, the state
 * dispatcher, and the drawing helpers every screen shares.
 *
 * A standalone port in the shape of ../coleco/: resolve a username, show the
 * lobby's paged server list, boot the chosen room's game client. Two NES
 * clients set its look:
 *
 *   - fujinet-config nes/ gives it the Family BASIC skin -- the light-blue
 *     double frame, the 7x7 font, "?xx ERROR" with a BEEP, a click on every
 *     press -- and the whole platform layer (fujidisp, fujiin, fujiedit, sfx
 *     are synced copies).
 *   - fujinet-google-calendar src/nes/ gives it the layout: a header in the
 *     frame's top edge, rules fencing the list off from a panel that spells
 *     out the selection, a status row, and the pad's buttons drawn as
 *     buttons in a legend along the bottom edge.
 */

#include <string.h>

#include "fujidisp.h"
#include "fujiin.h"
#include "sfx.h"
#include "state.h"

unsigned char state;

unsigned char cur;
unsigned char nrows;

char username[NAME_MAX + 1];

void status_line(const char *s)
{
    disp_row_clear(STATUS_ROW);
    disp_at(IN_L, STATUS_ROW, s);
}

/* "?MOUNT ERROR 8A", the way Family BASIC says "?SN ERROR", and its BEEP.
 * The code is the cartridge's own (FN_ETIMEOUT and friends) when the
 * transaction itself failed, else the reply command -- $15, a NAK from the
 * FujiNet. */
void fail(const char *what)
{
    fail_code(what, FN_ERRCODE != FN_OK ? FN_ERRCODE : FN_REPLYCMD);
}

void fail_code(const char *what, unsigned char code)
{
    unsigned char col = (unsigned char)(IN_L + 1 + strlen(what));

    disp_row_clear(STATUS_ROW);
    disp_at(IN_L, STATUS_ROW, "?");
    disp_at(IN_L + 1, STATUS_ROW, what);
    disp_at(col, STATUS_ROW, " ERROR ");
    disp_at_hex8((unsigned char)(col + 7), STATUS_ROW, code);
    sfx_beep();
}

void wait_frames(unsigned char n)
{
    unsigned char start = in_frames();

    while ((unsigned char)(in_frames() - start) < n)
        ;
}

/* A single rule across the text area, joined to the frame by tees. */
static void rule(unsigned char row)
{
    unsigned char i;

    disp_tile(FRAME_L, row, T_TEE_L);
    for (i = IN_L; i <= IN_R; i++)
        disp_tile(i, row, T_RULE);
    disp_tile(FRAME_R, row, T_TEE_R);
}

/* Text set into a frame edge, padded by a space either side the way
 * disp_box() sets its title. */
static void edge_text(unsigned char col, unsigned char row, const char *s)
{
    disp_char(col, row, ' ');
    disp_at((unsigned char)(col + 1), row, s);
    disp_char((unsigned char)(col + 1 + strlen(s)), row, ' ');
}

void draw_frame(void)
{
    disp_frame(NULL);
    edge_text(IN_L, FRAME_T, "FUJINET LOBBY");
    if (username[0] != 0)
        edge_text((unsigned char)(IN_R - 1 - strlen(username)), FRAME_T,
                  username);
    rule(RULE1_ROW);
    rule(RULE2_ROW);
    rule(RULE3_ROW);
}

/* " (A)JOIN (B)NAME <>PAGE ", the pad's own buttons standing in for
 * CONFIG's "A--" -- gcal's legend, in the Family BASIC frame. */
void draw_legend(void)
{
    unsigned char c = IN_L;

    disp_char(c++, LEGEND_ROW, ' ');
    disp_tile(c++, LEGEND_ROW, T_BTN_A);
    disp_at(c, LEGEND_ROW, "JOIN ");
    c += 5;
    disp_tile(c++, LEGEND_ROW, T_BTN_B);
    disp_at(c, LEGEND_ROW, "NAME ");
    c += 5;
    disp_tile(c++, LEGEND_ROW, T_PAD_L);
    disp_tile(c++, LEGEND_ROW, T_PAD_R);
    disp_at(c, LEGEND_ROW, "PAGE ");
}

/* The power-on screen, after Family BASIC's: no frame, the name typing
 * itself out a character at a time, then OK and the blinking cursor. Any
 * press cuts it short. */
static void splash(void)
{
    static const char name[] = "FUJINET LOBBY";
    unsigned char i;

    for (i = 0; name[i]; i++) {
        disp_char((unsigned char)(IN_L + i), 3, name[i]);
        sfx_blip();
        wait_frames(1);
    }
    disp_at(IN_L, 4, "FOR THE NES");
    disp_at(IN_L, 5, "(C) FUJINET PROJECT");
    disp_at(IN_L, 6, "OK");
    disp_cursor_at(IN_L, 7);
    for (i = 0; i < 60; i++) {
        if (in_read() != IN_NONE)
            break;
        disp_cursor_tick();
    }
    disp_cursor_off();
}

void main(void)
{
    sfx_init();
    in_init();
    disp_init();

    if (!fuji_nes_present()) {
        disp_at(IN_L, 3, "?NO FUJINET CART");
        sfx_beep();
        for (;;)
            ;
    }

    splash();
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
