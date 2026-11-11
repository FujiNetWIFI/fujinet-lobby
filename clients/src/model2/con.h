#ifndef MODEL2_CON_H
#define MODEL2_CON_H

#include <stdint.h>

// Raw BIOS console and P&T special functions
void con_out(uint8_t c) __z88dk_fastcall;
int con_in(void);
uint32_t con_ticks(void);
void con_vram_put(uint16_t off, uint8_t c);

#endif /* MODEL2_CON_H */
