/* lobby.c -- FujiNet Game Lobby for the Sega Master System: entry point,
 * the state dispatcher, and the helpers every screen shares.
 *
 * A standalone port in the shape of ../coleco/ and the nes branch's nes/:
 * resolve a username, show the lobby's paged server list, boot the chosen
 * room's game client. fujinet-config sms/ gives it its look and its whole
 * platform layer (fujidisp, fujiin, fujiedit, fujisnd, fujisplash are synced
 * copies): Phantasy Star's windows, the pointer notched into the window's
 * left edge, a COMMAND pop-up on Pause, the message window typing itself
 * out, and the progress window for the boot. The helpers below are
 * fujinet-config sms/config.c's, with the selection following the room's
 * screen row instead of a fixed pitch.
 */

#include <string.h>

#include "fujidisp.h"
#include "fujisnd.h"
#include "fujiin.h"
#include "fujisplash.h"
#include "ink.h"
#include "state.h"

unsigned char state;

unsigned char cur;
unsigned char nrows;

char username[NAME_MAX + 1];

void status_line(const char *s)
{
    msg_type(MSG_LINE1, s);
}

void status_now(const char *s)
{
    msg_put(MSG_LINE1, s);
    snd_settle();
    cur_steady();
}

void legend_line(const char *s)
{
    msg_put(MSG_LINE2, s);
}

void fail_code(const char *what, unsigned char code)
{
    snd_play(SND_ERROR);
    msg_type(MSG_LINE1, what);
    disp_at_hex8(28, MSG_LINE1, code);
}

/* The cartridge's own code (FN_ETIMEOUT and friends) when the transaction
 * itself failed, else the reply command -- $15, a NAK from the FujiNet. */
void fail(const char *what)
{
    fail_code(what, FN_ERRCODE != FN_OK ? FN_ERRCODE : FN_REPLYCMD);
}

void draw_frame(const char *title)
{
    layout_show();
    win_clear();
    win_title(title ? title : "");
    disp_row_blank(MSG_LINE1);
    disp_row_blank(MSG_LINE2);
}

void wait_frames(unsigned char n)
{
    in_wait(n);
}

/* The selected room: its text carries the invisible sprite-palette bit (for
 * the harness) and the left border beside it becomes the blinking pointer.
 * disp_row_attr() keeps each cell's ink bit, so the row keeps its colour. */
void list_select(unsigned char i, bool on)
{
    unsigned char row = rec_row[i];

    if (on) {
        disp_row_attr(row, A_SEL, 0);
        cur_show(0, row, T_CURSOR, T_VEDGE);
    } else {
        cur_hide();
        disp_row_attr(row, 0, A_SEL);
    }
}

/* A COMMAND window at the top right of the main window: items on every
 * other row, the pointer in its left edge, up and down wrapping. The list's
 * own pointer stays lit underneath until it closes. */
unsigned char menu_pick(const char *const *names, const unsigned char *events,
                        unsigned char n)
{
    unsigned char i = 0, w = 0, k, x0, ev;

    for (k = 0; k < n; k++) {
        unsigned char len = (unsigned char)strlen(names[k]);

        if (len > w)
            w = len;
    }
    x0 = (unsigned char)(TEXT_R - w - 2);

    snd_play(SND_OK);
    ovl_open(x0, 1, TEXT_R - 1, (unsigned char)(1 + 2 * n));
    for (k = 0; k < n; k++)
        disp_at((unsigned char)(x0 + 1), (unsigned char)(2 + 2 * k), names[k]);
    in_repeat(false);
    cur_show(x0, 2, T_CURSOR, T_VEDGE);
    for (;;) {
        ev = in_read();
        if (ev == IN_UP || ev == IN_DOWN) {
            if (ev == IN_UP)
                i = (unsigned char)(i ? i - 1 : n - 1);
            else
                i = (unsigned char)(i + 1 < n ? i + 1 : 0);
            cur_hide();
            cur_show(x0, (unsigned char)(2 + 2 * i), T_CURSOR, T_VEDGE);
            snd_play(SND_MOVE);
        } else if (ev == IN_FIRE || ev == IN_KEYSTAR || ev == IN_MENU) {
            break;
        }
    }
    in_repeat(true);
    snd_play(ev == IN_FIRE ? SND_OK : SND_BACK);
    ovl_close();
    if (nrows)
        list_select(cur, true);
    return ev == IN_FIRE ? events[i] : IN_NONE;
}

void main(void)
{
    snd_init();
    in_init();
    splash_show();
    in_init();                  /* drop presses made during the fade */
    disp_init();
    ink_init();

    if (!fuji_sms_present()) {
        draw_frame("FUJINET");
        snd_play(SND_ERROR);
        status_line("NO FUJINET CART");
        for (;;)
            in_frames();        /* keeps the sound ticking to its end */
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
