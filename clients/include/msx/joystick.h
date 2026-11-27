#ifndef _JOYSTICK_H
#define _JOYSTICK_H

#ifdef __MSX__

/* Macros that evaluate the return code of readJoystick().
   readJoystick() in src/msx/platform.c translates the BIOS GTSTCK direction
   (1-8, clockwise from up) into this layout, the same one the Adam uses. */
#define JOY_UP(v)    ((v) & 1)
#define JOY_DOWN(v)  ((v) & 2)
#define JOY_LEFT(v)  ((v) & 4)
#define JOY_RIGHT(v) ((v) & 8)
#define JOY_BTN_1(v) ((v) & 16) /* Universally available */
#define JOY_BTN_2(v) ((v) & 32) /* Second button if available */

#endif /* __MSX__ */

#endif
