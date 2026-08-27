#ifdef BUILD_MSDOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <i86.h>
#include <conio.h>
#include <dos.h>
#include <direct.h>
#include <graph.h>
#include <fujinet-fuji.h>

#include "qr.h"

#define FUJI_SIGNATURE "FUJI"

typedef struct {
    uint8_t query;
    char    signature[4];
    uint8_t unit;
} fuji_ioctl_query;

static int find_slot_drive(int slot)
{
    int drive;
    union REGS regs;
    struct SREGS sregs;
    fuji_ioctl_query query;

    for (drive = 3; drive <= 26; drive++) {
        memset(&query, 0, sizeof(query));
        regs.h.ah = 0x44;
        regs.h.al = 0x04;
        regs.h.bl = (unsigned char)drive;
        regs.w.cx = sizeof(query);
        regs.x.dx = FP_OFF(&query);
        sregs.ds  = FP_SEG(&query);
        int86x(0x21, &regs, &regs, &sregs);
        if (!(regs.x.cflag & INTR_CF) &&
            memcmp(query.signature, FUJI_SIGNATURE, 4) == 0 &&
            query.unit == slot)
            return drive;
    }
    return -1;
}

void initialize(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    set_video_mode(VIDEO_MODE_40COL);
    set_screen_bg_blue();
    cursor(0);
}

void waitvsync(void)
{
    delay(17);
}

void reboot(void)
{
    int drive;
    unsigned int total;

    cursor(1);
    set_video_mode(VIDEO_MODE_80COL);
    restore_screen_bg();

    drive = find_slot_drive(0);
    if (drive < 0)
        return;

    _dos_setdrive(drive, &total);
    chdir("\\");

    spawnlp(P_OVERLAY, "COMMAND.COM", "COMMAND.COM", "/C", "AUTOEXEC.BAT", NULL);
}

/*
 * QR rendering: mode 13h, 320x200 at 256 colours.
 *
 * The only platform here with a real framebuffer, so the symbol is drawn at
 * whatever module size fits rather than being pinned to the character grid.
 * 6 pixels a module gives a 126x126 symbol with a 24 pixel quiet zone on every
 * side, centred in 320x200, and pixels are close enough to square that there is
 * no distortion to speak of.
 */

#define QR_SCALE        6
#define QR_QUIET_PX     (QR_QUIET * QR_SCALE)
#define QR_SIZE_PX      (QR_MODULES * QR_SCALE)
#define QR_SCREEN_W     320
#define QR_SCREEN_H     200
#define QR_ORIGIN_X     ((QR_SCREEN_W - QR_SIZE_PX) / 2)
#define QR_ORIGIN_Y     ((QR_SCREEN_H - QR_SIZE_PX) / 2)

/* Mode 13h default palette: 15 is white, 0 is black. */
#define QR_COLOUR_LIGHT 15
#define QR_COLOUR_DARK  0

void qr_draw(void)
{
    uint8_t x, y;
    short px, py;

    _setvideomode(_MRES256COLOR);

    /* White page, so the quiet zone is simply the unpainted margin. */
    _setcolor(QR_COLOUR_LIGHT);
    _rectangle(_GFILLINTERIOR, 0, 0, QR_SCREEN_W - 1, QR_SCREEN_H - 1);

    _setcolor(QR_COLOUR_DARK);
    for (y = 0; y < QR_MODULES; y++) {
        for (x = 0; x < QR_MODULES; x++) {
            if (!qr_get(x, y))
                continue;
            px = QR_ORIGIN_X + (short) x * QR_SCALE;
            py = QR_ORIGIN_Y + (short) y * QR_SCALE;
            _rectangle(_GFILLINTERIOR, px, py, px + QR_SCALE - 1, py + QR_SCALE - 1);
        }
    }

    _settextposition(24, 10);
    _outtext("SCAN TO CHAT - ANY KEY");
}

void qr_restore(void)
{
    /* Back to what initialize() set up, not the 80 column mode reboot() uses. */
    set_video_mode(VIDEO_MODE_40COL);
    set_screen_bg_blue();
    cursor(0);
}

char qr_wait_key(void)
{
    /* conio.c's cgetc() returns 0 rather than blocking when no key is
       waiting, so it cannot be used to wait for one. */
    while (!kbhit())
        ;
    return (char) getch();
}

#endif /* BUILD_MSDOS */
