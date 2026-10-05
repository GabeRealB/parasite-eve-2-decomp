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

/// Four-byte-aligned byte-offset mask for the 1024 depth-sorted ordering-table tags.
///
/// Callers scale a sorting depth by `otDepthShift`, shift that value right by
/// two, and apply this mask. It keeps bits 2..11, so the result is a
/// byte offset from 0 through 4092 and selects tag 0 through 1023 relative to
/// the current ordering-table base. Bits above that window wrap into it.
/// Foreground packets use negative tag indices instead. The 64-tag display
/// tables are a separate extent. Dividing the masked offset by the tag size
/// selects that same tag.
enum { GPU_ORDERING_TABLE_DEPTH_BYTE_MASK = 0xFFC };

/// Tag bits for the next packet's 24-bit DMA address and the packet's word count.
enum { GPU_DMA_LINK_ADDRESS_MASK  = 0xFFFFFF,
       GPU_DMA_PACKET_LENGTH_MASK = 0xFF000000 };

/// Returns a writable `u_long*` to a DMA tag in the current ordering table.
///
/// `byteOffset` counts bytes from `gGpuCurrentOt`, not tags or camera depth. It
/// must be nonnegative and a multiple of the tag size (4 bytes on PlayStation).
/// Depth callers pass an offset already wrapped with
/// `GPU_ORDERING_TABLE_DEPTH_BYTE_MASK`; this accessor neither masks nor clamps
/// it. Signed tag indices, including reserved foreground entries, use
/// `gGpuCurrentOt + index` instead: division by unsigned `sizeof` would
/// convert a negative byte offset to unsigned.
///
/// The selected table must contain the tag, including any subsequent pointer
/// adjustment in tags. The pointer borrows the current table's lifetime and
/// must not be retained across table changes. The macro captures `gGpuCurrentOt`
/// without changing it and evaluates `byteOffset` once per expansion. `addPrim`
/// and `addPrims` expand their OT argument twice; their offsets must have no side
/// effects and their table base must remain stable while linking the packet.
#define GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(byteOffset) (&gGpuCurrentOt[(byteOffset) / sizeof(*gGpuCurrentOt)])

/// A writable `u32` view of a primitive's packed colour group.
///
/// On PlayStation, bits 0..7, 8..15 and 16..23 hold red, green and blue.
/// Bits 24..31 hold the packet's command byte for vertex 0 and padding for
/// later Gouraud vertices; compatible sprite source records hold flags there.
/// Reads and writes include all four bytes. An RGB-only value clears the command
/// byte at vertex 0, so set the packet code afterwards or include it in the word.
/// `GPU_PACK_COLOR_WORD` packs the four bytes.
///
/// `primitive` is evaluated once and must point to a mutable record with a
/// word-aligned four-byte group starting at the selected `rN` member.
/// `vertexIndex` is a literal suffix token pasted onto `r`, not a runtime index
/// or an expanded macro: use only a colour member present in the record
/// (0 for flat primitives, 0..2/0..3 for Gouraud triangles/quads).
/// This accessor captures no identifiers and does not allocate or retain storage.
#define GPU_PRIMITIVE_COLOR_WORD(primitive, vertexIndex) (*(u32*)&((primitive)->r##vertexIndex))

/// A writable `u32` view of one vertex's packed screen coordinates.
///
/// On PlayStation, bits 0..15 hold signed `xN` and bits 16..31 hold signed
/// `yN`, in pixels. That is the word a GTE screen-XY register holds and the
/// word `RotTransPers4` returns. Compatible sprite source records use the same
/// `x0`/`y0` pair for the rectangle origin. Reads and writes include both
/// halves, so a projected point moves as one word.
///
/// `primitive` is evaluated once and must point to a mutable record whose
/// selected `xN` is immediately followed by `yN` and begins on a four-byte
/// boundary. `vertexIndex` is a literal suffix token pasted onto `x`, not a
/// runtime index or an expanded macro: use only a coordinate pair present in
/// the record (0 for a sprite, 0..1 for a two-point line, 0..2 for a triangle,
/// 0..3 for a quad).
/// This accessor captures no identifiers and does not allocate or retain storage.
#define GPU_PRIMITIVE_XY_WORD(primitive, vertexIndex) (*(u32*)&((primitive)->x##vertexIndex))

/// Packs RGB and a command byte into a `u32` GPU colour word.
///
/// Red, green and blue occupy bits 0..7, 8..15 and 16..23; `commandByte`
/// occupies bits 24..31. Pass the packet's complete command for vertex 0,
/// or zero for the unused high byte of later Gouraud vertices. Assigning the
/// result to `GPU_PRIMITIVE_COLOR_WORD` replaces all four bytes. When writing
/// vertex 0 with a zero command, set the packet code afterwards before linking
/// it for drawing.
///
/// All arguments must be integer byte values (0..255); they are not masked or
/// clamped. Conversion to `u32` before shifting keeps every shift unsigned.
/// Each argument is evaluated once, with no specified relative evaluation order.
/// The macro captures no identifiers and is a constant expression when its
/// arguments are integer constant expressions.
#define GPU_PACK_COLOR_WORD(red, green, blue, commandByte) \
    ((u32)(red) | ((u32)(green) << 8) | ((u32)(blue) << 16) | ((u32)(commandByte) << 24))

/// Resident presentation, frame clocks and display/session controls shared by all overlays.
///
/// The game loop clears this object in place on initialization and soft reset;
/// deterministic replay also resets its clocks. Its storage lasts for the
/// resident executable's lifetime, while its selections and controls change
/// between frames and during task-owned presentation. Drawing consumers use
/// `drawBuffer` (0 or 1) for the current frame's resources; `otBuffer` and
/// `frameBuffer` belong to their respective presentation paths.
extern DisplayState gDisplayState;

extern GsOT Gpu_OtBuffers[2];

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

/// Selectors for one-, two- and three-vblank nominal frame timing.
enum {
    DISPLAY_TIMING_EVERY_VBLANK  = 0,
    DISPLAY_TIMING_TWO_VBLANKS   = 1,
    DISPLAY_TIMING_THREE_VBLANKS = 2,
};

/// Selects the animation step and pacing budget for subsequent frames.
///
/// `timingMode` is a `DISPLAY_TIMING_*` selector. The modes set `frameTicks`
/// to 1, 2 or 3 nominal 60-Hz animation ticks, VSync waits to 0 (next blank),
/// 2 or 3, and scanline budgets to 262, 525 or 787 respectively. These budgets
/// are fixed NTSC counts; the video region does not change them. Other values
/// leave all three settings unchanged.
///
/// Both paced presentation paths use the wait and budget; 480-line game-loop
/// presentation always waits for the next blank. The game-loop play-clock step
/// is separate (1, 2 or 2 ticks respectively). This changes settings without
/// waiting, presenting a frame or resetting any clock.
void displaySetFrameTiming(s32 timingMode);

/// Set the persistent vertical screen-shake offset in pixels, clamped to [-8, 8].
///
/// Wider arguments narrow to a signed byte before clamping. Each request
/// replaces the previous one and persists until another call or a display-state
/// reset; zero clears the request. Positive offsets move drawing downward.
/// The 240-line game loop applies the request to both draw origins and the
/// background-image crop after presenting its current frame.
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

/// Returns presentation to the game loop using the current session's image-memory layout.
///
/// Call after task-owned presentation has finished. Requires a live `gGameSession`
/// whose stage and area satisfy `memConfigureImageMemory`'s layout preconditions;
/// operations and allocations using repurposed storage must have ended.
/// Reconfigures image memory, selects `DISPLAY_OWNER_GAME_LOOP` and clears the
/// pending mode request. Auxiliary-heap initialization remains the caller's
/// responsibility; the game loop resets the primitive cursor on its next frame.
void displayResumeGameLoop(void);

/// Acquire a display hold that blocks normal menu-mode requests.
void Display_AcquireRef(void);

/// Releases one hold on normal menu-mode requests.
///
/// Balance `Display_AcquireRef` calls and keep outstanding holds within 1..255.
/// The last release clears the active hold bit while preserving all other
/// hold-state bits. If the hold bit is already clear, the count is reset to zero.
/// An active hold with a zero count wraps to 255 instead of opening the gate.
/// Queued menu transitions are dispatched by the main loop; requests with their
/// high bit set bypass this gate.
void displayReleaseMenuHold(void);

#endif // MAIN_DISPLAY_H
