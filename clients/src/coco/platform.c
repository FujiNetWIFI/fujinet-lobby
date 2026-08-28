#ifdef _CMOC_VERSION_

#include <stdint.h>
#include <string.h>
#include <coco.h>
#include <joystick.h>
#include <fujinet-fuji.h>

#include "qr.h"

extern char panel_spacer_string[];
extern DeviceSlot device_slots[1];

#define JOY_CENTER   31
#define JOY_HALF     16

#define JOY_LOW_TH   (JOY_CENTER - JOY_HALF)   /* 15 */
#define JOY_HIGH_TH  (JOY_CENTER + JOY_HALF)   /* 47 */

#define JOY_NOT_USED 0
#define JOY_SELECTING 1
#define JOY_USING 2

uint8_t right_joy_state = JOY_NOT_USED;
uint8_t left_joy_state = JOY_NOT_USED;

unsigned char readJoystick() {
    byte value = 0;
    bool lbtn1, lbtn2, rbtn1, rbtn2;
    byte h, v;

    byte buttons = readJoystickButtons();   /* active-high */

    /* NOTE: As of right now, the enum in the coco.h */
    /* header file is incorrect for the button values. */
    lbtn1  = (buttons & 0x02 ) == 0;
    lbtn2  = (buttons & 0x08 ) == 0;
    rbtn1  = (buttons & 0x01 ) == 0;
    rbtn2  = (buttons & 0x04 ) == 0;

    // The first time a button is pressed,
    // ONLY register that the joystick is active
    // and return.
    if (left_joy_state == JOY_NOT_USED && (lbtn1 || lbtn2))
    {
        left_joy_state = JOY_SELECTING;
        right_joy_state = JOY_NOT_USED;
    }
    else if (left_joy_state == JOY_SELECTING && !(lbtn1 || lbtn2))
    {
        left_joy_state = JOY_USING;
        right_joy_state = JOY_NOT_USED;
    }

    if (right_joy_state == JOY_NOT_USED && (rbtn1 || rbtn2))
    {
        right_joy_state = JOY_SELECTING;
        left_joy_state = JOY_NOT_USED;
    }
    else if (right_joy_state == JOY_SELECTING && !(rbtn1 || rbtn2))
    {
        right_joy_state = JOY_USING;
        left_joy_state = JOY_NOT_USED;
    }

    // Don't read joystick positions until one of the buttons is pressed.
    if (left_joy_state == JOY_USING || right_joy_state == JOY_USING)
    {
        const byte *joy = readJoystickPositions();

       // Toggle back and forth between left and right joystick
       // depending on which one's buttons were last pressed.
        if (left_joy_state == JOY_USING) // Using left joystick
        {
            h = joy[JOYSTK_LEFT_HORIZ];
            v = joy[JOYSTK_LEFT_VERT];
            if (lbtn1)
                value |= 16; /* bit 4 = button 1 */
            if (lbtn2)
                value |= 32; /* bit 5 = button 2 */
        }
        else // Using right joystick
        {
            h = joy[JOYSTK_RIGHT_HORIZ];
            v = joy[JOYSTK_RIGHT_VERT];

            if (rbtn1)
                value |= 16; /* bit 4 = button 1 */
            if (rbtn2)
                value |= 32; /* bit 5 = button 2 */
        }

        /* Direction bits: UP, DOWN, LEFT, RIGHT
           Vertical: 0 = UP, 63 = DOWN */
        if (v <= JOY_LOW_TH)
            value |= 1; /* up */
        if (v >= JOY_HIGH_TH)
            value |= 2; /* down */
        if (h <= JOY_LOW_TH)
            value |= 4; /* left */
        if (h >= JOY_HIGH_TH)
            value |= 8; /* right */
    }
    else
    {
        value = 0;
    } 

    return value;
}


void initialize() {
  initCoCoSupport();

  if (!isCoCo3) {
    // Colored dashes don't look good on non coco3
    strcpy(panel_spacer_string, "--------");
  }

  left_joy_state = JOY_NOT_USED;
  right_joy_state = JOY_NOT_USED;
}

void waitvsync() {
    setTimer(0);
    
    while (!getTimer());
}

/// @brief Invokes the CoCo BASIC RUNM command
/// @param filename 
void runm(char * filename) 
{
    // This reproduces the state when executing RUNM"FILE in BASIC
    // by filling in the command line buffer and jumping directly
    // to the Basic RUN procedure

    // Set beginning of (compressed) command: M"
    *((uint16_t*)0x2dd)=0x4D22;

    // Add the filename to the command
    strcpy(0x2df,filename);

    // Set command pointer
    *((uint16_t*)0xa6)=0x2dd; // set CHARAD
    
    // Jump to "RUNM" command
    asm
    {
        // Observed this value in actual RUNM. Not working unless 4Dxx is set
        ldd     #$4D1C

        // Jump to RUN procedure
        jmp     $AE75
    }
}

void reboot(void)
{
  // No need to reboot - just RUNM the game
  static uint8_t i;
  static char *filename;
  
  filename = device_slots[0].file;

  // Strip path from filename, converting to uppercase
  for(i=strlen(filename)-1;i<=255;i--) {
    if (filename[i]== '/') {
      filename=&filename[i+1];
      break;
    } else {
      if (filename[i]>=97 && filename[i]<=122 )
        filename[i]-=32;
    }
  }
  
  // Remove extension
  if (strstr(filename, "."))
    *((char*)strstr(filename, "."))=0;

  // Run the .bin file by the same name as the filename
  runm(filename);
}

/*
 * QR rendering: MC6847 SG4 semigraphics.
 *
 * Any byte >= $80 on the text screen is a semigraphics cell rather than a
 * character: 1 CCC ULUR LLLR, where the low four bits light the four quadrants
 * of the cell in colour CCC. That turns the 32x16 text screen into a 64x32 grid
 * of independently lit blocks -- the only way to get 21 modules and a full
 * 4-module quiet zone onto a screen only 16 character rows tall.
 *
 * The lobby already relies on this working: panel_spacer_string in main.c is a
 * row of SG4 bytes, and coco/conio.c passes anything >= $80 through untouched.
 *
 * Set bits are lit, so a *light* module is a set bit and a dark module is a
 * clear one -- the reverse of the character-cell platforms. Blocks are 4x6
 * pixels, so modules come out stretched about 1:1.5 vertically. QR decoders
 * build an affine transform from the three finder patterns, so a uniform
 * stretch is recoverable, but this is the most distorted of the platforms and
 * the one most worth testing against a real phone.
 */

#define QR_TEXT_BASE    ((uint8_t *) 0x0400)
#define QR_TEXT_COLS    32
#define QR_TEXT_ROWS    16
#define QR_BLOCK_COLS   (QR_TEXT_COLS * 2)   /* 64 */
#define QR_BLOCK_ROWS   (QR_TEXT_ROWS * 2)   /* 32 */

/* Buff (colour 4) on black reads as white on this display. */
#define QR_SG4_BASE     0xC0

/* Centre the symbol plus its quiet zone in the block grid. */
#define QR_ORIGIN_X     ((QR_BLOCK_COLS - QR_MODULES) / 2)   /* 21 */
#define QR_ORIGIN_Y     QR_QUIET                              /* 4 */

/* Is the block at (bx, by) lit? Light modules and the quiet zone are lit. */
static uint8_t qr_block(uint8_t bx, uint8_t by) {
  uint8_t mx, my;

  if (bx < QR_ORIGIN_X || by < QR_ORIGIN_Y)
    return 1;

  mx = bx - QR_ORIGIN_X;
  my = by - QR_ORIGIN_Y;

  if (mx >= QR_MODULES || my >= QR_MODULES)
    return 1;

  return qr_get(mx, my) ? 0 : 1;
}

void qr_draw() {
  uint8_t cx, cy, bx, by, cell;

  /* The bottom character row is left as text for the prompt, so the symbol and
     its quiet zone occupy block rows 0..29 of the 32 available. */
  for (cy = 0; cy < QR_TEXT_ROWS - 1; cy++) {
    for (cx = 0; cx < QR_TEXT_COLS; cx++) {
      bx = cx * 2;
      by = cy * 2;
      cell = QR_SG4_BASE
           | (qr_block(bx,     by)     << 3)   /* upper left  */
           | (qr_block(bx + 1, by)     << 2)   /* upper right */
           | (qr_block(bx,     by + 1) << 1)   /* lower left  */
           | (qr_block(bx + 1, by + 1));       /* lower right */
      QR_TEXT_BASE[cy * QR_TEXT_COLS + cx] = cell;
    }
  }

  gotoxy(5, QR_TEXT_ROWS - 1);
  cputs("SCAN TO CHAT - ANY KEY");
}

void qr_restore() {
  memset(QR_TEXT_BASE, 0x60, (uint16_t) QR_TEXT_COLS * QR_TEXT_ROWS);
}

char qr_wait_key() {
  /* cgetc() has no prototype in scope here, so cmoc assumes it returns int. */
  return (char) cgetc();
}

#endif /* _CMOC_VERSION_ */