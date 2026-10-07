#ifndef MAIN_PRIVATE_DISPLAY_H
#define MAIN_PRIVATE_DISPLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "types.h"

#include "main/display_types.h"
#include "display_types.h"

#define GPU_OT_END_PRIM 0xFFFFFF

extern u_long Gpu_OtTags[2 * GPU_ORDERING_TABLE_BUFFER_ENTRIES];

extern u8* Gpu_SysPrimCursor;

extern u8 Gpu_PrimBufStatic[0x6000];

extern volatile u8 D_8006EC30;

extern volatile u8 D_80070E38;

/// Negative red argument that disables automatic framebuffer clearing.
enum { DISPLAY_CLEAR_DISABLED = -1 };

/// Set the automatic framebuffer-clear colour for both draw buffers.
///
/// Any negative `red` disables clearing without changing the stored colour;
/// callers use `DISPLAY_CLEAR_DISABLED`. Otherwise clearing is enabled and
/// each channel stores the low byte of its argument (normally 0..255).
/// The settings persist until changed here or by display-mode setup.
void displaySetClearColor(s32 red, s32 green, s32 blue);

/// Binds small task-owned ordering tables and the static primitive arena.
///
/// Configures both `Gpu_OrderingTables` slots for 64 tags and a 0x6000-byte
/// primitive arena split into two 0x3000-byte halves. Previous GPU users must
/// have finished. Does not clear tags, select the current OT or primitive cursor,
/// initialize a task list, or change display ownership; the frame path clears
/// and selects a half before drawing. Allocates and releases no storage.
void displayInitTaskBuffers(void);

/// Binds full task-owned ordering tables and clears the selected frame's tags.
///
/// Configures both `Gpu_OrderingTables` slots for 1024 tags in the resident
/// frame-tag buffers, whose stride includes additional foreground storage.
/// `gDisplayState.frameBuffer` must be 0 or 1; previous GPU users of the buffers
/// must have finished. Clears only that slot's 1024-tag table, terminates tag
/// zero and selects it as `gGpuCurrentOt`, without the game loop's 32-tag bias.
/// The primitive arena, task list and display ownership are unchanged.
void gpuInitTaskOrderingTables(void);

/// Build and present the task-owned frame, restoring the caller's current OT.
///
/// `frameStart` is the VSync(1) horizontal-line counter origin; the return is
/// the next origin, possibly negative to compensate for callback time.
/// The OT-buffer and buffer-index arguments are retained and unused.
s32 Display_FrameFlipDraw(GsOT* otBufs, s32 frameStart, s32 unused3);

s32 Display_DispatchModeId(s32 arg0);

/// Presents a task-owned framebuffer using the queued flip and image policy.
///
/// `bufferIndex` selects buffer 0 or 1 with prepared environments and task OT.
/// The scheduling path must snapshot its flip and image controls before calling;
/// the VSync callback also uses those snapshots. Hold mode makes no GPU calls.
/// Full mode restores the background and movie when the image source is nonzero,
/// draws the current game OT, then the task OT. Other modes restore only
/// transition strips or the captured room slot before the task OT. The signed
/// flip byte suppresses the task OT for values 0x10..0x7F; values 0x80..0xFF
/// still draw it unless the low nibble selects hold.
///
/// Previous GPU users of reused storage must have finished; borrowed image,
/// movie and primitive storage must remain valid through transfer and drawing.
void displayPresentTaskFrame(s32 bufferIndex);

/// Selects the image-memory primitive reservation for task-owned drawing.
///
/// `Gpu_PrimHeapBase` must provide `MEMORY_PRIMITIVE_HEAP_BYTES` (0x10000)
/// writable bytes with previous GPU/heap users finished. Subsequent task frames
/// select one 0x8000-byte half. This records borrowed storage without allocating,
/// clearing it or resetting the live cursor; image-memory configuration owns
/// its lifetime. Complete packets must fit in the selected half.
void displayUseHeapTaskPrimitiveBuffer(void);

/// Selects the resident static primitive arena for task-owned drawing.
///
/// Subsequent task frames select one half of the 0x6000-byte arena (0x3000
/// bytes). Previous GPU users must have finished and packets must fit within
/// the selected half. Changes only the arena binding and byte capacity; the
/// current cursor is reset by the next frame. Allocates and releases no storage.
void displayUseStaticTaskPrimitiveBuffer(void);

/// Makes buffer `buf`'s ordering table the current one: clears it, terminates
/// it, and leaves the current-table pointer past the entries reserved at its
/// start. `buf` is the display-buffer index (0 or 1).
static inline void gpuBeginOt(s32 buf)
{
    // Declared here, not relied on from above, because the unit defining these
    // hides the file-scope externs to keep its own definition order.
    extern u_long* gGpuCurrentOt;
    extern u_long  Gpu_OtTags[];
    u_long*        ot;

    // Clear the reverse DMA chain before drawing tasks borrow its depth base.
    gGpuCurrentOt = Gpu_OtTags + buf * GPU_ORDERING_TABLE_BUFFER_ENTRIES;
    ClearOTagR(gGpuCurrentOt, GPU_ORDERING_TABLE_BUFFER_ENTRIES);
    ot            = gGpuCurrentOt;
    *ot           = GPU_OT_END_PRIM;
    gGpuCurrentOt = ot + GPU_ORDERING_TABLE_RESERVED_ENTRIES;
}

#endif // MAIN_PRIVATE_DISPLAY_H
