#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_107000_80131E5C;

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80136938);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80136C80);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80136E88);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_80137220);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_8013777C);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_6", func_actor_107000_801378D8);

void func_actor_107000_80137C8C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = D_actor_107000_80131E5C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

// func_actor_107000_80137CF4 is shared with actor_207000; see
// src/actors/lib/actors_shared_80137cf4.c.
