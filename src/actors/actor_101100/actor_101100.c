#include "common.h"

#include "actors/actor_101100.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 D_actor_101100_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80131F08);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132250);

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", D_actor_101100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", D_actor_101100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_8013279C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132B10);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132D78);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80133BB8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80134780);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135404);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135560);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801356BC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801357F0);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801359CC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135FDC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137B1C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137C88);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137FB8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", ActorsShared8013845cSub0);

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
    work    = task->idMap;
    scratch = (u8*)(SCRATCH_SP -= 0x68);

    scratch[0x64] = 0;
    sp.funcs[task->state](enemy, task, work, scratch);
    SCRATCH_SP += 0x68;
}

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", ActorsShared80138404Table);
