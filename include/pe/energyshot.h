#ifndef PE_ENERGYSHOT_H
#define PE_ENERGYSHOT_H

#include "main/task_types.h"

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

void func_energyshot_8012FFB8(Task* arg0);

void func_energyshot_8012EF34(Task* arg0);

#endif /* PE_ENERGYSHOT_H */
