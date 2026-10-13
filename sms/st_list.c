/* st_list.c -- ST_LIST: fetch a page of the lobby's binary server list,
 * render it as game-header + room rows (rooms, not games, are what is
 * selectable, matching the C clients' layout), page with the d-pad, and hand
 * off to ST_BOOT on button 1.
 *
 * The nes branch's nes/st_list.c (itself ../coleco/st_list.c's paging and
 * grouping), verbatim in its arithmetic: one network READ per 189-byte
 * record, the page-start stack, the widow rule for headers, and a page count
 * that replays the render over the whole list. Each room drawn is also
 * copied into `rooms`, so the message window can spell out whichever room
 * the pointer is on, and the join acts on that copy without asking the
 * server again.
 *
 * What the Master System adds is colour (ink.h): game headers in yellow,
 * rooms nobody is in grey, rooms with someone in them white.
 */

#include <string.h>

#include "fujidisp.h"
#include "fujisnd.h"
#include "fujiin.h"
#include "ink.h"
#include "netraw.h"
#include "state.h"

Room rooms[LIST_ROWS];
unsigned char rec_row[LIST_ROWS];

/* Pagination: intv's page-start-stack method. cur_off advances by the rows
 * actually DRAWN -- render truncation is what makes that arithmetic exact. */
static unsigned int  cur_off;           /* server-side offset of this page */
static unsigned int  page_off[PSTK_MAX];
static unsigned char page_depth;
static unsigned char page_full;         /* more records past this page */
static unsigned char pages = 1;         /* count_pages(); floored at render */

static char          prevgame[GAME_FIELD];
static char          label[12];         /* "PAGE 10/10" */

/* Pause's COMMAND window. */
static const char *const menu_names[] = { "REFRESH", "NAME", "CONFIG" };
static const unsigned char menu_events[] = { IN_KEYSTAR, IN_KEY0, IN_KEYHASH };

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
    prevgame[GAME_FIELD - 1] = 0;
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
    copy_field(rm->server, REC_SERVER, SERVER_FIELD);
    copy_field(rm->region, REC_REGION, REGION_FIELD);
    copy_field(rm->server_url, REC_SERVERURL, URL_FIELD);
    copy_field(rm->client_url, REC_CLIENTURL, URL_FIELD);
}

/* "players/max" right-flushed ending at PLAYERS_END, each clamped to 0-99
 * so the worst case ("99/99") never collides with the room name. Built right
 * to left -- no itoa, no buffer. */
static void draw_players(unsigned char row, const Room *rm, unsigned char ink)
{
    unsigned char p = rm->players;
    unsigned char m = rm->max_players;
    unsigned char col = PLAYERS_END;

    if (p > 99)
        p = 99;
    if (m > 99)
        m = 99;
    ink_char(col--, row, (char)('0' + m % 10), ink);
    if (m >= 10)
        ink_char(col--, row, (char)('0' + m / 10), ink);
    ink_char(col--, row, '/', ink);
    ink_char(col--, row, (char)('0' + p % 10), ink);
    if (p >= 10)
        ink_char(col, row, (char)('0' + p / 10), ink);
}

/* Message line 1 follows the pointer: the selected room's name in full (the
 * list row clips it) and its region on the right. Instant, no traffic. */
static void detail_draw(void)
{
    const Room *rm;
    unsigned char n, i;
    char c;

    disp_row_blank(MSG_LINE1);
    if (nrows == 0)
        return;
    rm = &rooms[cur];
    ink_at(TEXT_L, MSG_LINE1, rm->server, DETAIL_LEN, INK_WHITE);
    n = (unsigned char)strlen(rm->region);
    for (i = 0; i < n; i++) {
        c = rm->region[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        ink_char((unsigned char)(TEXT_R + 1 - n + i), MSG_LINE1, c, INK_WHITE);
    }
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

/* Right to left into `label` ending at `end`; returns where it starts. */
static unsigned char put_dec(unsigned char end, unsigned char v)
{
    do {
        label[--end] = (char)('0' + v % 10);
        v /= 10;
    } while (v && end);
    return end;
}

/* Text set into a border row, a blank either side the way win_title()
 * sets the title, ending at EDGE_END. */
static void edge_text(unsigned char row, const char *s, unsigned char n)
{
    unsigned char col = (unsigned char)(EDGE_END + 1 - n);

    disp_char((unsigned char)(col - 1), row, ' ');
    disp_at(col, row, s);
    disp_char(EDGE_END + 1, row, ' ');
}

/* "PAGE X/Y" in the message window's top border, where blank rows sit
 * above and below it (the font's capitals fill their cells, so in the main
 * window's bottom border it would touch the last room). label_clear() puts
 * the field it can occupy (" PAGE 10/10 ") back to plain edge, so a shorter
 * label leaves nothing of a longer one behind, and so the keyboard, which
 * keeps the message window, does not show a page number. */
static void label_clear(void)
{
    unsigned char c;

    for (c = EDGE_END + 1 - sizeof(label); c <= EDGE_END + 1; c++)
        disp_tile(c, MSG_TOP, T_HEDGE, 0);
}

static void label_draw(void)
{
    unsigned char s;

    s = put_dec(sizeof(label) - 1, pages);
    label[--s] = '/';
    s = put_dec(s, (unsigned char)(page_depth + 1));
    label[--s] = ' ';
    label[--s] = 'E';
    label[--s] = 'G';
    label[--s] = 'A';
    label[--s] = 'P';
    label[sizeof(label) - 1] = 0;

    label_clear();
    edge_text(MSG_TOP, label + s, (unsigned char)(sizeof(label) - 1 - s));
}

/* Fetch and render the page at cur_off. Enter with `cur` already holding the
 * selection intent (0 for a fresh page, 0xFF for "last row" on an up-cross);
 * it is clamped to what actually drew. */
static void list_page(void)
{
    volatile unsigned char *r;
    unsigned char i, row, got, drawn, hdr, ink;
    bool net_err = false;

    cur_hide();
    status_now("LOADING...");
    if (page_depth == 0)
        count_pages();

    for (;;) {
        for (i = 0; i < LIST_ROWS; i++)
            disp_row_blank((unsigned char)(LIST_TOP + i));

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
                save_prevgame();
                if (hdr) {
                    ink_at(GAME_COL, row, prevgame, GAME_LEN, INK_GAME);
                    row++;
                }
                rec_row[drawn] = row;
                ink = rooms[drawn].players ? INK_WHITE : INK_DIM;
                ink_at(ROOM_COL, row, rooms[drawn].server, ROOM_LEN, ink);
                draw_players(row, &rooms[drawn], ink);
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
                net_err ? "LOBBY UNREACHABLE." : "NO ROOMS ARE OPEN.");
        legend_line("2 REFRESH  PAUSE MENU");
        if (net_err)
            fail_code("ENET", netraw_err);
        else
            status_line("PRESS 2 TO LOOK AGAIN.");
    } else {
        if (cur >= nrows)
            cur = (unsigned char)(nrows - 1);
        list_select(cur, true);
        legend_line("1 JOIN  2 REFRESH  PAUSE MENU");
        detail_draw();
    }
}

static void list_draw(void)
{
    draw_frame("GAME LOBBY");
    edge_text(MAIN_TOP, username, (unsigned char)strlen(username));
    list_page();
}

static void page_back(void)
{
    if (page_depth != 0) {
        snd_play(SND_MOVE);
        page_depth--;
        cur_off = page_off[page_depth];
        cur = 0;
        list_page();
    }
}

static void page_fwd(void)
{
    if (page_full && page_depth < PSTK_MAX) {
        snd_play(SND_MOVE);
        page_off[page_depth++] = cur_off;
        cur_off += nrows;
        cur = 0;
        list_page();
    }
}

/* The pointer, with page-crossing at both ends: down off the last room
 * pages forward selecting the first, up off the first pages back selecting
 * the last. */
static void lmove(signed char d)
{
    if (d < 0) {
        if (nrows != 0 && cur > 0) {
            list_select(cur, false);
            cur--;
            list_select(cur, true);
            snd_play(SND_MOVE);
            detail_draw();
        } else if (page_depth != 0) {
            snd_play(SND_MOVE);
            page_depth--;
            cur_off = page_off[page_depth];
            cur = 0xFF;         /* clamped to the last drawn row */
            list_page();
        }
    } else {
        if (nrows == 0)
            return;
        if ((unsigned char)(cur + 1) < nrows) {
            list_select(cur, false);
            cur++;
            list_select(cur, true);
            snd_play(SND_MOVE);
            detail_draw();
        } else {
            page_fwd();
        }
    }
}

/* Back to the cartridge's CONFIG, through its loader page -- the way out
 * that CONFIG's own GAME LOBBY command came in by. */
static void to_config(void)
{
    cur_hide();
    status_now("BACK TO CONFIG.");
    fade_out();
    snd_init();
    fuji_sms_exit_to_config();  /* does not return */
}

void st_list(void)
{
    unsigned char ev;

    list_draw();
    while (state == ST_LIST) {
        ev = in_read();
        if (ev == IN_MENU)
            ev = menu_pick(menu_names, menu_events, 3);
        switch (ev) {
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
        case IN_KEYSTAR:
            /* A reload starts over: new rooms may have shifted every
             * offset past the first page. */
            snd_play(SND_OK);
            page_depth = 0;
            cur_off = 0;
            cur = 0;
            list_page();
            break;
        case IN_KEY0:
            label_clear();      /* the keyboard keeps the message window */
            name_edit();        /* fn_edit owned the screen: full redraw */
            list_draw();
            break;
        case IN_KEYHASH:
            to_config();
            break;
        case IN_FIRE:
            if (nrows != 0) {
                snd_play(SND_OK);
                state = ST_BOOT;
            }
            break;
        }
    }
}
