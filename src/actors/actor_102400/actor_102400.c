#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80131E84);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801323DC);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_8013277C);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80132A28);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E5C);
