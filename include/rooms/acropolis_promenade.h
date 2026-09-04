#ifndef ROOMS_ACROPOLIS_PROMENADE_H
#define ROOMS_ACROPOLIS_PROMENADE_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// 0x14 work block the promenade's streamed-scene task
/// (`func_acropolis_promenade_8017DB9C`) keeps at `Task::idMap`
/// (`Mem_Calloc(0x14, 0)` in its state 0). Laid out like the observatory's
/// `AobStreamWork`, which drives the same kind of scene.
///
/// `mtx` is `Wip_SysConfig.field_4`, the player actor's coordinate matrix; the
/// task walks its translation along `D_acropolis_promenade_80181184` once per
/// frame while the stream runs. `target` is the slot-3 task every message the
/// scene sends is addressed to, captured once from `Game_GetPtrSlot(3)`.
/// `child` is the prompt task spawned from `D_acropolis_promenade_80181148`
/// entry 3 and polled with `Task_PollKill`; `spawned` records that it exists,
/// since the calloc leaves it at 0. `script` is the scene's script task, which
/// the promenade task reparents itself under.
typedef struct ApmStreamWork {
    /* 0x00 */ MATRIX* mtx;
    /* 0x04 */ Task*   target;
    /* 0x08 */ Task*   child;
    /* 0x0C */ Task*   script;
    /* 0x10 */ u16     spawned;
    /* 0x12 */ u16     pad_12;
} ApmStreamWork;
STATIC_ASSERT_SIZEOF(ApmStreamWork, 0x14);

#endif // ROOMS_ACROPOLIS_PROMENADE_H
