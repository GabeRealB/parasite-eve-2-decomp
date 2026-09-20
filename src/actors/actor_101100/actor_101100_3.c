#include "common.h"

#include "actors/actor_101100.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 D_actor_101100_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", ActorsShared8013845cSub0);

/// Runs the actor's current state handler, copying the table onto the stack
/// before the call. The handler is handed a 0x68-byte scratchpad buffer,
/// borrowed from the scratchpad stack for the duration of the call, with its
/// last byte cleared.
void func_actor_101100_80138374(Task* task)
{
    Actor101100StateFuncTable3 sp;
    GpEnemy*                   enemy;
    void*                      work;
    u8*                        scratch;

    sp      = D_actor_101100_80131E24;
    enemy   = task->spawnArg2;
    work    = task->work;
    scratch = (u8*)(SCRATCH_SP -= 0x68);

    scratch[0x64] = 0;
    sp.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_SP += 0x68;
}
