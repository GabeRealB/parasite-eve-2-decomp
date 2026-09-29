#ifndef PE_METABOLISM_H
#define PE_METABOLISM_H

#include "main/task_types.h"

#include "common.h"

/// One 8-byte row of `D_metabolism_8012FB54`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the cast scales with the combo
/// counter). `field_0` is how many fan wedges the cast lays out - the number
/// of `D_metabolism_8012FB78` angles it seeds and then draws through
/// `func_metabolism_8012F840`. `field_2` is the brightness cap state 1 grows
/// `GpEffWork.scale` toward in steps of 0x10, `field_4` the per-frame
/// radius step added to `GpEffWork.angle`, and `field_6` both the
/// `Gp_SpawnEff` spawn arg for the three orbiting sparks and the radius at
/// which state 1 hands over to state 2.
typedef struct MetabolismStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ s16 field_6;
} MetabolismStep;
STATIC_ASSERT_SIZEOF(MetabolismStep, 8);

void func_metabolism_8012F5A0(Task* arg0);

void func_metabolism_8012EF34(Task* arg0);

#endif /* PE_METABOLISM_H */
