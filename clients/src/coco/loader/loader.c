#include <cmoc.h>
#include <coco.h>

/// @brief Invokes the CoCo BASIC RUNM command from machine code.
/// Builds the "M\"FILE" command in the low-memory direct-mode buffer at
/// $02DD and points CHARAD there before jumping into BASIC's RUNM routine.
/// Because the command state lives in low memory, the loaded binary may
/// overlap the old BASIC program area without corrupting the running load.
void runm(const char *filename)
{
    *((uint16_t *)0x2dd) = 0x4D22;   // 'M' '"'
    strcpy(0x2df, filename);
    *((uint16_t *)0xa6) = 0x2dd;     // CHARAD

    asm
    {
        ldd     #$4D1C
        jmp     $AE75
    }
}

int main(void)
{
    width(32);
    cls(1);
    printf("LOADING FUJINET LOBBY...");

    runm("LOBBY");

    return 0;
}
