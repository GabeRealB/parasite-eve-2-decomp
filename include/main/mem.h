#ifndef MAIN_MEM_H
#define MAIN_MEM_H

#include <psyq/sys/types.h>

#include "types.h"

extern u8* Gpu_PrimHeapBase;

extern size_t Gpu_PrimHeapSize;

/// Base of the RAM region selected for auxiliary allocations and image decoding.
///
/// `GActiveAuxHeapSize` is its heap extent in bytes, including heap3 metadata.
/// Image-memory configuration normally selects storage after the GPU primitive
/// reservation; `Mem_SetActiveAuxHeap` can select the whole image-memory region.
/// This storage is separate from the fixed primary heap. Selecting a region
/// only updates the base/size pair; `Mem_Init` or `Mem_InitAux` initializes a
/// nonempty region before heap operations.
///
/// Stage image decoding writes expanded DCT data directly at this base for
/// MDEC. Reinitializing or repurposing the storage invalidates its allocations;
/// the heap must be reinitialized after decoding before allocation resumes.
extern void* gMemActiveAuxHeap;

/// Length in bytes of the heap pointed to by `gMemActiveAuxHeap`.
extern size_t GActiveAuxHeapSize;

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

/// Releases an allocation from the primary heap.
///
/// `allocation` must be `NULL` or the original pointer to a live primary-heap
/// block, such as one returned by `Mem_Malloc` or `memCalloc` with
/// `auxHeap == false`. The caller owns cleanup of separately allocated data
/// and list links; releasing a non-null block ends its lifetime.
///
/// The primary heap becomes active even for `NULL`, which releases no block.
/// The previous heap selection is not restored. Use `memFreeFromHeap` for an
/// auxiliary-heap allocation.
void memFree(void* allocation);

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

#endif // MAIN_MEM_H
