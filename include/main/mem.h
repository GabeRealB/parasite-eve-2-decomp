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
/// reservation; `memSelectAuxHeapRegion` can select the whole image-memory region.
/// This storage is separate from the fixed primary heap. Selecting a region
/// only updates the base/size pair; `Mem_Init` or `memInitAuxHeap` initializes a
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

/// Initializes or resets the currently selected auxiliary heap3 region.
///
/// Uses `gMemActiveAuxHeap` and its byte extent, `GActiveAuxHeapSize`, including
/// allocator metadata. For a non-null base and nonzero extent, the base must be
/// word-aligned writable storage and the extent must be at least eight bytes.
/// Heap3 rounds the extent down to eight-byte units, creates its free-block ring
/// within that storage and makes it the allocator's active ring.
///
/// Resetting invalidates existing allocations in this region. Previous heap,
/// GPU and image-decoding uses of repurposed storage must have ended. A null base
/// or zero extent leaves storage and the allocator cursor untouched; it does
/// not establish an empty heap. The configured pair and primary heap are intact.
void memInitAuxHeap(void);

/// Allocates one block from the selected heap without clearing its payload.
///
/// `sizeBytes` is a byte count, not an element count. Heap3 rounds the payload
/// up to eight-byte units and reserves another eight bytes for its block header.
/// Payload alignment follows the heap base. A zero-byte request or allocation
/// failure prints a diagnostic and returns `NULL`; the caller handles failure.
/// Requests must not exceed 0xFFFFFFF8 bytes, so eight-byte rounding cannot wrap.
///
/// `auxHeap == true` selects the currently configured auxiliary heap; every
/// other value selects the primary heap. For a nonzero request, that heap must
/// be initialized and its base must remain in its free-block ring. Selection
/// resets the allocator's search cursor before allocation and is not restored,
/// even on failure. Allocations must preserve the base's ring membership for
/// subsequent heap operations.
///
/// The caller owns the block until release to the same heap: use `memFree`
/// for primary allocations or `memFreeFromHeap` for auxiliary allocations.
/// Keep the auxiliary region configured and its contents intact while its
/// allocations are live. Reinitializing or repurposing a heap invalidates them.
void* memMalloc(size_t sizeBytes, bool auxHeap);

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
/// block, such as one returned by `memMalloc` or `memCalloc` with
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

/// Selects a saved image-memory region for auxiliary heap operations.
///
/// `configuredAuxHeap == true` selects the saved auxiliary portion; `false`
/// selects the saved whole region. `memConfigureImageMemory` saves the portion
/// after the GPU primitive reservation, including the resident image workspace;
/// frame capture instead saves a portion of its workspace after the pixels.
/// Both views have runtime bases and byte extents, including allocator metadata,
/// and are independent of the fixed primary heap. Only the low 16 bits of the
/// selector are examined; values other than zero or one leave the pair intact.
///
/// The saved views must already be configured. Selection only copies the base
/// and extent into `gMemActiveAuxHeap` and `GActiveAuxHeapSize`; it leaves heap3
/// metadata and its search cursor untouched. Call `memInitAuxHeap` before using
/// a newly selected nonempty region. Allocations and GPU/image data discarded by
/// repurposing that storage must no longer be in use.
void memSelectAuxHeapRegion(bool configuredAuxHeap);

/// Configures the image-memory region, GPU primitive reservation and auxiliary heaps.
///
/// `stageId` is in 0..5. Stage 0 or `DISPLAY_VIDEO_NORMAL` selects the default
/// region [0x80179950, 0x801FD000), ignoring `areaId`. Otherwise the selected
/// stage's map overlay must be loaded and `areaId` must select a nonempty
/// image slot. Its word-aligned region must provide at least 0x10000 bytes
/// before the resident image workspace. No index or extent checks are performed.
///
/// Reserves the first 0x10000 bytes for primitives and selects the following
/// bytes up to 0x801D7000 as the active auxiliary heap. The saved whole region
/// and saved auxiliary portion extend another 0x26000 bytes past the slot extent,
/// through the resident image buffers and following storage to 0x801FD000.
/// `memSelectAuxHeapRegion` can select either saved view later.
/// Clears the primitive reservation's last ten bytes and records their address.
///
/// Previous allocations and GPU/image operations in repurposed storage must
/// have ended. This only configures the regions: call `Mem_Init` or `memInitAuxHeap`
/// before using a nonempty auxiliary heap, and reset the primitive cursor before
/// drawing. The fixed primary heap is independent of this layout.
void memConfigureImageMemory(s32 stageId, s32 areaId);

/// Copies the low 16 bits of a byte count from source to destination, advancing forward.
///
/// The copied regions must be readable and writable for `sizeBytes & 0xFFFF`
/// bytes. For an effective count of at least four, both addresses must have the same
/// residue modulo four: source alignment selects byte, halfword and word
/// accesses for both pointers. Smaller copies use bytes only. There is no
/// overlap handling; use disjoint regions. A zero effective count accesses
/// neither region. Source comes first, unlike the SDK's memcpy interface.
void memCopyBytes(const void* source, void* destination, u32 sizeBytes);

#endif // MAIN_MEM_H
