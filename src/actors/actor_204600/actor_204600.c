#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_204600_80149E24;

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_80149E68);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E20);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E24);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E30);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A1F4);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A3D0);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A5D0);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014A798);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014ACE8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014ADF8);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014AF30);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B424);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B5EC);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014B91C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014BC2C);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014BD74);

void func_actor_204600_8014C2C4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_204600_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C320);

INCLUDE_ASM("actors/nonmatchings/actor_204600/actor_204600", func_actor_204600_8014C438);

INCLUDE_RODATA("actors/nonmatchings/actor_204600/actor_204600", D_actor_204600_80149E5C);
