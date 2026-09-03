#ifndef ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x54 work block of the helipad enemy task, hung off the `Task::idMap`
/// slot -- it is the `Mem_Calloc(0x54)` block that
/// `func_acropolis_helicopter_landing_pad_8017D658` allocates, not a
/// `TaskIdMap`. Reach it with `(AhlpEnemyWork*)task->idMap`.
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_acropolis_helicopter_landing_pad_8017D7B0` points the `TmdObject`'s
/// `field_1C` / `field_20` at them and fills them from the three
/// `D_acropolis_helicopter_landing_pad_80182340` lights.
typedef struct AhlpEnemyWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ s32    field_C;
    /* 0x10 */ MATRIX lightMtx;
    /* 0x30 */ MATRIX colorMtx;
    /* 0x50 */ s16    field_50;
    /* 0x52 */ byte   pad_52[0x2];
} AhlpEnemyWork;
STATIC_ASSERT_SIZEOF(AhlpEnemyWork, 0x54);

/// Payload of room msg `0x7D3`, handled by
/// `func_acropolis_helicopter_landing_pad_8017D824`. `phase` selects which
/// of the two `RoomPlacement`s the model is moved to (0 / 2 first, 1 second).
typedef struct AhlpMsg7D3 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 phase;
} AhlpMsg7D3;
STATIC_ASSERT_SIZEOF(AhlpMsg7D3, 0x8);

/// Per-light 0x60 work block inside an `AhlpLight`. Filled by
/// `func_acropolis_helicopter_landing_pad_8017F010` every frame the light is
/// drawn: `x` / `y` / `z` copy the light's world `SVECTOR`, `field_50` is
/// `level * 16`, and `field_58` / `field_5C` are the constants 0x640 / 0x3200.
typedef struct AhlpLightWork {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte pad_4[0x14];
    /* 0x18 */ s32  x;
    /* 0x1C */ s32  y;
    /* 0x20 */ s32  z;
    /* 0x24 */ byte pad_24[0x2C];
    /* 0x50 */ s16  field_50;
    /* 0x52 */ s16  field_52;
    /* 0x54 */ s16  field_54;
    /* 0x56 */ byte pad_56[0x2];
    /* 0x58 */ s32  field_58;
    /* 0x5C */ s32  field_5C;
} AhlpLightWork;
STATIC_ASSERT_SIZEOF(AhlpLightWork, 0x60);

/// One of the two helipad floodlight records at `D_80115188` (main BSS).
/// Light `index` uses record `index & 1`. `state` is 2 while the light is
/// being drawn and reset to 0 once `Gp_State1C::field_4` reaches 4.
typedef struct AhlpLight {
    /* 0x00 */ s32           state;
    /* 0x04 */ AhlpLightWork work;
} AhlpLight;
STATIC_ASSERT_SIZEOF(AhlpLight, 0x64);

/// 0x14 scratch block `func_acropolis_helicopter_landing_pad_8017F010` takes
/// from `G_SCRATCH_HEAD` for one light. `otz` is `SZ3 >> 2` of the `RTPS`,
/// `flag` the GTE flag word (bit 31 rejects the light), `outer` / `inner` the
/// two glow radii `0xC000 / otz` and `0x1800 / otz`, and `sx` / `sy` the
/// projected centre.
typedef struct AhlpLightScratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 flag;
    /* 0x08 */ s32 outer;
    /* 0x0C */ s32 inner;
    /* 0x10 */ u16 sx;
    /* 0x12 */ u16 sy;
} AhlpLightScratch;
STATIC_ASSERT_SIZEOF(AhlpLightScratch, 0x14);

/// 0x20 scratch block `func_acropolis_helicopter_landing_pad_80180A64` takes
/// from `G_SCRATCH_HEAD` for one spark line. `a` / `b` are the two random
/// endpoints, rotated by the coord's `workm` and offset by its translation;
/// `otz` is `SZ3 >> 2` of the second `RTPS`, `flag` the GTE flag word (bit 31
/// rejects the line), and `x0..y1` the two projected screen points.
typedef struct AhlpSparkScratch {
    /* 0x00 */ SVECTOR a;
    /* 0x08 */ SVECTOR b;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ u16     x0;
    /* 0x1A */ u16     y0;
    /* 0x1C */ u16     x1;
    /* 0x1E */ u16     y1;
} AhlpSparkScratch;
STATIC_ASSERT_SIZEOF(AhlpSparkScratch, 0x20);

#endif
