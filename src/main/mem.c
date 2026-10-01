#include "main/mem.h"

#include <psyq/sys/types.h>
#include <psyq/malloc.h>
#include <psyq/stdio.h>

#include "types.h"

#include "mem.h"

/// Extent in bytes of the primary heap.
#define G_HEAP_SIZE 0xFF80

static void _memSetActiveHeap(bool auxHeap);

// The rom contains an empty function that is never called.
// Might have been a debug utility that is not present in
// the release.
static void Mem_Dummy0();

// `_freep` is exported by libapi: the block within a heap that `malloc3`
// and `free3` begin their search from. `InitHeap3` sets it to the heap it
// initializes, and the two routines move it as that heap's blocks are taken
// and released. Each heap is an independent ring of blocks, so writing
// `_freep` is what selects the active heap. The developers decided to
// repurpose the existing heap utilities instead of writing a custom
// implementation.
//
// NOLINTNEXTLINE
extern u8* _freep;

void Mem_Set(void* dest, u32 ch, u32 count)
{
    u32 i;
    u8* ptr;
    u8  v8;
    u16 v16;
    u32 v32;
    u32 remaining;
    u32 alignment;

    ptr       = (u8*)dest;
    remaining = count;

    v8  = ch & 0xFF;
    v16 = (u16)(v8 | (v8 << 8));
    v32 = (v8 << 24) + (v8 << 16) + (v8 << 8) + v8;

    while (remaining >= 4) {
        /* Alignment depends on address bits, not the pointed-to value. */
        alignment = (uintptr)ptr & 3;

        switch (alignment) {
            case 0:
                *(u32*)ptr = v32;
                ptr       += 4;
                remaining -= 4;
                break;

            case 1:
                *ptr++     = v8;
                *(u16*)ptr = v16;
                ptr       += 2;
                remaining -= 3;
                break;

            case 2:
                *(u16*)ptr = v16;
                ptr       += 2;
                remaining -= 2;
                break;

            case 3:
                *ptr       = v8;
                ptr       += 1;
                remaining -= 1;
                break;
        }
    }

    i = 0;
    while ((i & 0xFFFF) < remaining) {
        *ptr++ = v8;
        i++;
    }
}

/// Clears exactly `sizeBytes` bytes, using stores aligned to their access width.
static inline void _memClearAllocation(void* allocation, size_t sizeBytes)
{
    u32    tailByteIndex;
    size_t bytesRemaining;
    u8*    clearCursor;
    u16    zeroHalfword;
    u32    zeroWord;

    clearCursor    = allocation;
    bytesRemaining = sizeBytes;
    zeroWord       = 0;
    zeroHalfword   = 0;

    while (bytesRemaining >= 4) {
        // Use aligned word and halfword stores without crossing the requested extent.
        switch ((uintptr)clearCursor & 3) {
            case 0:
                *(u32*)clearCursor = zeroWord;
                clearCursor       += 4;
                bytesRemaining    -= 4;
                break;

            case 1:
                *clearCursor++     = 0;
                *(u16*)clearCursor = zeroHalfword;
                clearCursor       += 2;
                bytesRemaining    -= 3;
                break;

            case 2:
                *(u16*)clearCursor = zeroHalfword;
                clearCursor       += 2;
                bytesRemaining    -= 2;
                break;

            case 3:
                *clearCursor    = 0;
                clearCursor    += 1;
                bytesRemaining -= 1;
                break;
        }
    }

    tailByteIndex = 0;
    while ((u16)tailByteIndex < bytesRemaining) {
        *clearCursor++ = 0;
        tailByteIndex++;
    }
}

void* memCalloc(size_t sizeBytes, bool auxHeap)
{
    void* allocation;

    _memSetActiveHeap(auxHeap);
    allocation = malloc3(sizeBytes);
    if (allocation != NULL) {
        _memClearAllocation(allocation, sizeBytes);
    } else {
        printf("gmalloc2-->NULL\n");
    }
    return allocation;
}

/// Selects the initialized heap3 ring for subsequent allocations and releases.
///
/// `auxHeap == true` selects `gMemActiveAuxHeap`; every other value selects
/// `gMemPrimaryHeapBase`. The selected base must remain a member of its heap3
/// free-block ring, initialized by `Mem_Init` or, for the auxiliary heap,
/// `Mem_InitAux`.
///
/// Selection resets `_freep`, the allocator's search cursor, to that base and
/// persists until another heap is selected. Releases must return blocks to
/// their originating heap.
static void _memSetActiveHeap(bool auxHeap)
{
    void* heapBase;

    heapBase = auxHeap == true ? gMemActiveAuxHeap : gMemPrimaryHeapBase;
    _freep   = heapBase;
}

void* Mem_Malloc(size_t size, bool auxHeap)
{
    void* ptr;

    if (auxHeap == true) {
        _freep = gMemActiveAuxHeap;
    } else {
        _freep = gMemPrimaryHeapBase;
    }

    ptr = malloc3(size);
    if (ptr == NULL) {
        printf("gmalloc-->NULL\n");
    }
    return ptr;
}

void memFree(void* allocation)
{
    _freep = gMemPrimaryHeapBase;
    free3(allocation);
}

void memFreeFromHeap(void* allocation, bool auxHeap)
{
    void* heapBase;

    heapBase = auxHeap == true ? gMemActiveAuxHeap : gMemPrimaryHeapBase;
    _freep   = heapBase;
    free3(allocation);
}

void Mem_InitAux(void)
{
    InitHeap3(gMemActiveAuxHeap, GActiveAuxHeapSize);
}

void Mem_Init()
{
    InitHeap3(gMemActiveAuxHeap, GActiveAuxHeapSize);
    InitHeap3(gMemPrimaryHeapBase, G_HEAP_SIZE);
}

// The rom contains an empty function that is never called.
// Might have been a debug utility that is not present in
// the release.
static void Mem_Dummy0()
{
}

void Mem_SetActiveAuxHeap(bool aux0)
{
    switch (aux0 & 0xFFFF) {
        case false:
            gMemActiveAuxHeap  = Mem_AuxRegionBase;
            GActiveAuxHeapSize = Mem_AuxRegionBytes;
            break;

        case true:
            gMemActiveAuxHeap  = GAuxHeap;
            GActiveAuxHeapSize = GAuxHeapSize;
            break;
    }
}
