/* state.h -- the shared globals and the seams between the screen modules.
 *
 * One invariant to keep when adding code: the reply window at $5000 is only
 * repainted by committing a transaction. A record is read out of it once,
 * into st_list.c's room cache, before the next READ; everything after that
 * -- the panel, the join -- works from the cache. fn_edit() runs no
 * transactions, which is what makes "resolve, edit, write back" safe in
 * st_name.c.
 */

#ifndef STATE_H
#define STATE_H

#include <stdbool.h>

#include <fujinet-fuji.h>
#include <fujinet-nes.h>
#include <fujinet-bus-nes.h>

#include "constants.h"

/* One drawn room, as much of its record as the panel and the join need. */
typedef struct {
    unsigned char game_type;
    unsigned char players;
    unsigned char max_players;
    char game[GAME_FIELD];
    char server[SERVER_FIELD];
    char region[REGION_FIELD];
    char server_url[URL_FIELD];
    char client_url[URL_FIELD];
} Room;

extern unsigned char state;

extern unsigned char cur;        /* selection index within the drawn page */
extern unsigned char nrows;      /* room rows the current page actually drew */
extern Room rooms[LIST_ROWS];    /* those rooms, in draw order */

extern char username[NAME_MAX + 1];

/* lobby.c */
void status_line(const char *s);            /* STATUS_ROW, cleared first */
void fail(const char *what);                /* "?WHAT ERROR xx" + BEEP */
void fail_code(const char *what, unsigned char code); /* ...a given code */
void draw_frame(void);                      /* cls + frame + rules + name */
void draw_legend(void);                     /* the buttons, in the bottom edge */
void wait_frames(unsigned char n);          /* n vblanks, input ignored */

/* One screen per state; each draws itself, runs its own event loop, and
 * returns once it has set `state` to something else. */
void st_list(void);
void st_boot(void);                         /* boots rooms[cur]; returns only
                                               on failure, state back to
                                               ST_LIST */

/* st_name.c -- blocking, no state of their own */
void name_resolve(void);                    /* appkey -> username, or editor */
void name_edit(void);                       /* OSK; caller redraws after */

#endif /* STATE_H */
