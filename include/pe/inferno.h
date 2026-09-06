#ifndef PE_INFERNO_H
#define PE_INFERNO_H

#include "common.h"

/// The `SndEvt_EnqueueType6` id the inferno cast plays, indexed by
/// `Gp_StateC08.field_0 % 10 - 1` so the roar scales with the combo counter.
/// The same index also picks the state `func_inferno_8012EF88` advances to,
/// which is why the three ids and the three state chains run in step.
extern s32 D_inferno_801304F0[];

/// 0xC jitter block `func_inferno_8012F530` hangs off `Task::idMap` via
/// `Mem_Calloc(0xC)`. It is two parallel 6-byte columns, one per ring drawn
/// by the pair of fan routines: `field_0[0][i]` seeds the inner ring's
/// texture frame and `field_0[1][i]` the outer one. State 0 fills both with
/// one walker, `p = &field_0[0][i]`, writing `p[0]` and `p[6]`.
typedef struct InfernoIdMap {
    /* 0x0 */ u8 field_0[2][6];
} InfernoIdMap;
STATIC_ASSERT_SIZEOF(InfernoIdMap, 0xC);

/// One ring's geometry in `D_inferno_801304E4`, indexed by the `kind` the
/// caster passes to the fan routines (0 = inner ring, 1 = outer ring).
/// `field_0` is added to `GpEffWork::field_26` to give the ring's radius in
/// the ground plane, `field_2` is how far the inner rim is lifted along local
/// Y, and `field_4` plus `GpEffWork::field_2A` widens the outer rim.
typedef struct InfernoFanParam {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} InfernoFanParam;
STATIC_ASSERT_SIZEOF(InfernoFanParam, 0x6);

extern InfernoFanParam D_inferno_801304E4[];

/// 0x70-byte scratch `func_inferno_8012FF34` carves off `G_SCRATCH_HEAD` for
/// one ring. `inner` and `outer` are the six rim points of each edge of the
/// ring, built by `rsin` / `rcos`, rotated by the effect coordinate's `workm`
/// and offset by its translation. `sxy0` is where `inner[i]` projects to
/// through a single `RTPS`; `sxy1`..`sxy3` are the other three corners of the
/// segment quad through one `RTPT`. The four hold packed `SXY2` words, so the
/// screen X of each is the low half and the screen Y the arithmetic shift.
typedef struct InfernoFanScratch {
    /* 0x00 */ SVECTOR inner[6];
    /* 0x30 */ SVECTOR outer[6];
    /* 0x60 */ s32     sxy0;
    /* 0x64 */ s32     sxy1;
    /* 0x68 */ s32     sxy2;
    /* 0x6C */ s32     sxy3;
} InfernoFanScratch;
STATIC_ASSERT_SIZEOF(InfernoFanScratch, 0x70);

#endif /* PE_INFERNO_H */
