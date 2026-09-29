#ifndef PE_COMBUSTION_H
#define PE_COMBUSTION_H

#include "main/task_types.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// One 8-byte row of `D_combustion_80130980`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the burn scales with the combo counter).
/// `field_0` / `field_2` are the per-frame Y / Z drift added to the flame
/// overlay `GpEffWork.move`. `field_4` is the last
/// `GpEffWork.age` tick that still spawns flames, and `field_6` is the
/// last tick of the burn as a whole; it is also the pad-rumble duration
/// `Gp_SpawnPadLerp` is given when the effect starts.
typedef struct CombustionStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} CombustionStep;
STATIC_ASSERT_SIZEOF(CombustionStep, 0x8);

void func_combustion_8012EF34(Task* arg0);

void func_combustion_8012F2BC(Task* arg0);

void func_combustion_8012F888(Task* arg0);

void func_combustion_801308E0(Task* arg0);

#endif /* PE_COMBUSTION_H */
