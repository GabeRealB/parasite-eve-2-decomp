#ifndef STAGE_H
#define STAGE_H

#include "common.h"

#include "main/task.h"

// =============================================================================
// Types — stage / area flow context (Stage_Ctx)
// =============================================================================

/// Stage / flow context (Stage_Ctx → bss D_8007A320, size 0x38).
typedef struct _StageCtx {
    /* 0x00 */ TaskDesc* field_0; // task desc table for spawn
    /* 0x04 */ s32       field_4; // spawn arg
    /* 0x08 */ s32       field_8; // spawn arg
    /* 0x0C */ u32       field_C;
    /* 0x10 */ byte      unknown_10;
    /* 0x11 */ u8        field_11;
    /* 0x12 */ u8        field_12; // flow gate
    /* 0x13 */ u8        field_13;
    /* 0x14 */ u8        field_14;
    /* 0x15 */ u8        field_15;
    /* 0x16 */ byte      unknown_16;
    /* 0x17 */ u8        field_17; // flow gate
    /* 0x18 */ u8        field_18;
    /* 0x19 */ u8        field_19; // flag bits (bit0/1)
    /* 0x1A */ u8        field_1a;
    /* 0x1B */ byte      unknown_1b;
    /* 0x1C */ u32       field_1c;    // flag word
    /* 0x20 */ s32       field_20;
    /* 0x24 */ s32       field_24;    // last Display_State.field_118
    /* 0x28 */ s32       field_28;    // step counter
    /* 0x2C */ u8        field_2C[8]; // CDF load param block
    /* 0x34 */ u8        field_34[4]; // CDF load param block
} StageCtx;
STATIC_ASSERT_SIZEOF(StageCtx, 0x38);

// =============================================================================
// Globals
// =============================================================================

/// Active stage/flow context pointer.
extern StageCtx* Stage_Ctx;

#endif // STAGE_H
