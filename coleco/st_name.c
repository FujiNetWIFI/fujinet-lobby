/* st_name.c -- the shared FujiNet lobby username (appkey creator=1/app=1/
 * key=0), the slot the C clients and every game client read. The port of
 * intv/st_name.bas onto fn_edit(): resolve from the appkey with one retry (a
 * cold RP2040/ESP32 link right after the mailbox comes up can time out the
 * very first transaction), validate 2-8 chars of A-Z/0-9 (the slot is shared
 * by every FujiNet client, so it can hold junk left by something else, and
 * an unescaped character would corrupt a game's query string), and fall back
 * to the on-screen keyboard when empty or invalid.
 */

#include <string.h>

#include "fujiedit.h"
#include "fujiraw.h"
#include "state.h"

/* Fold a-z to A-Z in place; true if `s` is 2-8 chars of A-Z/0-9. */
static bool fold_valid(char *s)
{
    unsigned char len = (unsigned char)strlen(s);
    unsigned char i;
    char c;

    if (len < 2 || len > NAME_MAX)
        return false;
    for (i = 0; i < len; i++) {
        c = s[i];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
            s[i] = c;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

/* Run the editor over a copy of the current name. Only copies back and
 * writes the appkey on a valid accept; cancel leaves the stored name
 * untouched, and a cancel with nothing stored falls back to a placeholder so
 * the rest of the program always has something to draw. */
void name_edit(void)
{
    strcpy(fn_entry, username);
    for (;;) {
        if (!fn_edit("ENTER YOUR NAME", NAME_MAX))
            break;
        if (fold_valid(fn_entry)) {
            strcpy(username, fn_entry);
            if (fnraw_appkey_open(0, 1)) {
                fnraw_appkey_write(username);
                fnraw_appkey_close();
            }
            return;
        }
        /* Invalid (too short, or a folded char outside A-Z/0-9): re-run the
         * editor over the attempt so it can be fixed rather than retyped. */
    }
    if (username[0] == 0)
        strcpy(username, "COLECO");
}

void name_resolve(void)
{
    volatile unsigned char *r;
    unsigned int len;
    unsigned char i, try;
    bool opened = false;

    draw_frame(NULL);
    status_line("READING NAME...");

    username[0] = 0;
    for (try = 0; try < 2 && !opened; try++)
        opened = fnraw_appkey_open(0, 0);
    if (opened) {
        len = fnraw_appkey_read();
        if (len > NAME_MAX)
            len = NAME_MAX;
        r = FN_REPLY + 2;       /* past the 2-byte length prefix */
        for (i = 0; i < len; i++)
            username[i] = (char)r[i];
        username[len] = 0;
        fnraw_appkey_close();
        if (fold_valid(username))
            return;
    }

    username[0] = 0;
    name_edit();
}
