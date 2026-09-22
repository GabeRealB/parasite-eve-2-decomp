#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202400_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_80149E84);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014A3DC);
INCLUDE_RODATA("actors/nonmatchings/actor_202400/actor_202400", D_actor_202400_80149E24);
