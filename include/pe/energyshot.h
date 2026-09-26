#ifndef PE_ENERGYSHOT_H
#define PE_ENERGYSHOT_H

#include "common.h"

/// One 8-byte row of `D_energyshot_801300E4`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the wedge count. `field_2` is
/// the brightness cap state 1 grows `GpEffWork.scale` toward (and the ring
/// radius in state 2). `field_4` is the per-frame brightness step. `field_6` is
/// the beam depth / spawn height.
typedef struct EnergyShotScale {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 field_6;
} EnergyShotScale;
STATIC_ASSERT_SIZEOF(EnergyShotScale, 8);

/// The `SndEvt_EnqueueType6` id for each `D_energyshot_801300E4` row.
extern s32 D_energyshot_801300FC[];

/// Sixteen per-vertex texture-frame offsets, refilled once per cast by
/// `func_energyshot_8012EF34` and consumed by the GTE pass in
/// `func_energyshot_8012FA50`, where each is added to `gDisplayState.animFrame`
/// and reduced mod 6 to pick one of the six 0x28-wide frames of the beam
/// texture.
extern s16 D_energyshot_80130108[];

#endif /* PE_ENERGYSHOT_H */
