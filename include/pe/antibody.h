#ifndef PE_ANTIBODY_H
#define PE_ANTIBODY_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// One 14-byte row of `D_antibody_80130BD4`, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the effect scales with the combo
/// counter). `field_6` is the draw parameter `func_antibody_8012F734` seeds
/// `GpEffWork.field_24` with, and `field_8` is the base it is re-rolled from
/// on later frames (doubled in state 3). The remaining fields belong to the
/// draw helpers.
typedef struct AntibodyStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ u16 field_8;
    /* 0xA */ u16 field_A;
    /* 0xC */ u16 field_C;
} AntibodyStep;
STATIC_ASSERT_SIZEOF(AntibodyStep, 0xE);

/// Three antibody intensities, weakest first.
extern AntibodyStep D_antibody_80130BD4[];

/// 0x1C-byte scratch block `func_antibody_8012FBB0` takes from
/// `G_SCRATCH_HEAD` to draw one antibody mote. `vec` is the effect
/// coordinate's world position, projected through `GsWSMATRIX` with one
/// `RTPS` into `sxy0`. `flag` is `gte_stflg` (a negative value drops the
/// quad) and `otz` is `gte_stszotz`, incremented by 1 so it is both the
/// radius divisor and the OT bucket. `dx` / `dy` hold the current
/// `(arg2 * 39 / otz) * rsin|rcos(angle) >> 12` half-extents, taken at the
/// spin angle and again at `+ 0x400`; only their low halves are read back.
typedef struct AntibodyMoteScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ DVECTOR sxy0;
} AntibodyMoteScratch;
STATIC_ASSERT_SIZEOF(AntibodyMoteScratch, 0x1C);

/// 0x28-byte scratch block `func_antibody_80130428` takes from
/// `G_SCRATCH_HEAD` to draw one antibody arc. `v0` is the effect
/// coordinate's world position and `v1` the player's second part coordinate;
/// both are projected through `GsWSMATRIX` with one `RTPS` each, giving
/// `sx0`/`sy0` and `sx1`/`sy1`. `flag` is the `gte_stflg` of whichever
/// projection ran last (a negative value drops the quad) and `otz` is the
/// first projection's `gte_stszotz`, incremented by 1 before it becomes both
/// the radius divisor and the OT bucket. `dx` / `dy` hold the current
/// `(arg2 * 23 / otz) * rsin|rcos(angle) >> 12` half-extents; only their low
/// halves are read back.
typedef struct AntibodyArcScratch {
    /* 0x00 */ SVECTOR v0;
    /* 0x08 */ SVECTOR v1;
    /* 0x10 */ s32     otz;
    /* 0x14 */ s32     flag;
    /* 0x18 */ s32     dx;
    /* 0x1C */ s32     dy;
    /* 0x20 */ s16     sx0;
    /* 0x22 */ s16     sy0;
    /* 0x24 */ s16     sx1;
    /* 0x26 */ s16     sy1;
} AntibodyArcScratch;
STATIC_ASSERT_SIZEOF(AntibodyArcScratch, 0x28);

#endif /* PE_ANTIBODY_H */
