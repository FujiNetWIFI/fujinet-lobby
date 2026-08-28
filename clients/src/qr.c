#if defined(_CMOC_VERSION_)
#include <cmoc.h>
#include <coco.h>
#else
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#endif

#include <fujinet-fuji.h>

#include "qr.h"
#include "platform.h"

uint8_t qr_buf[57];
uint8_t qr_size;

bool qr_fetch(const char *url)
{
  qr_size = 0;

  if (url == NULL || url[0] == '\0')
    return false;

  if (!fuji_qrcode_v1(url, qr_buf))
    return false;

  // Every platform's renderer is written for a fixed 21x21 grid, so refuse
  // anything else rather than drawing a symbol that will not scan.
  if (qr_buf[0] != QR_MODULES)
    return false;

  qr_size = qr_buf[0];
  return true;
}

uint8_t qr_get(uint8_t x, uint8_t y)
{
  uint16_t i;

  // Out of range reads as light, which is what makes the quiet zone free: the
  // platform renderers just loop over a region larger than the symbol.
  if (x >= qr_size || y >= qr_size)
    return 0;

  // The FujiNet returns the matrix row-major, least significant bit first.
  i = (uint16_t) y * qr_size + x;
  return (qr_buf[1 + (i >> 3)] >> (i & 7)) & 1;
}

void qr_show(const char *url)
{
  if (!qr_fetch(url))
    return;

  qr_draw();
  qr_wait_key();
  qr_restore();
}
