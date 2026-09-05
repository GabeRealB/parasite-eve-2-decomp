#ifndef ROOMS_ACROPOLIS_FORKED_ROAD_H
#define ROOMS_ACROPOLIS_FORKED_ROAD_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgte.h>

/// 0x14 work block the forked road's streamed-scene task
/// (`func_acropolis_forked_road_8017DA24`) keeps at `Task::idMap`
/// (`Mem_Calloc(0x14, 0)` in its state 0).
///
/// `mtx` is `D_80073B8C`, the camera-target matrix the scene walks along
/// `D_acropolis_forked_road_80180F80` once per streamed frame. `target` is the
/// slot-3 task every placement message is addressed to, captured once from
/// `Game_GetPtrSlot(3)`. `script` is the scene's script task, which the room
/// task reparents itself under. The return ride
/// (`func_acropolis_forked_road_8017DD60`) also uses `skipper`, the task it
/// spawns when the pad asks to skip the scene, and `skipping`, the flag that
/// says that task is live.
typedef struct AfrStreamWork {
    /* 0x00 */ MATRIX* mtx;
    /* 0x04 */ Task*   target;
    /* 0x08 */ Task*   skipper;
    /* 0x0C */ Task*   script;
    /* 0x10 */ u16     skipping;
    /* 0x12 */ byte    pad_12[0x2];
} AfrStreamWork;
STATIC_ASSERT_SIZEOF(AfrStreamWork, 0x14);

#endif
