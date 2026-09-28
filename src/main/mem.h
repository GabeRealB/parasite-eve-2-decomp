#ifndef MAIN_PRIVATE_MEM_H
#define MAIN_PRIVATE_MEM_H

#include <psyq/sys/types.h>

#include "types.h"

/// Clears `size` bytes at `dst` one byte at a time, in place rather than
/// through `Mem_Set`.
#define MEM_CLEAR(dst, size)                             \
    {                                                    \
        u8* _clearPtr = (u8*)(dst);                      \
        u32 _clearI;                                     \
        for (_clearI = 0; _clearI < (size); _clearI++) { \
            *_clearPtr++ = 0;                            \
        }                                                \
    }

/// Base address of the primary heap, the region allocations are served from
/// while the primary heap is the active one.
///
/// The primary heap is a fixed part of the game's memory map: it is the RAM
/// below the area the overlays are loaded into, so a loaded overlay never
/// covers it. Its extent is therefore the `G_HEAP_SIZE` constant, where the
/// auxiliary heaps take both a base and an extent that are set at runtime as
/// the images backing them are loaded.
extern u8* gMemHeap;

/// Pointer to the auxiliary heap.
extern u8* GAuxHeap;

/// Length in bytes of the heap pointed to by `GAuxHeap`.
extern size_t GAuxHeapSize;

extern u8* Mem_AuxRegionBase;

extern size_t Mem_AuxRegionBytes;

#endif // MAIN_PRIVATE_MEM_H
