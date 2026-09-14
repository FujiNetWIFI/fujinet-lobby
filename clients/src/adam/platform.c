#ifdef __ADAM__

/**
 * @brief   Platform back-end for the Coleco Adam
 *
 * The Adam runs the shared lobby client. Everything here is either something
 * src/platform.h requires, or a thin bridge onto EOS and SmartKeys.
 */

#include <stdlib.h>
#include <conio.h>
#include <eos.h>
#include <smartkeys.h>

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

#endif /* __ADAM__ */
