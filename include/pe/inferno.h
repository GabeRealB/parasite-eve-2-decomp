#ifndef PE_INFERNO_H
#define PE_INFERNO_H

#include "common.h"

/// The `SndEvt_EnqueueType6` id the inferno cast plays, indexed by
/// `Gp_StateC08.field_0 % 10 - 1` so the roar scales with the combo counter.
/// The same index also picks the state `func_inferno_8012EF88` advances to,
/// which is why the three ids and the three state chains run in step.
extern s32 D_inferno_801304F0[];

/// 0xC jitter block `func_inferno_8012F530` hangs off `Task::idMap` via
/// `Mem_Calloc(0xC)`. State 0 fills two parallel 6-byte LCG columns; the
/// walker is `&field_0[i]`, with `p[6]` landing on `field_6[i]`.
typedef struct InfernoIdMap {
    /* 0x0 */ s8 field_0[6];
    /* 0x6 */ s8 field_6[6];
} InfernoIdMap;
STATIC_ASSERT_SIZEOF(InfernoIdMap, 0xC);

#endif /* PE_INFERNO_H */
