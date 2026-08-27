/**
 * @brief   Functions that must be implemented by each platform
 */

#ifndef PLATFORM_H
#define PLATFORM_H


// Include platform specific defines before the input include
#include "atari/vars.h"
#include "apple2/vars.h"
#include "c64/vars.h"
#include "coco/vars.h"
//#include "adam/vars.h"



// Platform specific implementations
unsigned char readJoystick();

/// @brief Platform specific initialization
void initialize();

/// @brief Wait for vertical sync
void waitvsync();

/// @brief Reboot the system to run mounted disk
void reboot();

/**
 * @brief Draw the QR code in qr_buf, with a "press a key" prompt.
 *
 * Free to switch video mode, repoint the character set, or write screen memory
 * directly -- qr_restore() puts it all back. Read the matrix with qr_get(),
 * which reports light for coordinates outside the symbol so a loop over the
 * quiet zone needs no special casing.
 *
 * A scanner needs QR_QUIET modules of blank margin on every side. Where the
 * screen cannot spare that much vertically, set the border and background to
 * the light colour so the margin continues into the overscan area.
 */
void qr_draw();

/// @brief Undo whatever qr_draw() did to the display.
void qr_restore();

/**
 * @brief Block until a key is pressed, and return it.
 *
 * Separate from cgetc() because that does not block on every platform.
 */
char qr_wait_key();

#endif /* PLATFORM_H */
