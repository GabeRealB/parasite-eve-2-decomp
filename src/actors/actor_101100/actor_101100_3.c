#include "common.h"

#include "actors/actor_101100.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 D_actor_101100_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80137B1C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100_3", func_actor_101100_80137C88);
