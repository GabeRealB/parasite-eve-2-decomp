#include "mem.h"

#include <psyq/sys/types.h>
#include <psyq/libapi.h>

#include "types.h"

#include "gamemain.h"

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
static u32 GStackBase;

// For some reason, the program starts by modifying its stack pointer and
// calling the actual entry function of the game. This address does not
// seem to be inserted by the linker.
static u32 GStackBase = 0x801fff00;

// Keep the heap-base word in main's fixed .data subsegment.
void* gMemPrimaryHeapBase = MEM_PRIMARY_HEAP_ADDRESS;

// BSS symbols (GAuxHeap … gCdCmdQueue … Mem_AuxRegionBytes) live in the `main` bss
// split (asm/USA/main/data/main.bss.s) so layout matches the retail binary.
// Defining the large gCdCmdQueue here makes GCC 2.8.1 reorder .comm symbols.

/// C-runtime entry that installs the game stack and starts resident execution.
///
/// Called once by the startup assembly after BSS and runtime-heap setup.
/// The `main` entry convention also invokes the compiler's runtime setup hook.
/// Never returns: `SetSp` leaves the saved return address on the abandoned
/// stack, and `gameMainRun` runs indefinitely on the replacement stack.
int main(void)
{
    SetSp(GStackBase);
    gameMainRun();
}
