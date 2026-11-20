#ifdef __ATARI5200__

/**
 * @brief   Atari 5200 platform layer: controller 1 and the boot hand-over
 *
 * The stick is the BIOS VBI's copy of the pots; the top fire button is the
 * trigger. The keypad is polled here (the BIOS's own keypad IRQ ends in a
 * routine that does nothing), and the lower fire button is read as ESC.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <joystick.h>

#include <fujinet-fuji.h>
#include <fujinet-atari5200.h>

#include "../platform.h"
#include "../io.h"

#define REG(a)     (*(volatile uint8_t *)(a))
#define PADDL0     REG(0x11)    /* stick X, 0-228, 114 at rest */
#define PADDL1     REG(0x12)    /* stick Y */
#define TRIG0      REG(0xC010)  /* top fire: 0 while held */
#define CONSOL     REG(0xC01F)
#define SKCTL      REG(0xE80F)  /* write */
#define SKSTAT     REG(0xE80F)  /* read; bit 3 clear: the lower fire button */

/* Past these the stick is pushed; between them it is at rest */
#define STICK_LO   0x40
#define STICK_HI   0xA4

/* Without debounce (SKCTL $02, the BIOS's) a held key interrupts on every
   scan but not on every frame: it counts as let go only after this many polls
   (about frames) with no interrupt. */
#define UP_POLLS   4
#define REPEAT_FIRST 20
#define REPEAT_NEXT  6

/* How long reboot() waits for the image: the cartridge gives MOUNT_IMAGE 60
   seconds, so a little more, but a push that has not started after 3 seconds
   never will */
#define MOUNT_FRAMES 3900u
#define IDLE_FRAMES  180u

extern void a52_screen_init(void);
extern void a52_key_init(void);       /* key.s */
extern volatile uint8_t a52_key;      /* the latest keypad key + 1 */

static uint8_t pending, held, up_count, repeat, low_down;

void initialize() {
  CONSOL = 0x04;                /* the pots on, controller 1's keypad */
  SKCTL = 0x02;                 /* keypad scan on */
  a52_screen_init();
  held = 0xFF;
  a52_key_init();
}

uint8_t readJoystick() {
  uint8_t x = PADDL0, y = PADDL1, joy = 0;

  if (y < STICK_LO)
    joy |= JOY_UP_MASK;
  else if (y > STICK_HI)
    joy |= JOY_DOWN_MASK;
  if (x < STICK_LO)
    joy |= JOY_LEFT_MASK;
  else if (x > STICK_HI)
    joy |= JOY_RIGHT_MASK;
  if (TRIG0 == 0)
    joy |= JOY_BTN_1_MASK;
  return joy;
}

/* The ASCII a keypad key stands for, 0 for none */
static uint8_t key_char(uint8_t k) {
  switch (k) {
    case 2:    return KEY_UP_ARROW;
    case 8:    return KEY_DOWN_ARROW;
    case 4:    return KEY_LEFT_ARROW;
    case 6:    return KEY_RIGHT_ARROW;
    case 0x0B: return KEY_RETURN;       /* # */
    case 0x0A:                          /* * */
    case 0x0E: return KEY_ESCAPE;       /* RESET */
    case 0x0C: return 'r';              /* START: refresh */
    case 0x0D: return 'c';              /* PAUSE: change name */
    case 0:    return 'q';              /* 0: QA mode */
    default:   return 0;
  }
}

static void poll_keys(void) {
  uint8_t k, c, fresh, skstat = SKSTAT;

  // The lower fire button is Escape
  if (!(skstat & 0x08)) {
    if (!low_down) {
      low_down = 1;
      pending = KEY_ESCAPE;
      return;
    }
  } else {
    low_down = 0;
  }

  k = a52_key;
  if (!k) {                     // no keypad interrupt since the last poll
    if (held != 0xFF && ++up_count >= UP_POLLS)
      held = 0xFF;
    return;
  }

  a52_key = 0;
  up_count = 0;
  k--;
  if (k != held) {
    held = k;
    repeat = REPEAT_FIRST;
    fresh = 1;
  } else {
    fresh = 0;
    if (--repeat)
      return;
    repeat = REPEAT_NEXT;
  }
  // Only the arrows repeat while held
  c = key_char(k);
  if (c && (fresh || (c >= KEY_LEFT_ARROW && c <= KEY_DOWN_ARROW)))
    pending = c;
}

uint8_t kbhit(void) {
  if (!pending)
    poll_keys();
  return pending != 0;
}

char cgetc(void) {
  char c;

  while (!kbhit())
    waitvsync();
  c = pending;
  pending = 0;
  return c;
}

/// @brief Pick the name with the stick: up/down changes the letter, fire or right
/// moves on, left or * goes back, # or START is done
void inputField(uint8_t x, uint8_t y, uint8_t max, char* buffer) {
  static const char wheel[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
  uint8_t len = strlen(buffer), pos, idx = 0, i;
  const char *p;

  for (i = 0; i < len; i++)
    if (buffer[i] >= 'a' && buffer[i] <= 'z')
      buffer[i] -= 32;
  if (len == 0) {
    buffer[0] = 'A';
    buffer[1] = 0;
    len = 1;
  }
  pos = len - 1;

  while (1) {
    p = strchr(wheel, buffer[pos]);
    idx = p ? p - wheel : 0;

    for (i = 0; i < max; i++) {
      gotoxy(x + i, y);
      revers(i == pos);
      cputc(i < len ? buffer[i] : ' ');
    }
    revers(0);

    waitvsync();
    readCommonInput();

    if ((input.key == KEY_RETURN || input.key == 'r') && len >= 2) {
      while (len > 2 && buffer[len - 1] == ' ')
        buffer[--len] = 0;
      gotoxy(x, y);
      for (i = 0; i < max; i++)
        cputc(i < len ? buffer[i] : ' ');
      // The # that ended this is not a select for event_loop(), which reads
      // input once more when the caller returns
      input.trigger = input.key = input.dirX = input.dirY = 0;
      return;
    }
    if (input.key == KEY_RETURN)
      continue;

    if (input.dirY) {
      idx = (idx + input.dirY + sizeof(wheel) - 1) % (sizeof(wheel) - 1);
      if (pos == 0 && wheel[idx] == ' ')
        idx = (idx + input.dirY + sizeof(wheel) - 1) % (sizeof(wheel) - 1);
      buffer[pos] = wheel[idx];
    } else if (input.dirX > 0 || input.trigger) {
      if (pos + 1 < max) {
        buffer[++pos] = 'A';
        buffer[pos + 1] = 0;
        len = pos + 1;
      }
    } else if (input.dirX < 0 || input.key == KEY_ESCAPE) {
      if (pos > 0) {
        buffer[pos--] = 0;
        len = pos + 1;
      }
    }
  }
}

/// @brief Wait for the FujiNet to hand the image to the cartridge, then start it
///
/// MOUNT_IMAGE has been answered by now. A FujiNet that is pushing the image
/// shows XFER, and READY when it is in; one that does not take the file for a
/// cartridge image (or could not open it) mounts it as a disk, or not at all,
/// and pushes nothing, so the state never leaves IDLE.
void reboot() {
  extern char buf[];
  uint16_t frames, idle = 0;
  uint8_t state, pct, last = 0xFF;
  const char *why = "Load timed out";

  for (frames = 0; frames < MOUNT_FRAMES; frames++) {
    state = fuji_a5200_boot_state();
    if (state == FUJI_A5200_BOOT_READY) {
      fuji_a5200_boot();
      why = "Could not start the image";
      break;
    }
    if (state == FUJI_A5200_BOOT_FAILED) {
      itoa(fuji_a5200_boot_error(), buf, 16);
      why = buf;
      break;
    }
    if (state == FUJI_A5200_BOOT_IDLE) {
      if (++idle >= IDLE_FRAMES) {
        why = "Not a cartridge image";
        break;
      }
    } else {
      idle = 0;
    }
    pct = fuji_a5200_boot_percent();
    if (pct != last) {
      last = pct;
      gotoxy(0, 23);
      cclear(40);
      gotoxy(0, 23);
      cputs("Loading ");
      itoa(pct, buf, 10);
      cputs(buf);
      cputc('%');
    }
    waitvsync();
  }

  // Only a failure comes back
  gotoxy(0, 23);
  cclear(40);
  gotoxy(0, 23);
  cputs(state == FUJI_A5200_BOOT_FAILED ? "Load error " : "");
  cputs(why);
  for (frames = 0; frames < 180; frames++)
    waitvsync();
}

#endif /* __ATARI5200__ */
