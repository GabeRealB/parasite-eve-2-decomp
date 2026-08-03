#include "common.h"

#include <psyq/libapi.h>

#include "main/game.h"
#include "main/fs.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
u32 GStackBase = 0x801fff00;

// Apparently, it seems that the heap is not located on the heap, but rather
// after the main heap defined by the entry point.
u8* GHeap = (u8*)0x80083800;

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
