#ifdef BUILD_MSDOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <i86.h>
#include <conio.h>
#include <dos.h>
#include <direct.h>
#include <fujinet-fuji.h>

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

#endif /* BUILD_MSDOS */
