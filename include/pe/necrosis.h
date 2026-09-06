#ifndef PE_NECROSIS_H
#define PE_NECROSIS_H

#include "common.h"

#include "gameplay/3A34.h"

/// One 4-byte row of `D_necrosis_801306BC`, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the `Gp_SpawnEff` draw
/// parameter (plus `field_22 * 0x60` each frame) and is copied into the
/// first `GpObj.field_1C`. `field_2` is the last `GpEffWork.field_22` tick
/// of the spawn loop; state 2 waits an extra 0x10 ticks past it. `field_2 +
/// 0xC` is also the pad-rumble duration at ignition.
typedef struct NecrosisStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} NecrosisStep;
STATIC_ASSERT_SIZEOF(NecrosisStep, 4);

/// Collision pair allocated by `func_necrosis_8012EF34` (`Mem_Calloc(0x58)`)
/// and stored in `Task::idMap`. `obj` is linked on list 1, `obj2` on list 7;
/// both point `field_C` at the one-element `rec` table (terminator `field_0
/// = 2`).
typedef struct NecrosisWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpObj   obj2;
    /* 0x40 */ GpRec18 rec;
} NecrosisWork;
STATIC_ASSERT_SIZEOF(NecrosisWork, 0x58);

/// Three necrosis intensities, weakest first.
extern NecrosisStep D_necrosis_801306BC[];
/// The `SndEvt_EnqueueType6` id for each `D_necrosis_801306BC` row.
extern s32 D_necrosis_801306C8[];

#endif /* PE_NECROSIS_H */
