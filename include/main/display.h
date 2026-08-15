#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

#define GPU_OT_ENTRIES  0x440
#define GPU_OT_END_PRIM 0xFFFFFF

// =============================================================================
// Types — dual DISPENV/DRAWENV + system flags
// =============================================================================

/// Dual DISPENV/DRAWENV + system flags. Known fields: see also STRUCT_FIELDS.md.
typedef struct _DisplayState {
    /* 0x000 */ s32         field_0;
    /* 0x004 */ s32         field_4;
    /* 0x008 */ s32         field_8;
    /* 0x00C */ s32         field_c;
    /* 0x010 */ s32         field_10;
    /* 0x014 */ s32         field_14;
    /* 0x018 */ u16         width;
    /* 0x01A */ u16         height;
    /* 0x01C */ u8          interlace;
    /* 0x01D */ s8          field_1d;
    /* 0x01E */ s8          field_1e;
    /* 0x01F */ u8          field_1f;
    /* 0x020 */ DISPENV     dispEnv[2];
    /* 0x048 */ DRAWENV     drawEnv[2];
    /* 0x100 */ u8          field_100;
    /* 0x101 */ u8          field_101;
    /* 0x102 */ byte        unknown_102[0x1];
    /* 0x103 */ u8          field_103;
    /* 0x104 */ u16         field_104;
    /* 0x106 */ u16         field_106;
    /* 0x108 */ volatile u8 vsyncFlag;
    /* 0x109 */ s8          vramYOffset;
    /* 0x10A */ u8          field_10a;
    /* 0x10B */ u8          field_10b;
    /* 0x10C */ byte        unknown_10c[0x1];
    /* 0x10D */ u8          field_10d;
    /* 0x10E */ u16         field_10e;
    /* 0x110 */ byte        unknown_110[0x2];
    /* 0x112 */ s16         field_112;
    /* 0x114 */ s32         field_114;
    /* 0x118 */ s32         frameMode;
    /* 0x11C */ byte        unknown_11c[0x1];
    /* 0x11D */ u8          field_11d;
    /* 0x11E */ u8          field_11e;
    /* 0x11F */ byte        unknown_11f[0x1];
    /* 0x120 */ s16         field_120;
    /* 0x122 */ u8          field_122;
    /* 0x123 */ s8          skipTeardown;
    /* 0x124 */ u16         region;
    /* 0x126 */ s8          field_126;
    /* 0x127 */ byte        unknown_127[0x1];
    /* 0x128 */ u8          field_128; // into draw scratch (model path)
    /* 0x129 */ byte        unknown_129[0x1];
    /* 0x12A */ u16         field_12a;
    /* 0x12C */ u16         field_12c;
    /* 0x12E */ u8          field_12e;
    /* 0x12F */ u8          field_12f;
    /* 0x130 */ u8          field_130;
    /* 0x131 */ byte        unknown_131[0x7];
} DisplayState;
STATIC_ASSERT_SIZEOF(DisplayState, 0x138);

// Types — ordering tables

/// Per-buffer OT context (Gpu_OtBuffers[2]). Indexed by display buffer (stride 0x14).
/// field_4 is OT start; field_10 is the last tag (passed to DrawOTag).
typedef struct _GpuOtBuf {
    /* 0x00 */ s32     depth;
    /* 0x04 */ u_long* ot;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* lastTag;
} GpuOtBuf;
STATIC_ASSERT_SIZEOF(GpuOtBuf, 0x14);

// =============================================================================
// Globals
// =============================================================================

/// Dual-buffer display / system state (bss @ 0x80070F68).
extern DisplayState Display_State;

extern const u16 Display_WidthTable[];
extern const u16 Display_HeightTable[];

extern u_long       Gpu_OtTags[2 * GPU_OT_ENTRIES];
extern GpuOtBuf     Gpu_OtBuffers[2];
extern GsOT         Gpu_OrderingTables[2];
extern u_long*      Gpu_CurrentOt;
extern u8*          D_80070EE0; // primitive buffer cursor
extern DR_TPAGE*    D_80071190; // primitive buffer cursor
extern volatile u8  D_8006EC30; // snapshot of Display_State.field_100 / field_103
extern volatile u8  D_80070E38;
extern volatile s32 D_80070F64; // VSync countdown
extern s8           D_8007272B;
extern s8           D_8007272D;

// --- APIs ---
void  Gpu_ClearOTag(s16 tableIdx);
void  Display_SetMode(s32 arg0);
void  Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2);
void  Display_ClampField126(s8 arg0);
void  Gpu_InitOtSmall(void);
void  Gpu_InitOt(void);
Task* Display_SpawnFromMode(void);
Task* Display_SpawnWithOtSmall(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
Task* Display_SpawnWithOt(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Display_FlipOtAndDispatch(s32 arg0);
void  Display_InvertFramebufferGray(void);
s32   Display_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void  Display_SetFadeMax(u8 arg0);
void  Display_SetDrawMode(s32 arg0);
s32   Display_InitModeObj(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Gpu_ResetGraphAndOt(void);
/// arg2 is unused; GameMain_Loop passes Display_State.field_114 for match.
s32 Display_FrameFlipDraw(s32 arg0, s32 arg1, s32 arg2);
s32 Display_DispatchModeId(s32 arg0);

/// Put draw/disp env and optionally transfer framebuffer strips (gamemain.c).
void Display_FlipDraw(s32 arg0);
/// VSync callback: timed flip / strip load / audio tick (gamemain.c).
void Display_VSyncCallback(void);
/// LoadImage strips from Fs_ImgBuffers into the active display buffer.
void Display_LoadImageStrips(s32 arg0);
/// Mem heap reset via session (otutil.c wrapper around Display_ResetHeapFromSession).
void Display_ResetHeapWrapper(void);
void Display_AcquireRef(void);
void Display_ReleaseRef(void);
void Display_TransitionTask(Task* task);
void Display_TaskLoadStep(Task* task);

#endif // DISPLAY_H
