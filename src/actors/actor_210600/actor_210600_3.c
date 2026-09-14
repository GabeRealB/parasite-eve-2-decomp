#include "common.h"

#include "actors/actor_210600.h"

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014B8C8);

/// State dispatcher: copies the overlay's 3-entry state table onto the stack
/// and calls the entry `Task::state` selects, handing it the task's 0x20 spawn
/// argument and the task itself.
void func_actor_210600_8014BA3C(Task* arg0)
{
    Actor210600DispatchCtx sp;

    sp.table = D_actor_210600_80149E24;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014BA98);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014BDA4);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C0EC);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C638);

INCLUDE_ASM("actors/nonmatchings/actor_210600/actor_210600_3", func_actor_210600_8014C7DC);
