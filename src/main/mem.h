#ifndef MAIN_PRIVATE_MEM_H
#define MAIN_PRIVATE_MEM_H

#include <psyq/sys/types.h>

#include "types.h"

/// Bytes reserved for GPU primitives when image memory is configured.
enum {
    MEMORY_PRIMITIVE_HEAP_BYTES = 0x10000,
};

/// Clears a caller-owned byte span without requiring alignment.
///
/// `destination` must provide `byteCount` writable bytes, in [0, UINT32_MAX].
/// A zero count performs no access and permits NULL. Borrows storage only during
/// this call and retains no pointer; the local byte cursor views the complete
/// requested object representation, including any bytes between its fields.
/// Each including translation unit has a private inline instance.
static inline void _memClearByteSpan(void* destination, u32 byteCount)
{
    u8* clearCursor = destination;
    u32 byteIndex;

    for (byteIndex = 0; byteIndex < byteCount; byteIndex++) {
        *clearCursor++ = 0;
    }
}

/// Fixed byte address of the resident primary heap's allocator storage.
///
/// Initializes the published heap base; it neither reserves nor initializes RAM.
/// Heap3 setup supplies the separate 0xFF80-byte extent and allocator metadata.
#define MEMORY_PRIMARY_HEAP_BASE ((void*)0x80083800)

/// Base of the fixed primary heap used by the resident allocation wrappers.
///
/// The writable RAM region [0x80083800, 0x80093780) contains 0xFF80 bytes,
/// including PsyQ heap3 bookkeeping. `memInitHeaps` initializes it before use.
/// Allocations with `auxHeap == false` and releases through `memFree` use this
/// heap. Its base stays fixed for the program's lifetime while the allocator's
/// free-list cursor moves within it; the storage belongs to the allocator.
extern void* gMemPrimaryHeapBase;

/// Saved auxiliary portion of image memory, selected by the true heap selector.
///
/// Ordinary configuration starts it after the primitive reservation and extends
/// through the resident image workspace; the initial active heap stops before
/// that workspace. Frame capture instead saves a suffix of the workspace after
/// its pixels. This is borrowed raw storage, requiring heap3 initialization
/// after selection and before allocation; it is separate from the primary heap.
extern void* gMemSavedAuxHeap;

/// Saved auxiliary-portion extent in bytes, including allocator metadata.
///
/// Paired with `gMemSavedAuxHeap`. Ordinary configuration includes the image
/// workspace; frame capture records 0x10000 minus the selected suffix offset.
/// Selection copies this extent rather than recalculating available space.
extern size_t gMemSavedAuxHeapBytes;

/// Saved complete image-memory region, selected by the false heap selector.
///
/// Ordinary configuration includes the primitive reservation and resident image
/// workspace. After capture this view is only the 0x10000-byte workspace after
/// the captured pixels. Selecting it for heap3 can overwrite GPU/image storage,
/// whose previous users must have finished; selection does not initialize it.
extern void* gMemSavedImageRegion;

/// Saved complete image-memory view's byte extent, including heap3 metadata.
///
/// Paired with `gMemSavedImageRegion`: the configured slot extent plus 0x26000
/// ordinarily, or the 0x10000-byte workspace after an area's captured frame.
extern size_t gMemSavedImageRegionBytes;

#endif // MAIN_PRIVATE_MEM_H
