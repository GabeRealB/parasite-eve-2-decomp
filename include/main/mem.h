#ifndef MEM_H
#define MEM_H

#include "common.h"

/// Size of the game heap.
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
/// Prior to allocating the data, it sets the active the heap.
/// See `Mem_SetActiveHeap` for more details.
///
/// @param size Number of bytes to allocate.
/// @param auxHeap Controlls in which heap to allocate the data block.
/// @return Allocated block or `NULL`.
void* Mem_Malloc(size_t size, bool auxHeap);

/// Allocates a zeroed block of memory.
///
/// Prior to allocating the data, it sets the active the heap.
/// See `Mem_SetActiveHeap` for more details.
///
/// @param size Number of bytes to allocate.
/// @param auxHeap Controlls in which heap to allocate the data block.
/// @return Allocated block or `NULL`.
void* Mem_Calloc(size_t size, bool auxHeap);

/// Frees an allocated pointer.
///
/// Prior to freeing the data, the primary heap is set to be active.
/// Is equivalent to `memFreeFromHeap(ptr, false)`.
///
/// @param ptr Pointer to the data to be freed.
void Mem_Free(void* ptr);

/// Frees a block, returning it to the heap `auxHeap` selects.
///
/// A block has to be released to the heap it was taken from, so the caller
/// says which one instead of the function assuming the primary heap.
/// `Mem_Free` is the primary-heap-only form.
///
/// @param ptr Pointer to the data to be freed.
/// @param auxHeap If `true`, the block is freed from the active auxiliary
///                heap, otherwise from the primary one.
void memFreeFromHeap(void* ptr, bool auxHeap);

/// Switches between the primary heap and the auxiliary heap.
///
/// The primary heap is the fixed region at `gMemHeap`; the auxiliary heap is
/// the region `gMemActiveAuxHeap` currently points at, with the extent
/// `GActiveAuxHeapSize`. After calling this function, the memory management
/// utilities, like `malloc3` and `free3`, will operate on one of the two
/// heaps. Only version 3 utilities are affected.
///
/// @param auxHeap If `true`, the auxiliary heap will be set as active.
///                Otherwise the primary one.
void Mem_SetActiveHeap(bool auxHeap);

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

/// Base address of the primary heap, the region the game allocates from
/// while no aux heap is active.
///
/// The region is fixed, so its extent is the `G_HEAP_SIZE` constant rather
/// than a size variable of the kind the aux heaps carry.
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
