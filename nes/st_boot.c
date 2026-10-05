/* st_boot.c -- boot the selected room's game client.
 *
 * The join is ../coleco/st_boot.c's, in clients/src/main.c's mount() order:
 * claim the LAST host slot and always overwrite it fresh (the other seven
 * accumulate whatever other clients left behind, so a "does it already
 * match" check can be fooled by leftover garbage -- intv/st_boot.bas's
 * documented policy), mount it, hand the room's server URL to the game
 * through appkey[game_type], stage the full path, and mount the image. The
 * room comes from st_list.c's cache, so unlike the ColecoVision there is no
 * re-fetch first.
 *
 * The wait is fujinet-config nes/st_boot.c's LOADING screen: what is being
 * loaded and from where, a bar the cartridge's own progress fills, and the
 * bytes received so far against the image's size. Success ends in
 * fuji_nes_boot(), which jumps into the cartridge's loader ROM; the loader
 * copies the image into the SRAMs and cold-starts it. Every failure says
 * why on the status row and returns to the list.
 */

#include <stdlib.h>
#include <string.h>

#include "fujidisp.h"
#include "fujiin.h"
#include "fujiraw.h"
#include "sfx.h"
#include "state.h"

#define GAME_ROW   5
#define ROOM_ROW   6
#define HOST_ROW   8
#define PATH_ROW   9
#define BAR_BOX_T  12
#define BAR_ROW    13
#define BAR_L      4            /* the box's left edge; the bar starts one in */
#define BAR_CELLS  22
#define PCT_ROW    16
#define PCT_COL    14
#define BYTES_ROW  18

/* The FujiNet answers MOUNT_IMAGE only once the whole image is on the cart,
 * and the cart gives it 60 seconds; wait a little longer than that so its
 * own timeout is the one that surfaces. */
#define MOUNT_FRAMES 3900u

/* How long a failure stays up before the list comes back. */
#define FAIL_FRAMES  150

/* The client URL, split in place: host NUL-terminated at `slash`, which is
 * then put back so the path keeps its leading '/'. */
static char boot_copy[URL_FIELD];

/* ---- the LOADING screen (fujinet-config nes/st_boot.c) ------------------ */

/* "got/total BYTES": the total and the layout are fixed once the image
 * stream opens, so each update rewrites only the right-aligned count. */
static char tot_txt[9];
static unsigned char tot_len, got_col;
static unsigned long shown_tot, shown_got;

static void boot_bytes(unsigned long got, unsigned long tot)
{
    char txt[9];
    unsigned char n;

    if (tot != shown_tot) {
        shown_tot = tot;
        shown_got = ~0ul;
        disp_row_clear(BYTES_ROW);
        if (tot == 0)
            return;
        ultoa(tot, tot_txt, 10);
        tot_len = (unsigned char)strlen(tot_txt);
        got_col = (unsigned char)((DISP_COLS - (tot_len * 2 + 7)) / 2);
        disp_at((unsigned char)(got_col + tot_len), BYTES_ROW, "/");
        disp_at((unsigned char)(got_col + tot_len + 1), BYTES_ROW, tot_txt);
        disp_at((unsigned char)(got_col + tot_len * 2 + 1), BYTES_ROW,
                " BYTES");
    }
    if (tot == 0 || got == shown_got)
        return;
    shown_got = got;
    ultoa(got, txt, 10);
    n = (unsigned char)strlen(txt);
    if (n > tot_len)
        n = tot_len;
    disp_at((unsigned char)(got_col + tot_len - n), BYTES_ROW, txt);
    while (n < tot_len)
        disp_char((unsigned char)(got_col + tot_len - 1 - n++), BYTES_ROW,
                  ' ');
}

static void boot_pct(unsigned char pct)
{
    disp_bar((unsigned char)(BAR_L + 1), BAR_ROW, BAR_CELLS, pct, 100);
    disp_at(PCT_COL, PCT_ROW, "    ");
    disp_at_u16(PCT_COL, PCT_ROW, pct);
    disp_at((unsigned char)(PCT_COL + (pct >= 100 ? 3 : pct >= 10 ? 2 : 1)),
            PCT_ROW, "%");
}

static void clipped(unsigned char col, unsigned char row, const char *s)
{
    unsigned char i;

    for (i = 0; col + i <= IN_R && s[i] != 0; i++)
        disp_char((unsigned char)(col + i), row, s[i]);
}

static void boot_screen(const Room *rm, const char *host, const char *path)
{
    disp_frame("LOADING");
    clipped(IN_L, GAME_ROW, rm->game);
    clipped(IN_L + 1, ROOM_ROW, rm->server);
    disp_at(IN_L, HOST_ROW, "FROM");
    clipped(IN_L + 5, HOST_ROW, host);
    clipped(IN_L + 5, PATH_ROW, path);
    disp_box(BAR_L, BAR_BOX_T, (unsigned char)(BAR_L + BAR_CELLS + 1),
             (unsigned char)(BAR_BOX_T + 2), NULL);
    disp_bar_reset();
    boot_pct(0);
    shown_tot = 0;
    disp_row_clear(BYTES_ROW);
}

static void load_error(unsigned char code)
{
    disp_row_clear(STATUS_ROW);
    disp_at(IN_L, STATUS_ROW, "?LOAD ERROR");
    disp_at_hex8(IN_L + 12, STATUS_ROW, code);
    sfx_beep();
}

/* MOUNT_IMAGE into device slot 0, the progress, and the boot. Verbatim from
 * fujinet-config nes/st_boot.c but for the "OK", which goes on the status
 * row here -- the legend row is the frame's bottom edge. */
static void boot_mount_swap(void)
{
    unsigned char pct = 0, want, st;
    bool acked = false;
    unsigned int frames = 0;

    /* Depending on the FujiNet, the image is pushed to the cartridge before
     * MOUNT_IMAGE is answered (so progress is only visible while that reply
     * is still outstanding) or after it. Either way: start the mount, then
     * watch the cart's boot registers -- and the reply -- until the image
     * is READY. The cart clears those registers when the mount starts. */
    status_line("LOADING...");
    want = fnraw_mount_start(DEVICE_SLOT, MODE_READ);
    for (;;) {
        unsigned char now;

        st = fuji_nes_boot_state();
        if (st == FUJI_NES_BOOT_FAILED) {
            load_error(fuji_nes_boot_error());
            return;
        }
        if (!acked && FN_ACKSEQ == want) {
            acked = true;
            if (!fnraw_reply_ok()) {
                fail("MOUNT");
                return;
            }
        }
        if (acked && st == FUJI_NES_BOOT_READY)
            break;
        if (++frames > MOUNT_FRAMES) {
            load_error(st);
            return;
        }
        now = fuji_nes_boot_percent();
        if (now > 100)
            now = 100;
        if (now != pct) {
            pct = now;
            boot_pct(pct);
        }
        boot_bytes(fuji_nes_boot_got(), fuji_nes_boot_total());
        wait_frames(1);
    }

    boot_pct(100);
    boot_bytes(fuji_nes_boot_total(), fuji_nes_boot_total());
    status_line("BOOTING");
    disp_at(IN_R - 1, STATUS_ROW, "OK");
    sfx_accept();
    wait_frames(10);            /* let the screen catch up */
    fuji_nes_boot();            /* does not return */
}

/* ---- the join ------------------------------------------------------------ */

static void refuse(const char *why)
{
    status_line(why);
    sfx_beep();
    wait_frames(FAIL_FRAMES);
}

void st_boot(void)
{
    const Room *rm = &rooms[cur];
    unsigned char i, host_start, slash;

    state = ST_LIST;            /* every failure path resumes the list */

    /* A record with game_type 0 has nothing valid to hand off via appkey --
     * refuse to boot it, same as main.c's mount(). */
    if (rm->game_type == 0) {
        refuse("?NOT A GAME ROOM");
        return;
    }

    /* Strip a leading scheme ("tnfs://", ...) if present -- assume TNFS
     * otherwise -- then split host from path at the first '/'. */
    strcpy(boot_copy, rm->client_url);
    host_start = 0;
    for (i = 0; boot_copy[i] != 0; i++) {
        if (boot_copy[i] == ':' && boot_copy[i + 1] == '/'
            && boot_copy[i + 2] == '/') {
            host_start = (unsigned char)(i + 3);
            break;
        }
    }
    slash = 0xFF;
    for (i = host_start; boot_copy[i] != 0; i++) {
        if (boot_copy[i] == '/') {
            slash = i;
            break;
        }
    }
    if (slash == 0xFF || slash == host_start) {
        refuse("?BAD CLIENT URL");
        return;
    }
    boot_copy[slash] = 0;       /* NUL-terminate the host in place */

    boot_screen(rm, boot_copy + host_start, boot_copy + slash + 1);

    status_line("SETTING HOST...");
    if (!fnraw_write_host_slot(HOST_SLOTS - 1, boot_copy + host_start)) {
        fail("HOSTS");
        wait_frames(FAIL_FRAMES);
        return;
    }
    boot_copy[slash] = '/';     /* the path keeps its leading '/' */

    status_line("MOUNTING HOST...");
    if (!fuji_mount_host_slot(HOST_SLOTS - 1)) {
        fail("HOST");
        wait_frames(FAIL_FRAMES);
        return;
    }

    /* The URL handoff: the booted game reads appkey[game_type] to know which
     * server to connect to. Failure is tolerated -- boot anyway, matching
     * intv. Before the mount: nothing after MOUNT_IMAGE runs on this side
     * but the progress loop. */
    if (fnraw_appkey_open(rm->game_type, 1)) {
        fnraw_appkey_write(rm->server_url);
        fnraw_appkey_close();
    }

    status_line("SETTING PATH...");
    if (!fnraw_set_device_path(DEVICE_SLOT, HOST_SLOTS - 1, MODE_READ,
                               boot_copy + slash)) {
        fail("PATH");
        wait_frames(FAIL_FRAMES);
        return;
    }

    boot_mount_swap();          /* only failure comes back */
    wait_frames(FAIL_FRAMES);
}
