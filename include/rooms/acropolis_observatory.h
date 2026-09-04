#ifndef ROOMS_ACROPOLIS_OBSERVATORY_H
#define ROOMS_ACROPOLIS_OBSERVATORY_H

#include "common.h"

#include "main/task.h"

/// 8-byte work block the observatory's scene task keeps at `Task::idMap`
/// (`Mem_Calloc(8, 0)` in state 0 of `func_acropolis_observatory_8017E19C`).
///
/// `target` is the slot-3 task every message the scene sends is addressed to,
/// captured once from `Game_GetPtrSlot(3)`. `step` selects the follow-up
/// record in `D_acropolis_observatory_8017FE68`: the calloc leaves it at 0,
/// which is the `-1` entry that means "nothing to send".
typedef struct AobSceneWork {
    /* 0x0 */ Task* target;
    /* 0x4 */ u16   field_4;
    /* 0x6 */ u16   step;
} AobSceneWork;
STATIC_ASSERT_SIZEOF(AobSceneWork, 8);

#endif
