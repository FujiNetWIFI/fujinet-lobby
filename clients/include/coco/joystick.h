#ifndef _JOYSTICK_H
#define _JOYSTICK_H


#ifdef _CMOC_VERSION_
#include <coco.h>

/* Macros that evaluate the return code of joy_read */
#define JOY_UP(v) ((v) & 1)
#define JOY_DOWN(v) ((v) & 2)
#define JOY_LEFT(v) ((v) & 4)
#define JOY_RIGHT(v) ((v) & 8)
#define JOY_BTN_1(v) ((v) & 16) /* Universally available */
#define JOY_BTN_2(v) ((v) & 32) /* Second button if available */
#endif

#endif
