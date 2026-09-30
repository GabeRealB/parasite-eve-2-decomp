#ifndef MAIN_DISPLAY_H
#define MAIN_DISPLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/display_types.h"
#include "main/task_types.h"

/// Base for linking primitives into the ordering table currently being built.
///
/// Each entry is a packed 32-bit GPU DMA tag. Indices count tags.
/// The normal frame path selects one of two 1088-tag buffers and advances this
/// base by 32 tags; indices -32 through 1055 then lie within that buffer. Depth
/// sorting uses indices 0 through 1023, and negative indices reach the reserved
/// foreground tags. Other display paths select the start of a 64- or 1024-tag
/// table instead, so callers must use indices within the selected table.
///
/// Drawing tasks borrow this base during dispatch. A nested display pass saves
/// and restores it; consumers must not retain it across frame/table changes or
/// free it. The display code owns and clears the backing storage before drawing.
extern u_long* gGpuCurrentOt;

/// Aligned byte-offset mask for the 1024 depth-sorted tags (0 through 4092 bytes).
enum { GPU_ORDERING_TABLE_DEPTH_BYTE_MASK = 0xFFC };

/// Tag bits for the next packet's 24-bit DMA address and the packet's word count.
enum { GPU_DMA_LINK_ADDRESS_MASK  = 0xFFFFFF,
       GPU_DMA_PACKET_LENGTH_MASK = 0xFF000000 };

/// Resolves a nonnegative, tag-aligned byte offset relative to `gGpuCurrentOt`.
///
/// The offset is evaluated once and must address a tag in the selected table.
/// This macro captures the current base; it neither clamps nor masks the offset.
/// `addPrim` evaluates its OT argument twice, so its callers need side-effect-free
/// offsets even when they use this helper.
#define GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(byteOffset) (&gGpuCurrentOt[(byteOffset) / sizeof(*gGpuCurrentOt)])

/// A writable `u32` view of a primitive's packed colour group.
///
/// On PlayStation, bits 0..7, 8..15 and 16..23 hold red, green and blue.
/// Bits 24..31 hold the packet's command byte for vertex 0 and padding for
/// later Gouraud vertices; compatible sprite source records hold flags there.
/// Reads and writes include all four bytes. An RGB-only value clears the command
/// byte at vertex 0, so set the packet code afterwards or include it in the word.
/// `PRIM_RGBC` packs the four bytes.
///
/// `primitive` is evaluated once and must point to a mutable record with a
/// word-aligned four-byte group starting at the selected `rN` member.
/// `vertexIndex` is a literal suffix token pasted onto `r`, not a runtime index
/// or an expanded macro: use only a colour member present in the record
/// (0 for flat primitives, 0..2/0..3 for Gouraud triangles/quads).
/// This accessor captures no identifiers and does not allocate or retain storage.
#define GPU_PRIMITIVE_COLOR_WORD(primitive, vertexIndex) (*(u32*)&((primitive)->r##vertexIndex))

/// Vertex `n`'s position word: `xn` in the low half, `yn` in the high half, the
/// layout the GTE stores a projected point in.
#define PRIM_XY_WORD(p, n) (*(u32*)&(p)->x##n)

/// A colour word from its bytes; `code` is the primitive code for vertex 0 and
/// 0 for the other vertices.
#define PRIM_RGBC(r, g, b, code) \
    ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(code) << 24))

/// Resident presentation, frame clocks and display/session controls shared by all overlays.
///
/// The game loop clears this object in place on initialization and soft reset;
/// deterministic replay also resets its clocks. Its storage lasts for the
/// resident executable's lifetime, while its selections and controls change
/// between frames and during task-owned presentation. Drawing consumers use
/// `drawBuffer` (0 or 1) for the current frame's resources; `otBuffer` and
/// `frameBuffer` belong to their respective presentation paths.
extern DisplayState gDisplayState;

extern GpuOtBuf Gpu_OtBuffers[2];

extern GsOT Gpu_OrderingTables[2];

/// Next free address in the selected frame's GPU packet arena.
///
/// The arena holds mixed primitive and draw-command packets, including merged
/// packets. Borrow this address as the packet's type and advance it by the full
/// reservation, which can exceed the packet written. Byte offsets require a
/// byte-pointer view; there is no common packet stride. Reservations must remain
/// word-aligned and fit in the selected half; allocation does not check capacity.
///
/// The game loop selects a half of `Gpu_PrimHeapBase`, whose total byte capacity
/// is `Gpu_PrimHeapSize`. Task-owned presentation instead selects a half of its
/// configured arena (the 0x6000-byte static arena or 0x10000-byte heap arena).
/// Display code resets the cursor before drawing; packets remain borrowed until
/// GPU drawing completes and that half is reused. Consumers must not free them
/// or retain the cursor across display/frame changes.
extern void* gGpuPrimCursor;

void Gpu_ClearOTag(s16 tableIdx);

/// Configure the two draw/display environments from packed setup bits.
///
/// Width index (bits 4..7) must be 0..4 for 256/320/384/512/640 pixels; height
/// index (bits 0..3) must be 0..1 for 240/480 lines. Any bit in 8..11 requests
/// interlace, and the saved interlace preference can also enable it. Other
/// options are DISPLAY_SETUP_* flags; zero low 16 bits selects the default.
void Display_SetMode(s32 modeBits);

/// Request vertical screen shake in signed pixels, clamped to [-8, 8].
///
/// The game loop applies the request after presenting its current frame.
void displaySetShakeY(s8 offsetY);

Task* Display_SpawnWithOtSmall(s32 arg0, s32 arg1, TaskSpawnArg arg2, TaskSpawnArg arg3);

Task* Display_SpawnWithOt(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, TaskSpawnArg arg3);

void Display_SetDrawMode(s32 arg0);

/// Queues a mode transition; the task is spawned asynchronously, so returns NULL.
Task* Display_InitModeObj(TaskDesc* descriptor, s32 arg1, TaskSpawnArg arg2, s32 arg3);

void Gpu_ResetGraphAndOt(void);

/// Upload the room background into buffer 0 or 1, cropped by the applied shake.
///
/// The CD loader selects a contiguous 320x240 image or twenty 16x240 strips.
/// Source offsets are bytes and remain word-aligned for the GPU transfer.
void Display_LoadImageStrips(s32 bufferIndex);

/// Mem heap reset via session (otutil.c wrapper around Display_ResetHeapFromSession).
void Display_ResetHeapWrapper(void);

/// Acquire a display hold that blocks normal menu-mode requests.
void Display_AcquireRef(void);

/// Release a matching acquisition; the final release unblocks menu requests.
///
/// Acquisitions and releases must be balanced; the count is stored in a byte.
void Display_ReleaseRef(void);

#endif // MAIN_DISPLAY_H
