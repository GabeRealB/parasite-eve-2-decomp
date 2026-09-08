#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80131E68);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801323D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801325D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132798);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132CE8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132DF8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132F30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133424);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801335EC);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_8013391C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133C2C);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133D74);

void func_actor_104600_801342C4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_104600_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80134320);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80134438);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E5C);
