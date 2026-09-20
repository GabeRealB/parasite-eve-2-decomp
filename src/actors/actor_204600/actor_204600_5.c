#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_204600_80149E5C;

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600_5", func_actor_204600_8014CB88);
