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

/// Initializes the primary and the auxilary heap.
void Mem_Init(void);

/// Initializes the auxilary heap.
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
/// Is equivalent to `Mem_Free2(ptr, false)`.
///
/// @param ptr Pointer to the data to be freed.
void Mem_Free(void* ptr);

/// Frees an allocated pointer.
///
/// Prior to freeing the data, it sets the active the heap.
/// See `Mem_SetActiveHeap` for more details.
///
/// @param ptr Pointer to the data to be freed.
/// @param auxHeap Controlls from which heap the pointer must be freed.
void Mem_Free2(void* ptr, bool auxHeap);

/// Switches between the primary heap and the auxilary heap.
///
/// A pointer to the axuilary heap is stored in `GActiveAuxHeap` and
/// has a capacity of `GActiveAuxHeapSize` bytes. This function either
/// enables the primary heap, pointed to by `GHeap`, or the auxilary
/// heap. After calling this function, the memory management utilities,
/// like `malloc3` and `free3`, will operate on one of the two heaps.
/// Only version 3 utilities are affected.
///
/// @param auxHeap If `true`, the auxilary heap will be set as active.
///                Otherwise the primary one.
void Mem_SetActiveHeap(bool auxHeap);

// TODO: Swaps between two auxilary heaps?
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

/// Pointer to the start of the game heap.
extern u8* GHeap;

/// Pointer to the auxilary heap.
extern u8* GAuxHeap;

/// Length in bytes of the heap pointed to by `GAuxHeap`.
extern size_t GAuxHeapSize;

extern size_t D_80068F88;

/// Pointer to the active auxilary heap.
extern u8* GActiveAuxHeap;

extern size_t D_80068F90;

/// Length in bytes of the heap pointed to by `GActiveAuxHeap`.
extern size_t GActiveAuxHeapSize;

extern int    D_80068F98;
extern u8*    D_800691F4;
extern size_t D_800691F8;

#endif // MEM_H
