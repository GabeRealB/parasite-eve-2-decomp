#ifndef PE_APOBIOSIS_H
#define PE_APOBIOSIS_H

#include "common.h"
#include "main/task.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x1C-byte scratch block `func_apobiosis_8012F9D0` takes from
/// `G_SCRATCH_HEAD`. `vec` is the effect coordinate's `workm.t[]` truncated to
/// s16 and projected through `GsWSMATRIX` with one `RTPS`: `flag` is the
/// `gte_stflg` of that projection (a negative value drops the ring), `otz` its
/// `gte_stszotz` and `sx` / `sy` its `gte_stsxy`. `inner` and `outer` are the
/// two on-screen radii the ring is swept between. Same layout as the gameplay
/// `GpArcScratch`.
typedef struct ApobiosisRingScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     inner;
    /* 0x14 */ s32     outer;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} ApobiosisRingScratch;
STATIC_ASSERT_SIZEOF(ApobiosisRingScratch, 0x1C);

/// 0x28-byte scratch block `func_apobiosis_80130630` takes from
/// `G_SCRATCH_HEAD` to draw one burst shard. `v0` is the effect coordinate's
/// world position and `v1` that position plus the three-halfword offset
/// `arg1`; both are projected through `GsWSMATRIX` with one `RTPS` each,
/// giving `sx0`/`sy0` and `sx1`/`sy1`. `flag` is the `gte_stflg` of whichever
/// projection ran last (a negative value drops the quad) and `otz` is the
/// first projection's `gte_stszotz`, incremented by 1 before it becomes both
/// the radius divisor and the OT bucket. `dx` / `dy` hold the current
/// `(arg3 * 23 / otz) * rsin|rcos(angle) >> 12` half-extents; only their low
/// halves are read back.
typedef struct ApobiosisShardScratch {
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
} ApobiosisShardScratch;
STATIC_ASSERT_SIZEOF(ApobiosisShardScratch, 0x28);

/// One 8-byte row of `D_apobiosis_80130B5C`, indexed by the effect's
/// `GpEffWork.field_20` / `field_2A` (`Gp_StateC08.field_0 % 10 - 1`, so the
/// burst scales with the combo counter). `field_0` is half the number of ring
/// points the cast lays out, `field_2` the ring radius it draws them at and
/// `field_4` the per-frame growth added to the cast's `GpEffWork.field_24`.
/// `field_6` is the shard radius `func_apobiosis_8012FE10` hands to
/// `func_apobiosis_8013017C` / `func_apobiosis_80130630` - doubled while the
/// shard is still parented to the cast (state 1), plain once it flies free
/// (state 2).
typedef struct ApobiosisStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ u16 field_6;
} ApobiosisStep;
STATIC_ASSERT_SIZEOF(ApobiosisStep, 0x8);

/// Three apobiosis intensities, weakest first.
extern ApobiosisStep D_apobiosis_80130B5C[];

/// The `SndEvt_EnqueueType6` id the cast plays, one per `D_apobiosis_80130B5C`
/// row, so the boom scales with the combo counter like the burst does.
extern s32 D_apobiosis_80130B74[];

/// Ring azimuths, two rows of up to eight. `func_apobiosis_8012EF4C` lays out
/// `ApobiosisStep::field_0 * 2` of them at `(i << 10) + rand()` in state 0 and
/// then jitters each by +-0x80 a frame; the first row is the shard's own angle
/// and the row `ApobiosisStep::field_4` entries later is its elevation.
extern s16 D_apobiosis_80130B80[];

/// The running cast task, cached by `func_apobiosis_8012EF4C` so each shard
/// can reparent itself onto the cast when it starts.
extern Task* D_apobiosis_80130BA0;

#endif /* PE_APOBIOSIS_H */
