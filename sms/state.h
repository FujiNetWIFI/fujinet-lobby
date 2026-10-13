/* state.h -- the shared globals and the seams between the screen modules.
 *
 * One invariant to keep when adding code: the reply window at $B000 is only
 * repainted by committing a transaction. A record is read out of it once,
 * into st_list.c's room cache, before the next READ; everything after that
 * -- the message window, the join -- works from the cache. fn_edit() runs no
 * transactions, which is what makes "resolve, edit, write back" safe in
 * st_name.c.
 */

#ifndef STATE_H
#define STATE_H

#include <stdbool.h>

#include <fujinet-fuji.h>
#include <fujinet-sms.h>
#include <fujinet-bus-sms.h>

#include "constants.h"

/* One drawn room, as much of its record as the message window and the join
 * need. The game's name is not kept: the header above the room drew it. */
typedef struct {
    unsigned char game_type;
    unsigned char players;
    unsigned char max_players;
    char server[SERVER_FIELD];
    char region[REGION_FIELD];
    char server_url[URL_FIELD];
    char client_url[URL_FIELD];
} Room;

extern unsigned char state;

extern unsigned char cur;        /* selection index within the drawn page */
extern unsigned char nrows;      /* room rows the current page actually drew */
extern Room rooms[LIST_ROWS];    /* those rooms, in draw order */
extern unsigned char rec_row[LIST_ROWS]; /* and the screen row of each */

extern char username[NAME_MAX + 1];

/* lobby.c */
void status_line(const char *s);            /* message line 1, typed out */
void status_now(const char *s);             /* line 1 at once, sound settled:
                                               say it before anything long */
void legend_line(const char *s);            /* message line 2 */
void fail(const char *what);                /* error buzz, line 1 + FN_ERRCODE */
void fail_code(const char *what, unsigned char code);
void draw_frame(const char *title);         /* main window cleared + retitled,
                                               message lines blank */
void wait_frames(unsigned char n);          /* n frames, input left alone */
void list_select(unsigned char i, bool on); /* the pointer onto/off room i */
/* Pause: pick one of a screen's actions from a Phantasy Star COMMAND pop-up;
 * returns the event that action stands for, or IN_NONE. */
unsigned char menu_pick(const char *const *names, const unsigned char *events,
                        unsigned char n);

/* One screen per state; each draws itself, runs its own event loop, and
 * returns once it has set `state` to something else. */
void st_list(void);
void st_boot(void);                         /* boots rooms[cur]; returns only
                                               on failure, state back to
                                               ST_LIST */

/* st_name.c -- blocking, no state of their own */
void name_resolve(void);                    /* appkey -> username, or editor */
void name_edit(void);                       /* keyboard; caller redraws after */

/* st_boot.c's progress window, from fujinet-config sms/st_boot.c */
void boot_begin(const volatile unsigned char *name);
                                            /* the progress window, titled with
                                               `name`; open it BEFORE SET PATH,
                                               which repaints the reply window */
void boot_cancel(void);                     /* close it again, on a failure */
void boot_mount_swap(void);                 /* MOUNT_IMAGE + progress + swap,
                                               window open; returns only on
                                               failure, window closed */

#endif /* STATE_H */
