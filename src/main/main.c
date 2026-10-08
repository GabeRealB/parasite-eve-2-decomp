#include "mem.h"

#include <psyq/sys/types.h>
#include <psyq/libapi.h>

#include "types.h"

#include "gamemain.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
static u32 GStackBase;

int main(void);

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
static u32 GStackBase = 0x801fff00;

// Keep the heap-base word in main's fixed .data subsegment.
void* gMemPrimaryHeapBase = MEM_PRIMARY_HEAP_ADDRESS;

// BSS symbols (GAuxHeap … gCdCmdQueue … Mem_AuxRegionBytes) live in the `main` bss
// split (asm/USA/main/data/main.bss.s) so layout matches the retail binary.
// Defining the large gCdCmdQueue here makes GCC 2.8.1 reorder .comm symbols.

int main(void)
{
    // Modify the stack pointer.
    SetSp(GStackBase);

    // Call the entry function.
    gameMainRun();
}
