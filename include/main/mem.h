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

/// Fills a writable memory region with the low byte of `value`.
///
/// `destination` must provide `sizeBytes` writable bytes and may have any
/// byte alignment. A zero byte count performs no access. The count is in
/// bytes, not elements; storage beyond that extent is left intact.
///
/// The fill neither allocates nor releases storage; ownership and lifetime
/// remain with the caller. All bits of `value` above bit seven are ignored.
void memFillBytes(void* destination, u32 value, size_t sizeBytes);

/// Initializes the primary and the auxiliary heap.
void Mem_Init(void);

/// Initializes the auxiliary heap.
void Mem_InitAux(void);

/// Allocates a block of memory.
///
/// Prior to allocating the data, it selects the primary or configured auxiliary
/// heap for the allocator.
///
/// @param size Number of bytes to allocate.
/// @param auxHeap If `true`, the block is allocated from the auxiliary heap,
///                otherwise from the primary one.
/// @return Allocated block or `NULL`.
void* Mem_Malloc(size_t size, bool auxHeap);

/// Allocates one block from the selected heap and clears its requested bytes.
///
/// `sizeBytes` is a byte count, not an element count. The heap uses eight-byte
/// allocation units; any rounded-up payload bytes are not cleared. Allocation
/// alignment follows the heap base. A zero-byte request or allocation failure
/// prints a diagnostic and returns `NULL`.
/// Requests must not exceed 0xFFFFFFF8 bytes, so eight-byte rounding cannot wrap.
///
/// `auxHeap == true` selects the currently configured auxiliary heap; every
/// other value selects the primary heap. For a nonzero request, the selected
/// heap must be initialized and its base must still belong to its free-block
/// ring. Selection replaces the allocator's search cursor before allocation;
/// the previous selection is not restored, even on failure.
///
/// The caller owns the block until release to the same heap: use `memFree`
/// for primary allocations or `memFreeFromHeap` for auxiliary allocations.
/// Keep the auxiliary region configured and its contents intact while its
/// allocations are live. Reinitializing or repurposing a heap invalidates them.
void* memCalloc(size_t sizeBytes, bool auxHeap);

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

/// Releases an allocation to the selected primary or auxiliary heap.
///
/// `allocation` must be `NULL` or the original pointer to a live block from
/// the selected heap. A non-null release requires initialized allocator
/// metadata with the heap base still in its free-block ring. Auxiliary releases
/// require the originating region to be configured, without reinitializing or
/// repurposing its storage while allocations are live.
/// The caller owns cleanup of nested allocations and list links; releasing
/// a non-null block ends its lifetime.
///
/// `auxHeap == true` selects the currently configured auxiliary heap; every
/// other value selects the primary heap. Selection resets the allocator's
/// search cursor to the heap base even for `NULL`, which releases no block.
/// The previous selection is not restored. `memFree` always selects primary.
void memFreeFromHeap(void* allocation, bool auxHeap);

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
