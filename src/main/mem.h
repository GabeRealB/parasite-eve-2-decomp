#ifndef MAIN_PRIVATE_MEM_H
#define MAIN_PRIVATE_MEM_H

#include <psyq/sys/types.h>

#include "types.h"

/// Clears `size` bytes at `dst` one byte at a time, in place rather than
/// through `memFillBytes`.
#define MEM_CLEAR(dst, size)                             \
    {                                                    \
        u8* _clearPtr = (u8*)(dst);                      \
        u32 _clearI;                                     \
        for (_clearI = 0; _clearI < (size); _clearI++) { \
            *_clearPtr++ = 0;                            \
        }                                                \
    }

/// Address of the primary heap: the fixed RAM between the resident executable
/// and the gameplay overlay.
#define MEM_PRIMARY_HEAP_ADDRESS ((void*)0x80083800)

/// Base of the fixed primary heap used by the resident allocation wrappers.
///
/// The writable RAM region [0x80083800, 0x80093780) contains 0xFF80 bytes,
/// including PsyQ heap3 bookkeeping. `Mem_Init` initializes it before use.
/// Allocations with `auxHeap == false` and releases through `memFree` use this
/// heap. Its base stays fixed for the program's lifetime while the allocator's
/// free-list cursor moves within it; the storage belongs to the allocator.
extern void* gMemPrimaryHeapBase;

/// Pointer to the auxiliary heap.
extern u8* GAuxHeap;

/// Length in bytes of the heap pointed to by `GAuxHeap`.
extern size_t GAuxHeapSize;

extern u8* Mem_AuxRegionBase;

extern size_t Mem_AuxRegionBytes;

#endif // MAIN_PRIVATE_MEM_H
