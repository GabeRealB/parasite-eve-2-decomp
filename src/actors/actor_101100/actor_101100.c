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

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", D_actor_101100_80131EEC);

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", ActorsShared80138404Table);
