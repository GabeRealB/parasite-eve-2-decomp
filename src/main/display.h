#ifndef MAIN_PRIVATE_DISPLAY_H
#define MAIN_PRIVATE_DISPLAY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/display_types.h"
#include "display_types.h"

#define GPU_OT_END_PRIM 0xFFFFFF

extern u_long Gpu_OtTags[2 * GPU_OT_ENTRIES];

extern u8* Gpu_SysPrimCursor;

extern u8 Gpu_PrimBufStatic[0x6000];

extern volatile u8 D_8006EC30;

extern volatile u8 D_80070E38;

void Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2);

void Gpu_InitOtSmall(void);

void Gpu_InitOt(void);

/// Renders a frame whose tasks own the display. Only `arg1`, the loop's frame
/// start, is read; the caller also passes the OT buffers and the current buffer.
s32 Display_FrameFlipDraw(GpuOtBuf* otBufs, s32 arg1, s32 unused3);

s32 Display_DispatchModeId(s32 arg0);

/// Put draw/disp env and optionally transfer framebuffer strips (gamemain.c).
void Display_FlipDraw(s32 arg0);

void Display_SetPrimBufLarge(void);

void Display_SetPrimBufSmall(void);

/// Makes buffer `buf`'s ordering table the current one: clears it, terminates
/// it, and leaves the current-table pointer past the entries reserved at its
/// start.
static inline void gpuBeginOt(s32 buf)
{
    // Declared here, not relied on from above, because the unit defining these
    // hides the file-scope externs to keep its own definition order.
    extern u_long* gGpuCurrentOt;
    extern u_long  Gpu_OtTags[];
    u_long*        ot;

    gGpuCurrentOt = Gpu_OtTags + buf * GPU_OT_ENTRIES;
    ClearOTagR(gGpuCurrentOt, GPU_OT_ENTRIES);
    ot            = gGpuCurrentOt;
    *ot           = GPU_OT_END_PRIM;
    gGpuCurrentOt = ot + 0x20;
}

#endif // MAIN_PRIVATE_DISPLAY_H
