#ifndef MEM_H
#define MEM_H

#include "common.h"

/// Extent in bytes of the primary heap.
#define G_HEAP_SIZE 0xFF80

/// Scratchpad temporary arena (grows downward from head).
#define G_SCRATCH_HEAD         PSX_SCRATCH_ADDR(0x3FC)
#define GameResetScratchHead() *(void**)G_SCRATCH_HEAD = G_SCRATCH_HEAD

// Types

typedef struct _HeapBlockHeader {
    u32                      size;
    u16                      isAllocated;
    u16                      magic;
    struct _HeapBlockHeader* prev;
    struct _HeapBlockHeader* next;
} HeapBlockHeader;
STATIC_ASSERT_SIZEOF(HeapBlockHeader, 0x10);

/// Optimized `memset` function.
///
/// Copies the value `(u8)ch` into the first `count` bytes of `dest`.
///
/// @param dest Destination buffer to where the value is written.
/// @param ch Character to write into the destination buffer.
/// @param count Number of bytes to write into the destination buffer.
void Mem_Set(void* dest, u32 ch, u32 count);

/// Initializes the primary and the auxiliary heap.
void Mem_Init(void);

/// Initializes the auxiliary heap.
void Mem_InitAux(void);

/// Allocates a block of memory.
///
/// Prior to allocating the data, it makes the heap it allocates from the
/// active one. See `memSetActiveHeap` for more details.
///
/// @param size Number of bytes to allocate.
/// @param auxHeap If `true`, the block is allocated from the auxiliary heap,
///                otherwise from the primary one.
/// @return Allocated block or `NULL`.
void* Mem_Malloc(size_t size, bool auxHeap);

/// Allocates a zeroed block of memory.
///
/// The block is zeroed before it is returned, so a caller can read any of its
/// fields before writing them. A failed allocation is reported and `NULL` is
/// returned.
///
/// An allocation is served from the active heap, so the heap `auxHeap` names is
/// made the active one first; see `memSetActiveHeap`.
///
/// @param size Number of bytes to allocate.
/// @param auxHeap If `true`, the block is allocated from the auxiliary heap,
///                otherwise from the primary one.
/// @return Allocated block or `NULL`.
void* memCalloc(size_t size, bool auxHeap);

/// Frees a block, returning it to the primary heap.
///
/// A block has to be released to the heap it was taken from, so this form is
/// for blocks the primary heap allocated; one taken from the auxiliary heap
/// goes through `memFreeFromHeap`, which names that heap instead of assuming
/// it. The primary heap is left the active one.
///
/// @param ptr Pointer to the data to be freed.
void memFree(void* ptr);

/// Frees a block, returning it to the heap `auxHeap` selects.
///
/// A block has to be released to the heap it was taken from, so the caller
/// names that heap instead of the primary one being assumed. The heap named is
/// left the active one. `memFree` is the primary-heap-only form.
///
/// @param ptr Pointer to the data to be freed.
/// @param auxHeap If `true`, the block is released to the auxiliary heap,
///                otherwise to the primary one.
void memFreeFromHeap(void* ptr, bool auxHeap);

/// Selects the heap the allocation routines operate on.
///
/// `malloc3` and `free3` work inside one heap at a time, and a block has to be
/// released to the heap it came from, so the heap in play is set before each
/// operation rather than once at start-up. The primary heap is the fixed
/// region at `gMemHeap`; the auxiliary heap is the region `gMemActiveAuxHeap`
/// currently points at.
///
/// @param auxHeap If `true`, the auxiliary heap becomes the active one,
///                otherwise the primary heap.
void memSetActiveHeap(bool auxHeap);

/// Selects which region serves as the auxiliary heap.
///
/// Passing `true` selects the region beyond the primary heap, `false` the
/// whole of the memory reserved for image data.
void Mem_SetActiveAuxHeap(bool aux0);

/// Alloc aux buffer and optionally MoveImage two VRAM strips (src/main/stream.c).
void Mem_AllocAuxWithImages(s16 flags);

/// Configure the aux heap from a Gfx image-slot table (implemented in boot.c).
void Mem_ConfigureAuxHeap(s32 arg0, s32 arg1);

/// Byte copy that does not require aligned src/dest (implemented in task.c).
void Mem_CopyUnaligned(void* src, void* dest, u32 count);

// =============================================================================
// Globals — primary / aux heaps
// =============================================================================

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

extern size_t Gpu_PrimHeapBase;
extern size_t Gpu_PrimHeapSize;

/// Base address of the auxiliary heap the game is currently allocating from.
///
/// The auxiliary heap is not one fixed region: the game can make the whole of
/// the memory reserved for image data available to it, or only the part of
/// that memory beyond the primary heap, and `Mem_SetActiveAuxHeap` switches
/// between the two.
extern u8* gMemActiveAuxHeap;

/// Length in bytes of the heap pointed to by `gMemActiveAuxHeap`.
extern size_t GActiveAuxHeapSize;

extern int    D_80068F98;
extern u8*    D_800691F4;
extern size_t D_800691F8;

#endif // MEM_H
