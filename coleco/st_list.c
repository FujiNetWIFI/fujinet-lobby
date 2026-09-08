/* st_list.c -- ST_LIST: fetch a page of the lobby's binary server list,
 * render it as game-header + room rows (rooms, not games, are what is
 * selectable, matching the C clients' layout), page with the stick or
 * keypad, and hand off to ST_BOOT on fire.
 *
 * The port of intv/st_list.bas onto fujinet-config/coleco/st_files.c's
 * streaming mechanics: one network READ per 189-byte record, each drawn
 * straight from the reply window to VRAM before the next read repaints it.
 * Nothing is cached -- the only page state in RAM is the previous record's
 * 17-byte game field (the header grouping needs it across repaints), one
 * screen-row byte per drawn room, and the page-start stack. Selection is
 * disp_row_invert()'s VRAM read-back: moving the bar costs no network
 * traffic, and booting re-fetches the chosen record by offset (st_boot.c).
 */

#include <string.h>

#include "fujidisp.h"
#include "fujisnd.h"
#include "fujiin.h"
#include "netraw.h"
#include "state.h"

/* Pagination: intv's page-start-stack method. cur_off advances by the rows
 * actually DRAWN -- render truncation is what makes that arithmetic exact. */
static unsigned int  page_off[PSTK_MAX];
static unsigned char page_depth;
static unsigned char pages = 1;         /* count_pages(); floored at render */

static char          prevgame[17];
static unsigned char rec_row[LIST_ROWS];

static unsigned char same_game(void)
{
    volatile unsigned char *r = FN_REPLY;
    unsigned char j;

    for (j = 0; j < 17; j++)
        if ((unsigned char)prevgame[j] != r[REC_GAME + j])
            return 0;
    return 1;
}

static void save_prevgame(void)
{
    volatile unsigned char *r = FN_REPLY;
    unsigned char j;

    for (j = 0; j < 17; j++)
        prevgame[j] = (char)r[REC_GAME + j];
}

/* "players/max" right-flushed ending at col 30, each clamped to 0-99 so the
 * worst case ("99/99") never collides with the room name. Built right to
 * left -- no itoa, no buffer. */
static void draw_players(unsigned char row)
{
    volatile unsigned char *r = FN_REPLY;
    unsigned char p = r[REC_PLAYERS];
    unsigned char m = r[REC_MAXPLAYERS];
    unsigned char col = 30;

    if (p > 99)
        p = 99;
    if (m > 99)
        m = 99;
    disp_char(col--, row, (char)('0' + m % 10));
    if (m >= 10)
        disp_char(col--, row, (char)('0' + m / 10));
    disp_char(col--, row, '/');
    disp_char(col--, row, (char)('0' + p % 10));
    if (p >= 10)
        disp_char(col, row, (char)('0' + p / 10));
}

/* Would a record whose header-need is `hdr` still fit at `row`? A header
 * needs its own row AND a room row under it (a header alone would be a
 * widow, so the record opens the next page instead, where the header is
 * redrawn); a bare room row needs one. Verbatim from intv's lb_render_list. */
static unsigned char rec_fits(unsigned char hdr, unsigned char row)
{
    if (hdr)
        return row < LIST_LAST_ROW;
    return row <= LIST_LAST_ROW;
}

/* Page count for the "page X/Y" corner. ceil(total/PAGESIZE) is wrong here:
 * pages are bounded by screen rows, headers cost rows, and a widowed header
 * defers its record -- so the count replays the render arithmetic over the
 * whole list, one record-sized read at a time, nothing kept but prevgame.
 * One extra HTTP request per full reload (depth-0 fetches only), same as
 * intv's lb_count_pages. A short read mid-walk just stops the count early;
 * the render-time floor keeps the display sane. */
static void count_pages(void)
{
    volatile unsigned char *r;
    unsigned char total, i, row, drawn, hdr;

    pages = 1;
    if (!netraw_lobby_open(0, 255))
        return;
    if (netraw_read(3) < 3) {
        netraw_close();
        return;
    }
    r = FN_REPLY;
    total = r[0];

    row = LIST_TOP;
    drawn = 0;
    for (i = 0; i < total; i++) {
        if (netraw_read(REC_STRIDE) < REC_STRIDE)
            break;
        hdr = (drawn == 0) || !same_game();
        if (!rec_fits(hdr, row)) {
            pages++;
            row = LIST_TOP;
            drawn = 0;
            hdr = 1;
        }
        if (hdr)
            row++;
        row++;
        drawn++;
        save_prevgame();
    }
    netraw_close();
}

static void list_legend(void)
{
    status_line(nrows ? "FIRE PLAYS THE SELECTED ROOM" : "9 CHECKS AGAIN");
    legend_line("1 NAME  < > PAGE  9 REFRESH");
}

/* Fetch and render the page at cur_off. Enter with `cur` already holding the
 * selection intent (0 for a fresh page, 0xFF for "last row" on an up-cross);
 * it is clamped to what actually drew. */
static void list_page(void)
{
    volatile unsigned char *r;
    unsigned char i, row, got, drawn, hdr, col;

    status_line("LOADING...");
    if (page_depth == 0)
        count_pages();

    for (;;) {
        for (i = 0; i < LIST_ROWS; i++)
            disp_row_clear((unsigned char)(LIST_TOP + i));

        nrows = 0;
        page_full = 0;
        got = 0;
        drawn = 0;

        if (netraw_lobby_open(cur_off, PAGESIZE)) {
            if (netraw_read(3) >= 3) {
                r = FN_REPLY;
                got = r[0];
                if (got > PAGESIZE)
                    got = PAGESIZE;
            }
            row = LIST_TOP;
            for (i = 0; i < got; i++) {
                if (netraw_read(REC_STRIDE) < REC_STRIDE)
                    break;      /* truncated reply: keep what landed */
                hdr = (drawn == 0) || !same_game();
                if (!rec_fits(hdr, row))
                    break;      /* row-bound: this record opens the next page */
                r = FN_REPLY;
                if (hdr) {
                    for (col = 0; col < GAME_LEN && r[REC_GAME + col] != 0; col++)
                        disp_char((unsigned char)(1 + col), row,
                                  (char)r[REC_GAME + col]);
                    row++;
                }
                rec_row[drawn] = row;
                for (col = 0; col < ROOM_LEN && r[REC_SERVER + col] != 0; col++)
                    disp_char((unsigned char)(2 + col), row,
                              (char)r[REC_SERVER + col]);
                draw_players(row);
                save_prevgame();
                drawn++;
                row++;
            }
            netraw_close();
            nrows = drawn;
            /* More pages exist if the server filled our ask, or if records
             * were consumed (or truncated) past what drew -- the second
             * clause also un-strands the tail of a render-truncated final
             * page, which intv's reply-size-only latch loses. */
            page_full = (got >= PAGESIZE) || (drawn < got);
        }

        if (nrows != 0 || page_depth == 0)
            break;
        /* Overshot past the end (the server 404s an empty page): pop the
         * page stack and refetch. Iterative, not recursive -- the stack down
         * here is ~250 bytes. */
        page_depth--;
        cur_off = page_off[page_depth];
    }

    /* Page counter, row 1: current/total, floored so a stale or failed count
     * never renders "4/3". */
    if ((unsigned char)(page_depth + 1) > pages)
        pages = (unsigned char)(page_depth + 1);
    disp_at(14, 1, "      ");
    i = 15;
    disp_at_u16(i, 1, (unsigned int)(page_depth + 1));
    i = (unsigned char)(i + ((page_depth + 1 >= 10) ? 2 : 1));
    disp_char(i, 1, '/');
    disp_at_u16((unsigned char)(i + 1), 1, pages);

    if (nrows == 0) {
        disp_at(2, LIST_TOP, "NO SERVERS ONLINE");
        cur = 0;
    } else {
        if (cur >= nrows)
            cur = (unsigned char)(nrows - 1);
        disp_row_invert(rec_row[cur], true);
    }
    list_legend();
}

static void list_draw(void)
{
    draw_frame(NULL);
    disp_at((unsigned char)(31 - strlen(username)), 1, username);
    disp_at(1, 3, "GAME / ROOM");
    disp_at(24, 3, "PLAYERS");
    list_page();
}

static void page_back(void)
{
    if (page_depth != 0) {
        page_depth--;
        cur_off = page_off[page_depth];
        cur = 0;
        snd_click();
        list_page();
    }
}

static void page_fwd(void)
{
    if (page_full && page_depth < PSTK_MAX) {
        page_off[page_depth++] = cur_off;
        cur_off += nrows;
        cur = 0;
        snd_click();
        list_page();
    }
}

/* The bar, with page-crossing at both ends: down off the last row pages
 * forward selecting the first, up off the first pages back selecting the
 * last. Every accepted move clicks; a move that goes nowhere stays silent. */
static void lmove(signed char d)
{
    if (d < 0) {
        if (nrows != 0 && cur > 0) {
            disp_row_invert(rec_row[cur], false);
            cur--;
            disp_row_invert(rec_row[cur], true);
            snd_click();
        } else if (page_depth != 0) {
            page_depth--;
            cur_off = page_off[page_depth];
            cur = 0xFF;         /* clamped to the last drawn row */
            snd_click();
            list_page();
        }
    } else {
        if (nrows == 0)
            return;
        if ((unsigned char)(cur + 1) < nrows) {
            disp_row_invert(rec_row[cur], false);
            cur++;
            disp_row_invert(rec_row[cur], true);
            snd_click();
        } else if (page_full && page_depth < PSTK_MAX) {
            page_off[page_depth++] = cur_off;
            cur_off += nrows;
            cur = 0;
            snd_click();
            list_page();
        }
    }
}

void st_list(void)
{
    list_draw();
    while (state == ST_LIST) {
        unsigned char ev = in_read();

        switch (ev) {
        case IN_UP:
            lmove(-1);
            break;
        case IN_DOWN:
            lmove(1);
            break;
        case IN_LEFT:
        case IN_KEY0 + 2:
            page_back();
            break;
        case IN_RIGHT:
        case IN_KEY0 + 3:
            page_fwd();
            break;
        case IN_KEY0 + 1:
            name_edit();        /* fn_edit owned the screen: full redraw */
            list_draw();
            break;
        case IN_KEY0 + 9:
            snd_click();
            list_page();
            break;
        case IN_FIRE:
            if (nrows != 0) {
                snd_click();
                state = ST_BOOT;
            }
            break;
        }
    }
}
