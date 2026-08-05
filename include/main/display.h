#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

// =============================================================================
// Types — dual DISPENV/DRAWENV + system flags
// =============================================================================

typedef struct _DisplayState {
    s32     field_0;
    s32     field_4;
    s32     field_8;
    s32     field_c;
    s32     field_10;
    s32     field_14;
    byte    unknown_18[0x5];
    s8      field_1d;
    s8      field_1e;
    u8      field_1f;
    DISPENV field_20[2];
    DRAWENV field_48[2];
    u8      field_100;
    u8      field_101;
    byte    unknown_102[0x1];
    u8      field_103;
    u16     field_104;
    u16     field_106;
    // Written by main, read by the VSync callback (func_80027498).
    volatile u8 field_108;
    byte        unknown_109[0x1];
    u8          field_10a;
    u8          field_10b;
    byte        unknown_10c[0x1];
    u8          field_10d;
    s16         field_10e;
    byte        unknown_110[0x2];
    s16         field_112;
    s32         field_114;
    s32         field_118;
    byte        unknown_11c[0x1];
    u8          field_11d;
    u8          field_11e;
    byte        unknown_11f[0x1];
    s16         field_120;
    u8          field_122;
    byte        unknown_123[0x1];
    u16         field_124;
    s8          field_126;
    byte        unknown_127[0x3];
    u16         field_12a;
    u16         field_12c;
    u8          field_12e;
    byte        unknown_12f[0x1];
    u8          field_130;
    byte        unknown_131[0x7];
} DisplayState;
STATIC_ASSERT_SIZEOF(DisplayState, 0x138);

// =============================================================================
// Globals
// =============================================================================

/// Dual-buffer display / system state (bss @ 0x80070F68).
extern DisplayState Display_State;

#endif // DISPLAY_H
