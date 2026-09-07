#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202400_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_80149E84);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014A3DC);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014A77C);

INCLUDE_RODATA("actors/nonmatchings/actor_202400/actor_202400", D_actor_202400_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_202400/actor_202400", D_actor_202400_80149E24);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014AA28);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014B240);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014B3B0);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014B830);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014B9B0);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014BD94);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014BEAC);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014C084);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014C1D4);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014C318);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014C5B0);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014C910);

INCLUDE_ASM("actors/nonmatchings/actor_202400/actor_202400", func_actor_202400_8014CAC4);

void func_actor_202400_8014CBD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_202400_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_202400/actor_202400", D_actor_202400_80149E5C);
