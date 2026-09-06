#ifndef PE_APOBIOSIS_H
#define PE_APOBIOSIS_H

#include "common.h"
#include "main/task.h"

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
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} ApobiosisStep;
STATIC_ASSERT_SIZEOF(ApobiosisStep, 0x8);

/// Three apobiosis intensities, weakest first.
extern ApobiosisStep D_apobiosis_80130B5C[];

/// The running cast task, cached by `func_apobiosis_8012EF4C` so each shard
/// can reparent itself onto the cast when it starts.
extern Task* D_apobiosis_80130BA0;

#endif /* PE_APOBIOSIS_H */
