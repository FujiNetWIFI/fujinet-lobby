#ifdef __TRS80M2__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <cpm.h>
#include <fujinet-fuji.h>
#include "con.h"
#include "vid.h"

extern HostSlot host_slots[];
extern DeviceSlot device_slots[];

// The FujiNet driver and its drives' DPHs live above $BFFF
#define DRIVER_BASE 0xC000
#define NO_DRIVE    0xFF

static uint8_t fcb[36];
static uint8_t dma[128];
static char program[9];
static char command[12];

unsigned char readJoystick(void)
{
  return 0;
}

void initialize(void)
{
  vid_init();
}

// About a frame, from P&T's 1/100 s clock
void waitvsync(void)
{
  static uint32_t start;

  start = con_ticks();
  while (con_ticks() - start < 2)
    ;
}

void quit(void)
{
  vid_exit();
  exit(0);
}

static void print(const char *s)
{
  while (*s)
    con_out((uint8_t)*s++);
}

// BIOS SELDSK (WBOOT+24): the drive's DPH, or 0 if there is no such drive
static uint16_t seldsk(uint8_t drive) __z88dk_fastcall __naked
{
#asm
    EXTERN  l_dcal
    ld      c,l
    ld      hl,(1)
    ld      de,24
    add     hl,de
    ld      e,1         ; not a first select
    jp      l_dcal
#endasm
}

/* The CP/M drive for FujiNet slot 0. The driver maps its drives to slots in
   letter order, so it is the lowest drive the driver answers for. */
static uint8_t fujinet_drive(void)
{
  static uint8_t d;

  for (d = 0; d < 16; d++)
    if (seldsk(d) >= DRIVER_BASE)
      return d;
  return NO_DRIVE;
}

static void set_fcb(uint8_t drive, const char *name)
{
  memset(fcb, 0, sizeof(fcb));
  fcb[0] = drive;
  memcpy(fcb + 1, name, 11);
}

/* The .COM on drive to run: the one named like the image, else the first */
static bool find_program(uint8_t drive, const char *image)
{
  static const char *s;
  static uint8_t i, r, *e;
  static char want[9], name[9];

  s = strrchr(image, '/');
  s = s ? s + 1 : image;
  for (i = 0; i < 8 && s[i] && s[i] != '.'; i++)
    want[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
  want[i] = 0;

  program[0] = 0;
  bdos(CPM_SDMA, (int)dma);
  set_fcb(drive + 1, "????????COM");
  for (r = (uint8_t)bdos(CPM_FFST, (int)fcb); r != 0xFF; r = (uint8_t)bdos(CPM_FNXT, 0))
  {
    e = dma + (r & 3) * 32 + 1;
    for (i = 0; i < 8 && (e[i] & 0x7F) != ' '; i++)
      name[i] = e[i] & 0x7F;
    name[i] = 0;
    if (!program[0] || !strcmp(name, want))
      strcpy(program, name);
  }
  return program[0] != 0;
}

/* CP/M runs the last line of A:$$$.SUB at the next warm boot */
static bool submit(const char *line)
{
  static uint8_t n;

  set_fcb(1, "$$$     SUB");
  bdos(CPM_DEL, (int)fcb);
  set_fcb(1, "$$$     SUB");
  if ((uint8_t)bdos(CPM_MAKE, (int)fcb) == 0xFF)
    return false;

  n = (uint8_t)strlen(line);
  memset(dma, 0, sizeof(dma));
  dma[0] = n;
  memcpy(dma + 1, line, n);
  bdos(CPM_SDMA, (int)dma);
  if (bdos(CPM_WRIT, (int)fcb) & 0xFF)
    return false;
  return (uint8_t)bdos(CPM_CLS, (int)fcb) != 0xFF;
}

/// @brief Run the game mounted on FujiNet drive 1 through CP/M, or say where it is
void reboot(void)
{
  static uint8_t drive;
  static DeviceSlot *d;

  vid_exit();
  d = &device_slots[0];

  drive = fujinet_drive();
  // Logs the new image in, and puts BDOS back on A: after the probe
  bdos(CPM_RDS, 0);

  if (drive != NO_DRIVE && find_program(drive, (const char *)d->file))
  {
    command[0] = 'A' + drive;
    command[1] = ':';
    strcpy(command + 2, program);
    if (submit(command))
      exit(0);
  }

  // As CONFIG does when it can't boot
  print("FujiNet drives:\r\n  1 R/O ");
  print((const char *)host_slots[d->hostSlot]);
  print(":");
  print((const char *)d->file);
  print("\r\n");
  exit(0);
}

#endif /* __TRS80M2__ */
