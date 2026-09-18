#include "common.h"

#include <psyq/libapi.h>

#include "main/gamemain.h"
#include "main/mem.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
u32 GStackBase = 0x801fff00;

// Base of the primary heap. The word sits at an address the game's memory map
// fixes, and this unit's `.data` is the one the split places there, so the
// definition stays here rather than beside the wrappers in mem.c that read it;
// moving it means re-attributing that subsegment. `mem.h` documents the symbol.
u8* gMemHeap = (u8*)0x80083800;

// BSS symbols (GAuxHeap … CdCmd_Queue … D_800691F8) live in the `main` bss
// split (asm/USA/main/data/main.bss.s) so layout matches the retail binary.
// Defining the large CdCmd_Queue here makes GCC 2.8.1 reorder .comm symbols.

int main(void)
{
    // Modify the stack pointer.
    SetSp(GStackBase);

    // Call the entry function.
    GameMain();
}
