/**
 * @brief   QR code screen shown before booting into a game
 *
 * Each lobby room has a companion web chat room. Just before the machine boots
 * the game we draw that room's URL as a QR code, so a player can scan it with a
 * phone and land in the chat for the table they are about to sit at.
 *
 * The symbol is always QR version 1, 21x21 modules. That is the largest an
 * Intellivision can render, and keeping every platform on the same size means
 * one URL budget and one set of expectations everywhere.
 */

#ifndef QR_H
#define QR_H

#if defined(_CMOC_VERSION_)

/* cmoc has bool built in, and the project's include/coco/stdbool.h shim
   conflicts with it once coco.h has been seen. Same guard fujinet-lib uses. */
#include <cmoc.h>
#include <coco.h>

#else

#include <stdint.h>
#include <stdbool.h>

#endif

/// @brief Modules per side of a version 1 symbol.
#define QR_MODULES 21

/// @brief Modules of blank margin a scanner needs around the symbol.
#define QR_QUIET   4

/// @brief The encoded matrix: [0] is the module count, [1..56] the modules.
extern uint8_t qr_buf[57];

/// @brief Module count from the last successful fetch, 0 if none.
extern uint8_t qr_size;

/**
 * @brief Ask the FujiNet to encode url and read back the module matrix.
 * @return false if the url is empty, the FujiNet refused it, or the result was
 *         not a version 1 symbol. Callers must carry on regardless.
 */
bool qr_fetch(const char *url);

/**
 * @brief Is the module at (x, y) dark?
 * @return 1 for dark, 0 for light or outside the symbol -- so the quiet zone
 *         falls out of a loop that runs past the edges.
 */
uint8_t qr_get(uint8_t x, uint8_t y);

/**
 * @brief Fetch, draw, wait for a keypress, and restore the screen.
 *
 * Silently does nothing if the room has no chat url or the encode fails. A
 * missing QR code must never stop a game from booting.
 */
void qr_show(const char *url);

#endif /* QR_H */
