#ifdef __ATARI7800__

/**
 * @brief   Atari 7800 platform layer: the left controller, the console
 *          switches and the boot hand-over
 *
 * The stick and the left button (or a 2600 joystick's one) are the
 * joystick. The right button and RESET are Escape; PAUSE refreshes, SELECT
 * changes the name, and SELECT with PAUSE toggles QA mode. The switches
 * count when let go, so the pair can be told from either alone.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <joystick.h>

#include <fujinet-fuji.h>
#include <fujinet-atari7800.h>

#include "../platform.h"
#include "../io.h"

#define REG(a)     (*(volatile uint8_t *)(a))
#define INPT0      REG(0x08)    /* right button, two-button mode: bit 7 set */
#define INPT1      REG(0x09)    /* left button, two-button mode: bit 7 set */
#define INPT4      REG(0x0C)    /* a 2600 joystick's button: bit 7 clear */
#define SWCHA      REG(0x280)   /* bits 7-4: right, left, down, up; low */
#define SWCHB      REG(0x282)   /* bit 0 RESET, 1 SELECT, 3 PAUSE; low */
#define CTLSWB     REG(0x283)

#define PB2        0x04         /* the left port's two-button mode, driven low */

#define SW_RESET   0x01
#define SW_SELECT  0x02
#define SW_PAUSE   0x08
#define SW_B2      0x10         /* not a switch: the right button */

/* How long reboot() waits for the image: the cartridge gives MOUNT_IMAGE 60
   seconds, so a little more, but a push that has not started after 3 seconds
   never will */
#define MOUNT_FRAMES 3900u
#define IDLE_FRAMES  180u

static uint8_t held, seen, pending;

void initialize() {
  CTLSWB = PB2;
  SWCHB = 0;
  a78_init();
  a78_clrscr();
}

void waitvsync() {
  a78_vsync();
}

static bool fire(void) {
  return (INPT1 & 0x80) || !(INPT4 & 0x80);
}

uint8_t readJoystick() {
  uint8_t j = (uint8_t)~SWCHA, joy = 0;

  if (j & 0x10)
    joy |= JOY_UP_MASK;
  else if (j & 0x20)
    joy |= JOY_DOWN_MASK;
  if (j & 0x40)
    joy |= JOY_LEFT_MASK;
  else if (j & 0x80)
    joy |= JOY_RIGHT_MASK;
  if (fire())
    joy |= JOY_BTN_1_MASK;
  return joy;
}

/* The switches and the right button: a key when all are let go */
static void poll_keys(void) {
  uint8_t now = (uint8_t)~SWCHB & (SW_RESET | SW_SELECT | SW_PAUSE);

  if (INPT0 & 0x80)
    now |= SW_B2;
  if (now) {
    seen |= now;
    held = 1;
    return;
  }
  if (!held)
    return;
  held = 0;
  if ((seen & (SW_SELECT | SW_PAUSE)) == (SW_SELECT | SW_PAUSE))
    pending = 'q';
  else if (seen & (SW_RESET | SW_B2))
    pending = KEY_ESCAPE;
  else if (seen & SW_SELECT)
    pending = 'c';
  else if (seen & SW_PAUSE)
    pending = 'r';
  seen = 0;
}

uint8_t kbhit(void) {
  if (!pending)
    poll_keys();
  return pending != 0;
}

/* A key, or the left button pressed anew (pause() waits on cgetc()) */
char cgetc(void) {
  char c;
  bool was = fire();

  while (!kbhit()) {
    waitvsync();
    if (fire()) {
      if (!was)
        return KEY_RETURN;
    } else {
      was = false;
    }
  }
  c = pending;
  pending = 0;
  return c;
}

/* Two lines of help under the name */
static void name_help(uint8_t x, uint8_t y, bool on) {
  textcolor(R_NOTE);
  gotoxy(x, y + 2);
  if (on)
    cputs("Stick: letter  FIRE: next  B2: back");
  else
    cclear(35);
  gotoxy(x, y + 3);
  if (on)
    cputs("PAUSE: done");
  else
    cclear(11);
  textcolor(R_TEXT);
}

/// @brief Pick the name with the stick: up/down changes the letter, fire or right
/// moves on, left or the right button goes back, PAUSE is done
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
  name_help(x - 1, y, true);

  while (1) {
    p = strchr(wheel, buffer[pos]);
    idx = p ? p - wheel : 0;

    gotoxy(x, y);
    for (i = 0; i < max; i++) {
      revers(i == pos);
      cputc(i < len ? buffer[i] : ' ');
    }
    revers(0);
    a78_flush();

    waitvsync();
    readCommonInput();

    if ((input.key == KEY_RETURN || input.key == 'r') && len >= 2) {
      while (len > 2 && buffer[len - 1] == ' ')
        buffer[--len] = 0;
      gotoxy(x, y);
      for (i = 0; i < max; i++)
        cputc(i < len ? buffer[i] : ' ');
      name_help(x - 1, y, false);
      // The press that ended this is not a select for event_loop(), which
      // reads input once more when the caller returns
      input.trigger = input.key = input.dirX = input.dirY = 0;
      return;
    }
    if (input.key == KEY_RETURN || input.key == 'r')
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

static void footer(const char *s) {
  textcolor(R_STATUS);
  gotoxy(0, 23);
  cclear(40);
  gotoxy(0, 23);
  cputs(s);
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
  uint8_t state = FUJI_A7800_BOOT_IDLE, pct, last = 0xFF;
  const char *why = "Load timed out";

  for (frames = 0; frames < MOUNT_FRAMES; frames++) {
    state = fuji_a7800_boot_state();
    if (state == FUJI_A7800_BOOT_READY) {
      fuji_a7800_boot();
      why = "Could not start the image";
      break;
    }
    if (state == FUJI_A7800_BOOT_FAILED) {
      itoa(fuji_a7800_boot_error(), buf, 16);
      why = buf;
      break;
    }
    if (state == FUJI_A7800_BOOT_IDLE) {
      if (++idle >= IDLE_FRAMES) {
        why = "Not a cartridge image";
        break;
      }
    } else {
      idle = 0;
    }
    pct = fuji_a7800_boot_percent();
    if (pct != last) {
      last = pct;
      strcpy(buf, "Loading ");
      itoa(pct, buf + 8, 10);
      strcat(buf, "%");
      footer(buf);
    }
    waitvsync();
  }

  // Only a failure comes back
  textcolor(R_ERROR);
  gotoxy(0, 23);
  cclear(40);
  gotoxy(0, 23);
  if (state == FUJI_A7800_BOOT_FAILED)
    cputs("Load error ");
  cputs(why);
  textcolor(R_TEXT);
  for (frames = 0; frames < 180; frames++)
    waitvsync();
}

#endif /* __ATARI7800__ */
