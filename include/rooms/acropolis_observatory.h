#ifndef ROOMS_ACROPOLIS_OBSERVATORY_H
#define ROOMS_ACROPOLIS_OBSERVATORY_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgte.h>

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

/// 0x14 work block the observatory's streamed-scene task
/// (`func_acropolis_observatory_8017DD3C`) keeps at `Task::idMap`
/// (`Mem_Calloc(0x14, 0)` in its state 0).
///
/// `mtx` is `Wip_SysConfig.field_4`, the player actor's coordinate matrix; the
/// task walks its translation along `D_acropolis_observatory_8017F16C` once per
/// frame while the stream runs. `target` is the slot-3 task every message the
/// scene sends is addressed to, captured once from `Game_GetPtrSlot(3)`.
/// `child` is the prompt task spawned from `D_acropolis_observatory_8017E7DC`
/// entry 2 and polled with `Task_PollKill`; `spawned` records that it exists,
/// since the calloc leaves it at 0. `script` is the scene's script task, which
/// the observatory task reparents itself under.
typedef struct AobStreamWork {
    /* 0x00 */ MATRIX* mtx;
    /* 0x04 */ Task*   target;
    /* 0x08 */ Task*   child;
    /* 0x0C */ Task*   script;
    /* 0x10 */ u16     spawned;
    /* 0x12 */ u16     pad_12;
} AobStreamWork;
STATIC_ASSERT_SIZEOF(AobStreamWork, 0x14);

/// 0x18 block the observatory's lens-flare task takes off `G_SCRATCH_HEAD` for
/// one frame. `pos` is the model's world position (`GsCOORDINATE2::workm`
/// translation) loaded into the GTE as V0; `sx`/`sy`, `otz` and `flag` are the
/// `rtps` results read back with `gte_stsxy`, `gte_stszotz` and `gte_stflg`.
/// `otz` doubles as the sprite's depth after being pulled 0x40 towards the
/// camera and clamped to 0x10, and `half` is the flare's half-extent in pixels,
/// `0x5D00 / otz`, so the quad shrinks with distance.
typedef struct AobFlareScratch {
    /* 0x00 */ SVECTOR pos;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     half;
    /* 0x14 */ u16     sx;
    /* 0x16 */ u16     sy;
} AobFlareScratch;
STATIC_ASSERT_SIZEOF(AobFlareScratch, 0x18);

#endif
