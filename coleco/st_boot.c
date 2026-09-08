/* st_boot.c -- boot the selected room's game client: intv/st_boot.bas's
 * mount flow on top of fujinet-config/coleco/st_boot.c's mount-and-swap.
 *
 * Entered with cur_off + cur naming the chosen record's server-side offset.
 * The list screen never kept the record (RAM RULES), so the first step is to
 * re-fetch exactly that record at pagesize=1 and immediately make the three
 * bounded copies that must survive the window repaints the mount sequence
 * causes: game_type, the server URL (handed to the booted game through
 * appkey[game_type] -- the whole point), and the client URL (split into TNFS
 * host + path). Then, in main.c's mount() order: claim the LAST host slot
 * and always overwrite it fresh -- the other seven accumulate whatever other
 * clients left behind, so a "does it already match" check can be fooled by
 * leftover garbage (intv/st_boot.bas's documented policy) -- mount it, write
 * the appkey, stage the full path, and take the swap.
 *
 * Success ends in fuji_coleco_boot_swap(), which cold-starts the console
 * into the new image; there is no way back but a power cycle. Every failure
 * shows briefly on the status line and returns to the list -- there is no
 * keyboard to dismiss a message with.
 */

#include "fujidisp.h"
#include "fujiraw.h"
#include "netraw.h"
#include "state.h"

/* The only reply-to-RAM copies in the program besides username/prevgame;
 * both bounded to the wire fields' 65 bytes. */
static char url_copy[65];
static char boot_copy[65];
static unsigned char game_type;

/* MOUNT_IMAGE into device slot 0 and take the ROM swap. Verbatim from
 * fujinet-config/coleco/st_boot.c: the ACK comes back at once, the image
 * itself arrives asynchronously, and the cart publishes its progress. */
static void boot_mount_swap(void)
{
    unsigned char pct = 0xFF;

    status_line("MOUNTING...");
    if (!fuji_mount_disk_image(DEVICE_SLOT, MODE_READ)) {
        fail("EMOUNT");
        return;
    }

    status_line("LOADING");
    for (;;) {
        unsigned char st = fuji_coleco_boot_state();

        if (st == FUJI_COLECO_BOOT_READY)
            break;
        if (st == FUJI_COLECO_BOOT_FAILED) {
            disp_row_clear(STATUS_ROW);
            disp_at(1, STATUS_ROW, "ELOAD");
            disp_at_hex8(28, STATUS_ROW, fuji_coleco_boot_error());
            return;
        }
        if (fuji_coleco_boot_percent() != pct) {
            pct = fuji_coleco_boot_percent();
            disp_at_u16(24, STATUS_ROW, pct);
        }
    }

    status_line("BOOTING");
    fuji_coleco_boot_swap();    /* does not return */
}

void st_boot(void)
{
    volatile unsigned char *r;
    unsigned char i, host_start, slash;

    state = ST_LIST;            /* every failure path resumes the list */

    /* Re-fetch the chosen record. The list is live, so the record at this
     * offset can in principle have shifted since the page was drawn; the
     * game_type sanity check below is the guard that matters. */
    status_line("FETCHING...");
    if (!netraw_lobby_open((unsigned int)(cur_off + cur), 1)) {
        fail("ENET");
        wait_frames(120);
        return;
    }
    r = FN_REPLY;
    if (netraw_read(3) < 3 || r[0] < 1) {
        netraw_close();
        fail("EGONE");
        wait_frames(120);
        return;
    }
    if (netraw_read(REC_STRIDE) < REC_STRIDE) {
        netraw_close();
        fail("EREAD");
        wait_frames(120);
        return;
    }

    /* The bounded copies, before ANY further transaction (netraw_close
     * included) repaints the window. */
    r = FN_REPLY;
    game_type = r[REC_GAMETYPE];
    for (i = 0; i < 64; i++) {
        url_copy[i] = (char)r[REC_SERVERURL + i];
        boot_copy[i] = (char)r[REC_CLIENTURL + i];
    }
    url_copy[64] = 0;
    boot_copy[64] = 0;
    netraw_close();

    /* A record with game_type 0 has nothing valid to hand off via appkey --
     * refuse to boot it, same as main.c's mount(). */
    if (game_type == 0) {
        status_line("INVALID ENTRY");
        wait_frames(120);
        return;
    }

    /* Strip a leading scheme ("tnfs://", ...) if present -- assume TNFS
     * otherwise -- then split host from path at the first '/'. */
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
        status_line("INVALID CLIENT URL");
        wait_frames(120);
        return;
    }

    status_line("SETTING HOST...");
    boot_copy[slash] = 0;       /* NUL-terminate the host in place */
    if (!fnraw_write_host_slot(HOST_SLOTS - 1, boot_copy + host_start)) {
        fail("EHOSTS");
        wait_frames(120);
        return;
    }
    boot_copy[slash] = '/';     /* the path keeps its leading '/' */

    status_line("MOUNTING HOST...");
    if (!fuji_mount_host_slot(HOST_SLOTS - 1)) {
        fail("EHOST");
        wait_frames(120);
        return;
    }

    /* The URL handoff: the booted game reads appkey[game_type] to know which
     * server to connect to. Failure is tolerated -- boot anyway, matching
     * intv. */
    if (fnraw_appkey_open(game_type, 1)) {
        fnraw_appkey_write(url_copy);
        fnraw_appkey_close();
    }

    status_line("SET PATH...");
    if (!fnraw_set_device_path(DEVICE_SLOT, HOST_SLOTS - 1, MODE_READ,
                               boot_copy + slash)) {
        fail("EPATH");
        wait_frames(120);
        return;
    }

    boot_mount_swap();          /* only failure comes back */
    wait_frames(120);
}
