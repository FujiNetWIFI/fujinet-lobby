/* st_list.c -- ST_LIST: fetch a page of the lobby's binary server list,
 * render it as game-header + room rows (rooms, not games, are what is
 * selectable, matching the C clients' layout), page with the d-pad, and hand
 * off to ST_BOOT on A.
 *
 * ../coleco/st_list.c's paging and grouping, verbatim in their arithmetic:
 * one network READ per 189-byte record, the page-start stack, the widow rule
 * for headers, and a page count that replays the render over the whole list.
 * What the NES adds is RAM to spare: each room drawn is also copied into
 * `rooms`, so the panel under the list can spell out whichever room the bar
 * is on, and the join acts on that copy without asking the server again.
 */

#include <string.h>

#include "fujidisp.h"
#include "fujiin.h"
#include "netraw.h"
#include "state.h"

Room rooms[LIST_ROWS];

/* Pagination: intv's page-start-stack method. cur_off advances by the rows
 * actually DRAWN -- render truncation is what makes that arithmetic exact. */
static unsigned int  cur_off;           /* server-side offset of this page */
static unsigned int  page_off[PSTK_MAX];
static unsigned char page_depth;
static unsigned char page_full;         /* more records past this page */
static unsigned char pages = 1;         /* count_pages(); floored at render */

static char          prevgame[GAME_FIELD];
static unsigned char rec_row[LIST_ROWS];

static unsigned char same_game(void)
{
    volatile unsigned char *r = FN_REPLY;
    unsigned char j;

    for (j = 0; j < GAME_FIELD; j++)
        if ((unsigned char)prevgame[j] != r[REC_GAME + j])
            return 0;
    return 1;
}

static void save_prevgame(void)
{
    volatile unsigned char *r = FN_REPLY;
    unsigned char j;

    for (j = 0; j < GAME_FIELD; j++)
        prevgame[j] = (char)r[REC_GAME + j];
}

/* A wire string field, out of the window and always NUL-terminated. */
static void copy_field(char *dst, unsigned char off, unsigned char len)
{
    volatile unsigned char *r = FN_REPLY + off;
    unsigned char j;

    for (j = 0; j < (unsigned char)(len - 1); j++)
        dst[j] = (char)r[j];
    dst[len - 1] = 0;
}

/* The record in the window, into rooms[n], before the next READ repaints
 * it. */
static void keep_room(unsigned char n)
{
    volatile unsigned char *r = FN_REPLY;
    Room *rm = &rooms[n];

    rm->game_type = r[REC_GAMETYPE];
    rm->players = r[REC_PLAYERS];
    rm->max_players = r[REC_MAXPLAYERS];
    copy_field(rm->game, REC_GAME, GAME_FIELD);
    copy_field(rm->server, REC_SERVER, SERVER_FIELD);
    copy_field(rm->region, REC_REGION, REGION_FIELD);
    copy_field(rm->server_url, REC_SERVERURL, URL_FIELD);
    copy_field(rm->client_url, REC_CLIENTURL, URL_FIELD);
}

/* "players/max" right-flushed ending at PLAYERS_END, each clamped to 0-99
 * so the worst case ("99/99") never collides with the room name. Built right
 * to left -- no itoa, no buffer. */
static void draw_players(unsigned char row, const Room *rm)
{
    unsigned char p = rm->players;
    unsigned char m = rm->max_players;
    unsigned char col = PLAYERS_END;

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

/* A string clipped to `n` columns. */
static void draw_clipped(unsigned char col, unsigned char row,
                         const char *s, unsigned char n)
{
    unsigned char i;

    for (i = 0; i < n && s[i] != 0; i++)
        disp_char((unsigned char)(col + i), row, s[i]);
}

/* The panel: the selected room's name in full (the list row clips it), then
 * its region and where its client comes from -- the client URL without its
 * scheme, as the join will split it. Redrawn on every bar move; no
 * traffic. */
static void panel_draw(void)
{
    const Room *rm;
    const char *u;
    unsigned char i, c;
    char ch;

    disp_row_clear(PANEL_ROW);
    disp_row_clear(PANEL_ROW + 1);
    if (nrows == 0)
        return;
    rm = &rooms[cur];

    draw_clipped(IN_L, PANEL_ROW, rm->server, IN_W);

    c = IN_L;
    for (i = 0; rm->region[i] != 0; i++) {
        ch = rm->region[i];
        if (ch >= 'a' && ch <= 'z')
            ch = (char)(ch - 32);
        disp_char(c++, PANEL_ROW + 1, ch);
    }
    if (c != IN_L)
        c++;
    u = strstr(rm->client_url, "://");
    u = u ? u + 3 : rm->client_url;
    draw_clipped(c, (unsigned char)(PANEL_ROW + 1), u,
                 (unsigned char)(IN_R + 1 - c));
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

/* Page count for the "PAGE X/Y" label. ceil(total/PAGESIZE) is wrong here:
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

static void label_draw(void)
{
    unsigned char c = 18;

    disp_row_clear(LABEL_ROW);
    disp_at(IN_L, LABEL_ROW, "GAME/ROOM");
    disp_at(c - 5, LABEL_ROW, "PAGE");
    disp_at_u16(c, LABEL_ROW, (unsigned int)(page_depth + 1));
    c = (unsigned char)(c + ((page_depth + 1 >= 10) ? 2 : 1));
    disp_char(c, LABEL_ROW, '/');
    disp_at_u16((unsigned char)(c + 1), LABEL_ROW, pages);
    disp_at(IN_R - 3, LABEL_ROW, "PLRS");
}

/* Fetch and render the page at cur_off. Enter with `cur` already holding the
 * selection intent (0 for a fresh page, 0xFF for "last row" on an up-cross);
 * it is clamped to what actually drew. */
static void list_page(void)
{
    volatile unsigned char *r;
    unsigned char i, row, got, drawn, hdr;
    bool net_err = false;

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

        if (!netraw_lobby_open(cur_off, PAGESIZE)) {
            /* An empty page 404s, which is not a fault; a cartridge that
             * timed out, or a FujiNet that could not reach the server, is. */
            net_err = netraw_fault;
        } else {
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
                keep_room(drawn);
                if (hdr) {
                    draw_clipped(GAME_COL, row, rooms[drawn].game, GAME_LEN);
                    row++;
                }
                rec_row[drawn] = row;
                draw_clipped(ROOM_COL, row, rooms[drawn].server, ROOM_LEN);
                draw_players(row, &rooms[drawn]);
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
         * page stack and refetch. Iterative, not recursive. */
        page_depth--;
        cur_off = page_off[page_depth];
    }

    /* Floored so a stale or failed count never renders "4/3". */
    if ((unsigned char)(page_depth + 1) > pages)
        pages = (unsigned char)(page_depth + 1);
    label_draw();

    if (nrows == 0) {
        cur = 0;
        disp_at(ROOM_COL, LIST_TOP + 1,
                net_err ? "LOBBY UNREACHABLE" : "NO SERVERS ONLINE");
    } else {
        if (cur >= nrows)
            cur = (unsigned char)(nrows - 1);
        disp_row_invert(rec_row[cur], true);
    }
    panel_draw();
    if (net_err) {
        fail_code("NET", netraw_err);
        disp_at(IN_R - 6, STATUS_ROW, "START");
    } else {
        status_line(nrows ? "START--RELOAD" : "START--LOOK AGAIN");
    }
}

static void list_draw(void)
{
    draw_frame();
    draw_legend();
    list_page();
}

static void page_back(void)
{
    if (page_depth != 0) {
        page_depth--;
        cur_off = page_off[page_depth];
        cur = 0;
        list_page();
    }
}

static void page_fwd(void)
{
    if (page_full && page_depth < PSTK_MAX) {
        page_off[page_depth++] = cur_off;
        cur_off += nrows;
        cur = 0;
        list_page();
    }
}

/* The bar, with page-crossing at both ends: down off the last row pages
 * forward selecting the first, up off the first pages back selecting the
 * last. */
static void lmove(signed char d)
{
    if (d < 0) {
        if (nrows != 0 && cur > 0) {
            disp_row_invert(rec_row[cur], false);
            cur--;
            disp_row_invert(rec_row[cur], true);
            panel_draw();
        } else if (page_depth != 0) {
            page_depth--;
            cur_off = page_off[page_depth];
            cur = 0xFF;         /* clamped to the last drawn row */
            list_page();
        }
    } else {
        if (nrows == 0)
            return;
        if ((unsigned char)(cur + 1) < nrows) {
            disp_row_invert(rec_row[cur], false);
            cur++;
            disp_row_invert(rec_row[cur], true);
            panel_draw();
        } else {
            page_fwd();
        }
    }
}

void st_list(void)
{
    list_draw();
    while (state == ST_LIST) {
        switch (in_read()) {
        case IN_UP:
            lmove(-1);
            break;
        case IN_DOWN:
            lmove(1);
            break;
        case IN_LEFT:
            page_back();
            break;
        case IN_RIGHT:
            page_fwd();
            break;
        case IN_BACK:
            name_edit();        /* fn_edit owned the screen: full redraw */
            list_draw();
            break;
        case IN_START:
            /* A reload starts over: new rooms may have shifted every
             * offset past the first page. */
            page_depth = 0;
            cur_off = 0;
            cur = 0;
            list_page();
            break;
        case IN_FIRE:
            if (nrows != 0)
                state = ST_BOOT;
            break;
        }
    }
}
