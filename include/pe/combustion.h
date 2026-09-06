#ifndef PE_COMBUSTION_H
#define PE_COMBUSTION_H

#include "common.h"

/// One 8-byte row of `D_combustion_80130980`, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the burn scales with the combo counter).
/// `field_0` / `field_2` are the per-frame Y / Z drift added to the flame
/// overlay `GpEffWork.field_12` / `field_14`. `field_4` is the last
/// `GpEffWork.field_22` tick that still spawns flames, and `field_6` is the
/// last tick of the burn as a whole; it is also the pad-rumble duration
/// `Gp_SpawnPadLerp` is given when the effect starts.
typedef struct CombustionStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} CombustionStep;
STATIC_ASSERT_SIZEOF(CombustionStep, 0x8);

/// Three combustion intensities, weakest first.
extern CombustionStep D_combustion_80130980[];
/// The `SndEvt_EnqueueType6` id for each `D_combustion_80130980` row.
extern s32 D_combustion_80130998[];
/// The effect coordinate's world Y at ignition, saved by
/// `func_combustion_8012EF34` before it re-bases the coordinate on the player.
extern s32 D_combustion_801309A4;

#endif /* PE_COMBUSTION_H */
