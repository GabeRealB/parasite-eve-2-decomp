#include "mem.h"

#include <psyq/sys/types.h>
#include <psyq/libapi.h>

#include "types.h"

#include "gamemain.h"

/// Initial CPU stack-pointer byte address installed before resident execution.
///
/// The stack grows downward from this cached RAM address. Stored as the
/// unsigned address word accepted by `SetSp`, rather than a C object pointer.
static u32 _gBootStackAddress = 0x801fff00;

// Keep the heap-base word in main's fixed .data subsegment.
void* gMemPrimaryHeapBase = MEM_PRIMARY_HEAP_ADDRESS;

// BSS symbols (gMemSavedAuxHeap … gCdCmdQueue … gMemSavedImageRegionBytes) live in the `main` bss
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
    SetSp(_gBootStackAddress);
    gameMainRun();
}
