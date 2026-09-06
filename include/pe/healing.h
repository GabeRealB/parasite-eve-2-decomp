#ifndef PE_HEALING_H
#define PE_HEALING_H

#include "common.h"

/// One 8-byte row of `D_healing_8012FC1C`, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_2` is the brightness cap state 1
/// grows `GpEffWork.field_24` toward (and the starting radius in
/// `func_healing_8012F5E4`). `field_4` is the per-frame radius step and the
/// yaw passed to `Gfx_RotMatrixY` as `-(field_4 * 2)`. `field_6` is both the
/// `Gp_SpawnEff` spawn arg and the radius at which state 1 advances to 2.
typedef struct HealingScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} HealingScale;
STATIC_ASSERT_SIZEOF(HealingScale, 8);

extern HealingScale D_healing_8012FC1C[];
/// The `SndEvt_EnqueueType6` id for each `D_healing_8012FC1C` row.
extern s32 D_healing_8012FC34[];

#endif /* PE_HEALING_H */
