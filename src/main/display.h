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

void Gpu_InitOtSmall(void);

void Gpu_InitOt(void);

/// Build and present the task-owned frame, restoring the caller's current OT.
///
/// `frameStart` is the VSync(1) horizontal-line counter origin; the return is
/// the next origin, possibly negative to compensate for callback time.
/// The OT-buffer and buffer-index arguments are retained and unused.
s32 Display_FrameFlipDraw(GsOT* otBufs, s32 frameStart, s32 unused3);

s32 Display_DispatchModeId(s32 arg0);

/// Put draw/disp env and optionally transfer framebuffer strips (gamemain.c).
void Display_FlipDraw(s32 bufferIndex);

void Display_SetPrimBufLarge(void);

void Display_SetPrimBufSmall(void);

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
