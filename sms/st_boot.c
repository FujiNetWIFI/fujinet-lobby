/* st_boot.c -- join the selected room: hand its server URL to the game,
 * mount its client into device slot 0, watch the image arrive, and take the
 * ROM swap.
 *
 * The join is the nes branch's nes/st_boot.c (itself ../coleco/st_boot.c),
 * in clients/src/main.c's mount() order: claim the LAST host slot and always
 * overwrite it fresh (the other seven accumulate whatever other clients left
 * behind, so a "does it already match" check can be fooled by leftover
 * garbage -- intv/st_boot.bas's documented policy), mount it, hand the
 * room's server URL to the game through appkey[game_type], stage the full
 * path, and mount the image. The room comes from st_list.c's cache, so there
 * is no re-fetch first.
 *
 * The wait is fujinet-config sms/st_boot.c's progress window, taken whole:
 * every server pushes the whole image to the cartridge INSIDE the
 * MOUNT_IMAGE transaction, before it ACKs. Committed through the lib, the
 * mount would sit silent for the entire load and come back already READY.
 * So it is committed by hand (fnraw_mount_start) and this watches the cart's
 * boot registers on the status page while the ACK is outstanding, drawing
 * them into a pop-up window. Until the ACK arrives NOTHING may be written to
 * the mailbox -- a register write would only queue up behind the
 * transaction; reading the status page and polling the VDP are both fine.
 * Success ends in fuji_sms_boot(), which hands the console to the cart's
 * loader page; the loader copies the image into the SRAM and starts it.
 * Every failure says why in the message window and returns to the list.
 */

#include <string.h>

#include "fujidisp.h"
#include "fujisnd.h"
#include "fujiin.h"
#include "fujiraw.h"
#include "state.h"

/* The window, inside the main window, its lines on every other row. */
#define WIN_X0     2
#define WIN_Y0     4
#define WIN_X1     29
#define WIN_Y1     12
#define IN_COL     3            /* first interior column */
#define IN_W       26
#define PHASE_ROW  5
#define NAME_ROW   7
#define BAR_ROW    9
#define BYTES_ROW  11
#define BAR_COL    4            /* first cell; the caps sit either side */
#define BAR_CELLS  20
#define PCT_COL    25           /* "100%" in 25-28 */

#define IDLE_GRACE 60           /* ACKed but nothing pushed: not a ROM */
#define QUIET_MAX  4200         /* 70 s without a sign of life: past the
                                   cart's own 60 s per-frame deadline */

static char line[IN_W + 1];     /* no arrays as locals: see constants.h */
static char pct_text[5];        /* " 42%" */

/* What the window shows (shown_*) and what it should show next (want_*).
 * The arithmetic is done while the screen is being drawn and only the VRAM
 * writes are left for the moment after the frame flag, so they land in the
 * vertical blank instead of tearing a row the VDP is half-way through. And
 * only what changed is written: a full repaint would be most of a frame. */
static unsigned int shown_px, want_px;
static unsigned char shown_pct, want_pct;
static unsigned int shown_k, want_k;
static bool bytes_drawn;        /* the whole byte line is on screen */

static void phase(const char *word)
{
    disp_at(IN_COL, PHASE_ROW, word);
}

static void bar_cell(unsigned char i, unsigned int px)
{
    unsigned int from = (unsigned int)i * 8;
    unsigned char n = 0;

    if (px > from)
        n = (unsigned char)(px - from >= 8 ? 8 : px - from);
    disp_tile((unsigned char)(BAR_COL + i), BAR_ROW, (unsigned char)(T_BAR0 + n), 0);
}

/* Right to left into `line` ending at `end`; returns where it starts. */
static unsigned char put_u16(unsigned char end, unsigned int v)
{
    do {
        line[--end] = (char)('0' + v % 10);
        v /= 10;
    } while (v && end);
    return end;
}

/* The byte line is "  57K OF 512K": the count right-aligned in a fixed
 * four-character field, so after the whole line has been drawn once only
 * those four characters ever change. */
#define K_W 4

static void build_count(unsigned int got_k)
{
    unsigned char s = put_u16(K_W, got_k);

    while (s)
        line[--s] = ' ';
}

static void build_bytes(unsigned int got_k, unsigned int tot_k)
{
    unsigned char i, n = K_W, s;
    static const char of[] = "K OF ";

    build_count(got_k);
    for (i = 0; of[i]; i++)
        line[n++] = of[i];
    s = put_u16(IN_W, tot_k);
    while (s < IN_W)
        line[n++] = line[s++];
    line[n++] = 'K';
    while (n < IN_W)
        line[n++] = ' ';
    line[IN_W] = '\0';
}

/* Bar, percent and byte count, all from the ONE sample: the cart's counters
 * move fast next to the console's redraw (the desktop app pushes a 512K
 * image in a few frames), so mixing the percent register with a byte count
 * read a moment later would put two different instants in one window. The
 * percent comes from the bar's own pixels, so the two always agree. */
static void prepare(unsigned long got, unsigned long tot)
{
    unsigned char pct;

    want_px = 0;
    if (tot) {
        if (got > tot)
            got = tot;
        want_px = (unsigned int)(got * (BAR_CELLS * 8) / tot);
    }
    pct = (unsigned char)(want_px * 5 / 8);         /* 160 px = 100% */
    if (pct != want_pct) {
        want_pct = pct;
        pct_text[0] = (char)(pct >= 100 ? '1' : ' ');
        pct_text[1] = (char)(pct >= 10 ? '0' + (pct / 10) % 10 : ' ');
        pct_text[2] = (char)('0' + pct % 10);
        pct_text[3] = '%';
        pct_text[4] = '\0';
    }
    if ((unsigned int)(got >> 10) != want_k) {
        want_k = (unsigned int)(got >> 10);
        if (bytes_drawn)
            build_count(want_k);
        else
            build_bytes(want_k, (unsigned int)(tot >> 10));
    }
}

/* Only the VRAM writes, and only for what changed: on the bar, the cells
 * between the old edge and the new one. */
static void apply(void)
{
    unsigned int lo, hi;
    unsigned char i;

    if (want_px != shown_px) {
        lo = want_px < shown_px ? want_px : shown_px;
        hi = want_px < shown_px ? shown_px : want_px;
        for (i = (unsigned char)(lo / 8); i < BAR_CELLS && (unsigned int)i * 8 < hi; i++)
            bar_cell(i, want_px);
        shown_px = want_px;
    }
    if (want_pct != shown_pct) {
        shown_pct = want_pct;
        disp_at(PCT_COL, BAR_ROW, pct_text);
    }
    if (want_k != shown_k) {
        shown_k = want_k;
        if (bytes_drawn) {
            line[K_W] = '\0';
            disp_at(IN_COL, BYTES_ROW, line);
            line[K_W] = 'K';
        } else {
            disp_at(IN_COL, BYTES_ROW, line);
            bytes_drawn = true;
        }
    }
}

void boot_begin(const volatile unsigned char *name)
{
    unsigned char n = 0, i, k;

    msg_put(MSG_LINE1, "PLEASE WAIT.");
    msg_put(MSG_LINE2, "");
    ovl_open(WIN_X0, WIN_Y0, WIN_X1, WIN_Y1);
    phase("MOUNTING");

    /* The tail of the name, if it is long: the end is the interesting part. */
    while (n < FULLLEN && name[n] != 0)
        n++;
    i = (unsigned char)(n > IN_W ? n - IN_W : 0);
    for (k = 0; i + k < n; k++)
        disp_char((unsigned char)(IN_COL + k), NAME_ROW, (char)name[i + k]);

    disp_tile(BAR_COL - 1, BAR_ROW, T_BARCAP, 0);
    disp_tile(BAR_COL + BAR_CELLS, BAR_ROW, T_BARCAP, A_HFLIP);
    for (k = 0; k < BAR_CELLS; k++)
        bar_cell(k, 0);
    shown_px = want_px = 0;
    shown_pct = want_pct = 0xFF;
    shown_k = want_k = 0xFFFF;
    bytes_drawn = false;
    prepare(0, 0);              /* "  0%"; the byte line waits for a size */
    shown_k = want_k;
    apply();
}

void boot_cancel(void)
{
    ovl_close();
    in_init();                  /* drop presses made while it loaded */
}

void boot_mount_swap(void)
{
    unsigned char want, st, now, last, last_st = 0xFF, idle = 0;
    unsigned long got, tot, last_got = 0xFFFFFFFFUL;
    unsigned int quiet = 0;
    bool acked = false;

    want = fnraw_mount_start(DEVICE_SLOT, MODE_READ);
    last = in_frames();
    for (;;) {
        now = in_frames();      /* ticks the sound and the cursor */
        if (now == last)
            continue;
        last = now;
        apply();                /* in the vertical blank: see prepare() */

        st = fuji_sms_boot_state();
        if (!acked && FN_ACKSEQ == want) {
            acked = true;
            if (!fnraw_reply_ok()) {
                unsigned char code = FN_ERRCODE;

                boot_cancel();
                fail_code("EMOUNT", code);
                return;
            }
        }

        /* The outcome is read only once the ACK is in. Before it, a FAILED
         * may be left over from an earlier attempt (the cart resets the
         * registers only when it takes up the SEQ write), and leaving early
         * would send the browser's next transaction out behind this one --
         * its commit would then take this mount's ACK for its own. Every
         * failure ends in an ACK or NAK soon enough. */
        if (acked && st == FUJI_SMS_BOOT_FAILED) {
            boot_cancel();
            fail_code("ELOAD", fuji_sms_boot_error());
            return;
        }
        if (acked && st == FUJI_SMS_BOOT_READY)
            break;
        if (acked && st == FUJI_SMS_BOOT_IDLE && ++idle > IDLE_GRACE) {
            /* Mounted as a plain disk: the server pushed nothing. */
            boot_cancel();
            snd_play(SND_ERROR);
            status_line("NOT A CARTRIDGE IMAGE.");
            return;
        }

        if (st != last_st) {
            last_st = st;
            quiet = 0;
            if (st == FUJI_SMS_BOOT_XFER)
                phase("LOADING ");
        }
        if (st == FUJI_SMS_BOOT_XFER) {
            got = fnraw_boot_bytes(FN_BOOTGOT);
            if (got != last_got) {
                last_got = got;
                quiet = 0;
                prepare(got, fnraw_boot_bytes(FN_BOOTTOT));
            }
        }
        if (++quiet > QUIET_MAX) {
            boot_cancel();
            fail_code("ETIMEOUT", FN_ETIMEOUT);
            return;
        }
    }

    /* Whatever the window last showed, it ends full. Then the chime, the
     * scene fades out the way Phantasy Star leaves one, and the PSG goes
     * quiet so nothing carries into the game. */
    tot = fnraw_boot_bytes(FN_BOOTTOT);
    prepare(tot, tot);
    in_wait(1);                 /* into the vertical blank */
    apply();
    phase("READY   ");
    snd_play(SND_READY);
    while (snd_busy())
        in_frames();
    fade_out();
    snd_init();
    fuji_sms_boot();            /* does not return */
}

/* ---- the join ------------------------------------------------------------ */

/* How long a failure stays up before the list comes back. */
#define FAIL_FRAMES  150

/* The client URL, split in place: host NUL-terminated at `slash`, which is
 * then put back so the path keeps its leading '/'. */
static char boot_copy[URL_FIELD];

static void refuse(const char *why)
{
    snd_play(SND_ERROR);
    status_line(why);
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
        refuse("NOT A GAME ROOM.");
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
        refuse("BAD CLIENT URL.");
        return;
    }
    boot_copy[slash] = 0;       /* NUL-terminate the host in place */

    status_now("SETTING HOST...");
    if (!fnraw_write_host_slot(HOST_SLOTS - 1, boot_copy + host_start)) {
        fail("EHOSTS");
        wait_frames(FAIL_FRAMES);
        return;
    }
    boot_copy[slash] = '/';     /* the path keeps its leading '/' */

    status_now("MOUNTING HOST...");
    if (!fuji_mount_host_slot(HOST_SLOTS - 1)) {
        fail("EHOST");
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

    /* The window takes the room's name before SET PATH runs. */
    boot_begin((const volatile unsigned char *)rm->server);
    if (!fnraw_set_device_path(DEVICE_SLOT, HOST_SLOTS - 1, MODE_READ,
                               boot_copy + slash)) {
        boot_cancel();
        fail("EPATH");
        wait_frames(FAIL_FRAMES);
        return;
    }

    boot_mount_swap();          /* only failure comes back */
    wait_frames(FAIL_FRAMES);
}
