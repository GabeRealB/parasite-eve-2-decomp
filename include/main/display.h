#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/task.h"

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
    /* 0x018 */ u16         field_18; // display width
    /* 0x01A */ u16         field_1a; // display height
    /* 0x01C */ u8          field_1c; // interlace enable
    /* 0x01D */ s8          field_1d;
    /* 0x01E */ s8          field_1e;
    /* 0x01F */ u8          field_1f;
    /* 0x020 */ DISPENV     field_20[2]; // dual display env
    /* 0x048 */ DRAWENV     field_48[2]; // dual draw env
    /* 0x100 */ u8          field_100;
    /* 0x101 */ u8          field_101;
    /* 0x102 */ byte        unknown_102[0x1];
    /* 0x103 */ u8          field_103;
    /* 0x104 */ u16         field_104;
    /* 0x106 */ u16         field_106;
    /* 0x108 */ volatile u8 field_108; // written by main, read by VSync cb
    /* 0x109 */ s8          field_109; // VRAM Y offset for image transfer
    /* 0x10A */ u8          field_10a;
    /* 0x10B */ u8          field_10b;
    /* 0x10C */ byte        unknown_10c[0x1];
    /* 0x10D */ u8          field_10d;
    /* 0x10E */ u16         field_10e;
    /* 0x110 */ byte        unknown_110[0x2];
    /* 0x112 */ s16         field_112;
    /* 0x114 */ s32         field_114;
    /* 0x118 */ s32         field_118; // frame/mode word (stage flow)
    /* 0x11C */ byte        unknown_11c[0x1];
    /* 0x11D */ u8          field_11d;
    /* 0x11E */ u8          field_11e;
    /* 0x11F */ byte        unknown_11f[0x1];
    /* 0x120 */ s16         field_120;
    /* 0x122 */ u8          field_122;
    /* 0x123 */ s8          field_123; // Task_Kill overlay-teardown gate
    /* 0x124 */ u16         field_124; // region (1 → PAL / CdStream 0x14 sectors)
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
    /* 0x00 */ s32     field_0;
    /* 0x04 */ u_long* field_4;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* field_10;
} GpuOtBuf;
STATIC_ASSERT_SIZEOF(GpuOtBuf, 0x14);

/// Double-buffered ordering-table descriptor (same layout as PsyQ GsOT).
/// Used by Gpu_OrderingTables and passed to GsClearOt.
typedef struct _GameOt {
    /* 0x00 */ u_long  length;
    /* 0x04 */ u_long* org;
    /* 0x08 */ u_long  offset;
    /* 0x0C */ u_long  point;
    /* 0x10 */ u_long* tag;
} GameOt;
STATIC_ASSERT_SIZEOF(GameOt, 0x14);

/// PsyQ GsClearOt, declared with GameOt* so callers need not include libgs.h.
void GsClearOt(unsigned short offset, unsigned short point, GameOt* otp);

// =============================================================================
// Globals
// =============================================================================

/// Dual-buffer display / system state (bss @ 0x80070F68).
extern DisplayState Display_State;

// --- APIs (from unknown_syms) ---
void  Display_SetMode(s32 arg0);
void  Display_SetAutoClear(s32 arg0, s32 arg1, s32 arg2);
void  Display_ClampField126(s8 arg0);
void  Gpu_InitOtSmall(void);
Task* Display_SpawnFromMode(void);
void  Display_FlipOtAndDispatch(s32 arg0);
void  Display_InvertFramebufferGray(void);
s32   Display_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void  Display_SetFadeMax(u8 arg0);
s32   Display_InitModeObj(TaskDesc* arg0, s32 arg1, s32 arg2, s32 arg3);
void  Gpu_ResetGraphAndOt(void);

#endif // DISPLAY_H
