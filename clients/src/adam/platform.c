#ifdef __ADAM__

/**
 * @brief   Platform back-end for the Coleco Adam
 *
 * The Adam runs the shared lobby client. Everything here is either something
 * src/platform.h requires, or a thin bridge onto EOS and SmartKeys.
 */

#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <eos.h>
#include <smartkeys.h>
#include <video/tms99x8.h>

#include "qr.h"
#include "vars.h"

static GameControllerData cont;

/**
 * @brief Read both joysticks as one.
 *
 * EOS's decode table reports directions as 1=up 2=right 4=down 8=left. The
 * shared client expects the layout in include/adam/joystick.h, which is the
 * one cc65 and the CoCo use, so the bits are transposed here.
 *
 * Deliberately no auto-repeat: io.c's readCommonInput() already debounces and
 * repeats, and a second timer here would fight it.
 */
unsigned char readJoystick(void)
{
  unsigned char raw, value;

  eos_read_game_controller(0x03, &cont);

  raw = cont.joystick1 | cont.joystick2;

  value = 0;
  if (raw & 1)
    value |= 1;  /* up */
  if (raw & 4)
    value |= 2;  /* down */
  if (raw & 8)
    value |= 4;  /* left */
  if (raw & 2)
    value |= 8;  /* right */

  if (cont.joystick1_button_left || cont.joystick2_button_left)
    value |= 16;
  if (cont.joystick1_button_right || cont.joystick2_button_right)
    value |= 32;

  return value;
}

/**
 * @brief Platform specific initialization
 *
 * Mirrors screen_init() in fujinet-config's src/adam/screen.c: sound, then
 * GRAPHICS II via SmartKeys, then arm the asynchronous keyboard read that
 * conio.c's adam_kbhit() drives.
 */
void initialize(void)
{
  smartkeys_sound_init();
  smartkeys_set_mode();

  /* Content rows are black on white, as in CONFIG. banner() flips this to
     white on blue for the title row. */
  adam_set_normal(BLACK, WHITE);

  eos_start_read_keyboard();

  smartkeys_sound_play(SOUND_POSITIVE_CHIME);
}

/**
 * @brief Wait for vertical sync
 *
 * video/tms99x8.h has no vsync entry point, and polling the VDP status
 * register would race EOS, which owns the interrupt. waitvsync() is only ever
 * used to pace the event loop, so a frame-length sleep is enough -- the same
 * approach src/msdos/platform.c takes.
 */
void waitvsync(void)
{
  msleep(17);
}

/// @brief Reboot the system to run mounted disk
void reboot(void)
{
  /* Same as system_boot() in fujinet-config's src/adam/system.c. */
  eos_init();
}

/*
 * QR rendering: TMS9918 GRAPHICS II.
 *
 * Unlike every other platform here, the Adam has a real bitmap -- 256x192 with
 * one bit per pixel -- so the symbol is drawn at true 1:1 with square modules.
 * No semigraphics cells, no lo-res blocks, no reliance on the border colour to
 * carry the quiet zone into overscan.
 *
 * At 5 pixels per module a 21-module symbol is 105x105. Centred in the 168
 * scanlines above the SmartKeys strip that leaves 75px of white either side
 * and ~32px above and below -- 6 modules of quiet zone against the 4 a scanner
 * needs, so qr_get()'s out-of-range-reads-as-light behaviour is not even
 * exercised.
 *
 * Attribute 0x1F is fg 1 (black) on bg 15 (white), so a set pattern bit is a
 * dark module: the same polarity as every platform except the CoCo.
 */

#define QR_SCALE      5
#define QR_SIZE       (QR_MODULES * QR_SCALE)          /* 105 */
#define QR_ORIGIN_X   ((MODE2_WIDTH - QR_SIZE) / 2)    /* 75  */

/* Character rows the lobby owns; rows 21-23 are the SmartKeys strip. */
#define QR_ROWS       21
#define QR_AREA_H     (QR_ROWS * 8)                    /* 168 */
#define QR_ORIGIN_Y   ((QR_AREA_H - QR_SIZE) / 2)      /* 31  */

/* Black on white. */
#define QR_ATTR       0x1F

/* One character row of the pattern table: 32 columns x 8 scanlines, in the
   order the VDP stores them, so a row is a single vdp_vwrite. */
static unsigned char rowbuf[256];

/* One scanline of the symbol, rebuilt only when the module row changes. */
static unsigned char bits[32];

static void qr_build_module_row(unsigned char my)
{
  unsigned char mx, px, x0;

  memset(bits, 0, sizeof(bits));

  for (mx = 0; mx < QR_MODULES; mx++)
    {
      if (!qr_get(mx, my))
        continue;

      x0 = QR_ORIGIN_X + mx * QR_SCALE;
      for (px = x0; px < x0 + QR_SCALE; px++)
        bits[px >> 3] |= 0x80 >> (px & 7);
    }
}

void qr_draw(void)
{
  unsigned char r, ly, i, my, last_my;
  unsigned int y;

  /* Deliberately not bracketed with vdp_blank()/vdp_noblank(). The repaint is
     21 vdp_vwrite calls, far too quick to tear, and getting the sense of those
     two the wrong way round would leave the code invisible. */

  /* Paint the whole lobby area white. Everything outside the symbol is then
     quiet zone, and any leftover list text is cleared by the pattern writes
     below. */
  vdp_vfill(MODE2_ATTR, QR_ATTR, (unsigned int) QR_ROWS * 256);

  last_my = 0xFF;

  for (r = 0; r < QR_ROWS; r++)
    {
      memset(rowbuf, 0, sizeof(rowbuf));

      for (ly = 0; ly < 8; ly++)
        {
          y = ((unsigned int) r << 3) + ly;

          if (y < QR_ORIGIN_Y)
            continue;

          my = (unsigned char) ((y - QR_ORIGIN_Y) / QR_SCALE);
          if (my >= QR_MODULES)
            continue;

          /* y only ever increases, so this rebuilds 21 times, not 105. */
          if (my != last_my)
            {
              qr_build_module_row(my);
              last_my = my;
            }

          for (i = 0; i < 32; i++)
            rowbuf[((unsigned int) i << 3) + ly] = bits[i];
        }

      vdp_vwrite(rowbuf, (unsigned int) r << 8, 256);
    }

  smartkeys_display(NULL, NULL, NULL, NULL, NULL, NULL);
  smartkeys_status("  SCAN TO CHAT\n  PRESS ANY KEY TO PLAY");
  smartkeys_sound_play(SOUND_DOUBLE_CHIME);
}

/// @brief Undo whatever qr_draw() did to the display.
void qr_restore(void)
{
  smartkeys_set_mode();
  adam_set_normal(BLACK, WHITE);
}

/// @brief Block until a key is pressed, and return it.
char qr_wait_key(void)
{
  return (char) adam_cgetc();
}

#endif /* __ADAM__ */
