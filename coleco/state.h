/* state.h -- the shared globals and the seams between the screen modules.
 *
 * One invariant to keep when adding code: the reply window at $F800 is only
 * repainted by committing a transaction. Any flow that acts on a record after
 * an intervening transaction must re-fetch it and stream out of the fresh
 * window (the way st_boot.c re-reads its record at pagesize=1) -- never hold
 * a window across an intervening transaction. fn_edit() runs no transactions,
 * which is what makes "resolve, edit, write back" safe in st_name.c.
 */

#ifndef STATE_H
#define STATE_H

#include <stdbool.h>

#include <fujinet-fuji.h>
#include <fujinet-coleco.h>
#include <fujinet-bus-coleco.h>

#include "constants.h"

extern unsigned char state;

extern unsigned char cur;        /* selection index within the drawn page */
extern unsigned char nrows;      /* room rows the current page actually drew */
extern unsigned char page_full;  /* nonzero = more records past this page */
extern unsigned int  cur_off;    /* server-side record offset of this page */

extern char username[NAME_MAX + 1];

/* lobby.c */
void status_line(const char *s);            /* row 22, cleared first */
void legend_line(const char *s);            /* row 23, cleared first */
void fail(const char *what);                /* row 22 + FN_ERRCODE in hex */
void draw_frame(const char *subtitle);      /* cls + title (+ row-3 subtitle) */
void wait_frames(unsigned char n);          /* n vblanks, input ignored */

/* One screen per state; each draws itself, runs its own event loop, and
 * returns once it has set `state` to something else. */
void st_list(void);
void st_boot(void);                         /* boots the record at
                                               cur_off + cur; returns only on
                                               failure, state back to ST_LIST */

/* st_name.c -- blocking, no state of their own */
void name_resolve(void);                    /* appkey -> username, or editor */
void name_edit(void);                       /* OSK; caller redraws after */

#endif /* STATE_H */
