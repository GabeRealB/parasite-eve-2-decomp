#ifndef PE_ANTIBODY_H
#define PE_ANTIBODY_H

#include "common.h"

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

#endif /* PE_ANTIBODY_H */
