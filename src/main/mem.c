#include "main/mem.h"

#include <psyq/sys/types.h>
#include <psyq/malloc.h>
#include <psyq/stdio.h>

#include "types.h"

#include "mem.h"

/// Extent in bytes of the primary heap.
#define G_HEAP_SIZE 0xFF80

static void _memSetActiveHeap(bool auxHeap);

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

void memFillBytes(void* destination, u32 value, size_t sizeBytes)
{
    u32    tailByteIndex;
    u8*    fillCursor;
    u8     fillByte;
    u16    fillHalfword;
    u32    fillWord;
    size_t bytesRemaining;

    fillCursor     = destination;
    bytesRemaining = sizeBytes;

    // Repeat the byte in every lane of the wider aligned stores.
    fillByte     = (u8)value;
    fillHalfword = fillByte | (fillByte << 8);
    fillWord     = ((u32)fillByte << 24) + (fillByte << 16) + (fillByte << 8) + fillByte;

    /// Fills a byte run ending at the next word boundary using aligned stores.
    ///
    /// Consumes four, three, two or one bytes for address residues zero, one,
    /// two or three modulo four. Advances `cursor` to word alignment and
    /// decreases `remaining` by the same byte count. Requires
    /// `remaining >= sizeof(u32)` and `remaining` writable bytes at `cursor`.
    ///
    /// `cursor` and `remaining` must be distinct, stable modifiable `u8*` and
    /// `size_t` lvalues. Argument expressions must have no evaluation side
    /// effects, and the objects holding them must be outside the filled storage.
    /// `cursor` is evaluated repeatedly; `remaining` is evaluated and updated
    /// once. `byteValue`, `halfwordValue` and `wordValue` must have types `u8`,
    /// `u16` and `u32`, with the wider values repeating the same byte. Each
    /// selected value is evaluated once; residue one selects both byte and
    /// halfword. Captures no locals and requires no configuration bindings.
    /// Defined only around the fill loop and undefined immediately afterward.
#define MEMORY_FILL_ALIGNED_CHUNK(cursor, remaining, byteValue, halfwordValue, wordValue) \
    do {                                                                                  \
        switch ((uintptr)(cursor) & (sizeof(u32) - 1)) {                                  \
            case 0:                                                                       \
                *(u32*)(cursor) = (wordValue);                                            \
                (cursor)       += sizeof(u32);                                            \
                (remaining)    -= sizeof(u32);                                            \
                break;                                                                    \
            case 1:                                                                       \
                *(cursor)++     = (byteValue);                                            \
                *(u16*)(cursor) = (halfwordValue);                                        \
                (cursor)       += sizeof(u16);                                            \
                (remaining)    -= 1 + sizeof(u16);                                        \
                break;                                                                    \
            case 2:                                                                       \
                *(u16*)(cursor) = (halfwordValue);                                        \
                (cursor)       += sizeof(u16);                                            \
                (remaining)    -= sizeof(u16);                                            \
                break;                                                                    \
            case 3:                                                                       \
                *(cursor)++  = (byteValue);                                               \
                (remaining) -= 1;                                                         \
                break;                                                                    \
        }                                                                                 \
    } while (0)

    while (bytesRemaining >= sizeof(u32)) {
        // Reach word alignment with narrower stores within the requested extent.
        MEMORY_FILL_ALIGNED_CHUNK(fillCursor, bytesRemaining, fillByte, fillHalfword, fillWord);
    }

#undef MEMORY_FILL_ALIGNED_CHUNK

    // Fewer than four bytes remain; the 16-bit counter view cannot wrap here.
    tailByteIndex = 0;
    while ((u16)tailByteIndex < bytesRemaining) {
        *fillCursor++ = fillByte;
        tailByteIndex++;
    }
}

/// Zeroes the requested payload bytes of an allocation.
///
/// `allocation` must provide `sizeBytes` writable bytes; its address may have
/// any byte alignment. A zero byte count performs no access. The allocation's
/// rounded-up tail and heap metadata are left intact; ownership stays with
/// the caller.
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

    /// Clears bytes through the next word boundary using aligned stores.
    ///
    /// Consumes 4, 3, 2 or 1 bytes for address residues 0, 1, 2 or 3 modulo four.
    /// Advances `cursor` to word alignment and subtracts the cleared byte count
    /// from `remaining`. Requires `remaining >= sizeof(u32)` and that many
    /// writable bytes at `cursor`.
    ///
    /// `cursor` and `remaining` must be stable modifiable `u8*` and `size_t`
    /// lvalues, distinct and outside the cleared storage. Expressions must have no
    /// evaluation side effects; `cursor` is evaluated repeatedly. `wordZero`
    /// and `halfwordZero` must be zero-valued `u32` and `u16` expressions; only
    /// the selected store value is evaluated, at most once. No surrounding
    /// locals or configuration macros are required.
#define MEMORY_CLEAR_ALIGNED_CHUNK(cursor, remaining, wordZero, halfwordZero) \
    do {                                                                      \
        switch ((uintptr)(cursor) & 3) {                                      \
            case 0:                                                           \
                *(u32*)(cursor) = (wordZero);                                 \
                (cursor)       += sizeof(u32);                                \
                (remaining)    -= sizeof(u32);                                \
                break;                                                        \
                                                                              \
            case 1:                                                           \
                *(cursor)++     = 0;                                          \
                *(u16*)(cursor) = (halfwordZero);                             \
                (cursor)       += sizeof(u16);                                \
                (remaining)    -= 1 + sizeof(u16);                            \
                break;                                                        \
                                                                              \
            case 2:                                                           \
                *(u16*)(cursor) = (halfwordZero);                             \
                (cursor)       += sizeof(u16);                                \
                (remaining)    -= sizeof(u16);                                \
                break;                                                        \
                                                                              \
            case 3:                                                           \
                *(cursor)++  = 0;                                             \
                (remaining) -= 1;                                             \
                break;                                                        \
        }                                                                     \
    } while (0)

    while (bytesRemaining >= sizeof(u32)) {
        // Reach word alignment with narrower stores, all within the requested extent.
        MEMORY_CLEAR_ALIGNED_CHUNK(clearCursor, bytesRemaining, zeroWord, zeroHalfword);
    }

#undef MEMORY_CLEAR_ALIGNED_CHUNK

    // Fewer than four bytes remain; the 16-bit counter view cannot wrap here.
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
/// `memInitAuxHeap`.
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

void* memMalloc(size_t sizeBytes, bool auxHeap)
{
    void* allocation;
    void* heapBase;

    // Selection resets the search cursor; the selected base must stay in its ring.
    heapBase = auxHeap == true ? gMemActiveAuxHeap : gMemPrimaryHeapBase;
    _freep   = heapBase;

    allocation = malloc3(sizeBytes);
    if (allocation == NULL) {
        printf("gmalloc-->NULL\n");
    }
    return allocation;
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

void memInitAuxHeap(void)
{
    InitHeap3(gMemActiveAuxHeap, GActiveAuxHeapSize);
}

void Mem_Init()
{
    InitHeap3(gMemActiveAuxHeap, GActiveAuxHeapSize);
    InitHeap3(gMemPrimaryHeapBase, G_HEAP_SIZE);
}

/// Unreferenced no-op retained at its original position in the resident image.
///
/// Its original purpose is unproven.
static void _memNoop(void)
{
}

void memSelectAuxHeapRegion(bool configuredAuxHeap)
{
    // Only the low halfword selects a view; other values leave the pair intact.
    switch ((u16)configuredAuxHeap) {
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
