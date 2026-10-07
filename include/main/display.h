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

/// Discards the primitives linked into one resident frame ordering table.
///
/// `bufferIndex` selects buffer 0 or 1. Rebuilds all 1088 tags as a reverse
/// DMA chain ending at the first tag, including the reserved foreground tags.
/// Call once GPU drawing has finished using the selected table. The current
/// ordering-table base and primitive storage retain their existing values.
void gpuClearFrameOrderingTable(s16 bufferIndex);

/// Configures the resident framebuffer layout and draw/display environments.
///
/// `setupBits` uses `DISPLAY_SETUP_*`: bits 4..7 select widths 256/320/384/512/640
/// pixels (indices 0..4), and bits 0..3 select 240/480 lines (indices 0..1).
/// Callers must supply valid indices; there is no bounds check. Zero low 16 bits
/// selects `DISPLAY_SETUP_DEFAULT` (320x240); higher bits are ignored.
/// Any bit in 8..11 requests interlace; the live save's preference can force it.
///
/// The 240-line layout alternates VRAM regions at y=0 and y=272, drawing into
/// the region opposite the displayed one. The 480-line layout shares y=0 for
/// both buffers and disables drawing into the displayed area. Drawing origins
/// are centered in each region. `DISPLAY_SETUP_RGB24` enables 24-bit display
/// only in the 240-line layout; the 480-line layout keeps the SDK's 16-bit mode.
/// Dithering is enabled in both layouts. Clearing is black unless
/// `DISPLAY_SETUP_NO_CLEAR` disables it. View and default lights are reset
/// unless `DISPLAY_SETUP_KEEP_VIEW` is set.
///
/// Replaces both environment pairs in `gDisplayState` in place; subsequent
/// presentation applies the rebuilt environments to the GPU.
void displayConfigureFramebuffers(s32 setupBits);

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

/// Spawns a bank entry or direct descriptor to take over frame presentation.
///
/// Requires game-loop ownership and an empty or disposable display-task list.
/// Other owners reject the request with NULL and no changes. `bank`, `selector`
/// and both payload words follow `taskSpawn`'s bounds and lifetime contract:
/// banks 0..14 select an unchecked index; a negative bank uses a descriptor
/// pointer. Reads the descriptor synchronously without invoking its callback.
///
/// Configures two 64-tag OTs and the 0x6000-byte static primitive arena, starts
/// the task framebuffer opposite `drawBuffer`, and initializes the display list.
/// GPU use of this storage and tasks on the old list must already have ended.
/// On success, requests bare-OT mode and task-owned full presentation; the
/// callback runs in a subsequent display-list walk. It must arrange completion
/// and return presentation through `displayResumeGameLoop` when appropriate.
///
/// Returns the task or NULL on allocation/body-attachment failure. Failure
/// retains the new buffer bindings, framebuffer selection and empty list, but
/// leaves presentation ownership and the pending mode unchanged. Restores the
/// previously selected execution list on either spawn result. No arena is
/// allocated or freed; pointer payload ownership belongs to the callback.
Task* displaySpawnTask(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

/// Spawns an indexed descriptor to take over frame presentation.
///
/// `table[index]` must be a live non-terminator descriptor; the signed element
/// index is unchecked. Follows `taskSpawnFromTable`'s payload and resource
/// lifetime contract. The descriptor is read synchronously and not retained.
/// Uses the same ownership gate, buffer preparation, deferred callback dispatch,
/// success handoff, failure side effects and list restoration as `displaySpawnTask`.
Task* displaySpawnTaskFromTable(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);

/// Drawing policy selected while a display-mode task presents the frame.
enum {
    DISPLAY_TASK_DRAW_CLEAR      = 0, // Task OT over a black clear, with no background image
    DISPLAY_TASK_DRAW_ROOM       = 1, // Task OT over the captured room image, without clearing
    DISPLAY_TASK_DRAW_TRANSITION = 2, // Task OT over transition strips, without clearing
    DISPLAY_TASK_DRAW_HOLD       = 3, // Hold presentation; retain the image and clear-colour settings
};

/// Selects task-owned presentation, its background image and its clear policy.
///
/// `drawMode` is a `DISPLAY_TASK_DRAW_*` selector. Hold changes only the flip
/// mode; other unrecognized values leave all settings intact. No frame is
/// presented and no display ownership is acquired by this call.
void displaySetTaskDrawMode(s32 drawMode);

/// Presentation mode stored for a queued mode task.
///
/// Modes 1, 3 and 4 keep the room's current resources. Mode 0, mode 2 and every
/// mode from 5 up reload them. A default request in the Acropolis plaza is
/// stored as mode 1.
enum {
    STAGE_ENTRY_RELOAD        = 0,     // Capture the frame, reset, and reload room resources
    STAGE_ENTRY_KEEP          = 1,     // Keep resources; full flip and image strips
    STAGE_ENTRY_HOLD          = 3,     // Keep resources; hold the flip and draw no image
    STAGE_ENTRY_DRAW_ACTORS   = 4,     // Keep resources, draw active actors, flip during a view transition
    STAGE_ENTRY_GRAY_CAPTURE  = 0x100, // Reload path, then invert the captured RAM image to grey
    STAGE_ENTRY_RELOAD_FORCED = 0x102, // Reload without grey conversion, including in the Acropolis plaza
};

/// Queues a display-mode task and its presentation policy for asynchronous spawning.
///
/// Borrows entry 0 of `descriptor` and copies the two spawn words. Keep the
/// descriptor, its callback and any pointed-to payload loaded until the task
/// has finished using them. A pending display-mode request rejects this call
/// without changing it. Otherwise the transition context and fade are reset;
/// `STAGE_ENTRY_RELOAD` in the Acropolis plaza becomes `STAGE_ENTRY_KEEP`.
/// Other entry modes follow the resource rules above.
///
/// Always returns `NULL`, including after accepting the request. This is not
/// the spawned task pointer and does not report whether the request was accepted.
Task* displayQueueModeTask(TaskDesc* descriptor, s32 spawnArg1, TaskSpawnArg spawnArg2, s32 entryMode);

/// Resets queued GPU work and forgets attached-model buffers before image-memory reuse.
///
/// Uses the command-queue reset mode and clears both resident frame ordering
/// tables. Every attached model's buffer pointer becomes NULL, regardless of
/// its draw or automatic-allocation flags; no heap block is individually freed.
/// Models, coordinates, sources, list links and half selectors stay intact.
/// Retire other users of the discarded buffers and reset or repurpose their
/// auxiliary storage before allocating replacements. This does not initialize
/// a heap or restore buffers; `tmdResetAuxHeapAndRestoreBuffers` performs those
/// steps after the desired image-memory region has been selected.
void gpuResetAndInvalidateModelBuffers(void);

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

/// Acquires one hold on normal menu-mode requests.
///
/// Balance each acquisition with `displayReleaseMenuHold`. If the active bit
/// is clear, sets it and starts the byte count at one, preserving the low bits.
/// Otherwise increments the count, including the initial active/zero-count state.
/// An active count must be below 255 before acquisition: incrementing 255 wraps
/// to zero without clearing the gate. No overflow check is performed.
///
/// Holds delay queued menu requests while the game loop owns presentation;
/// requests with their high bit set bypass the gate. This does not stop task
/// execution, acquire a resource reference or take presentation ownership.
void displayAcquireMenuHold(void);

/// Releases one hold on normal menu-mode requests.
///
/// Balance `displayAcquireMenuHold` calls and keep outstanding holds within 1..255.
/// The last release clears the active hold bit while preserving all other
/// hold-state bits. If the hold bit is already clear, the count is reset to zero.
/// An active hold with a zero count wraps to 255 instead of opening the gate.
/// Queued menu transitions are dispatched by the main loop; requests with their
/// high bit set bypass this gate.
void displayReleaseMenuHold(void);

#endif // MAIN_DISPLAY_H
