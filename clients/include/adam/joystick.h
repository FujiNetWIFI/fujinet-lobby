#ifndef _JOYSTICK_H
#define _JOYSTICK_H

#ifdef __ADAM__

/* Macros that evaluate the return code of readJoystick().
   readJoystick() in src/adam/platform.c translates the EOS decode table
   (1=up 2=right 4=down 8=left) into this layout, which is the one cc65's
   joystick driver and include/coco/joystick.h both use. */
#define JOY_UP(v)    ((v) & 1)
#define JOY_DOWN(v)  ((v) & 2)
#define JOY_LEFT(v)  ((v) & 4)
#define JOY_RIGHT(v) ((v) & 8)
#define JOY_BTN_1(v) ((v) & 16) /* Universally available */
#define JOY_BTN_2(v) ((v) & 32) /* Second button if available */

#endif /* __ADAM__ */

#endif
