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

#endif /* PLATFORM_H */
