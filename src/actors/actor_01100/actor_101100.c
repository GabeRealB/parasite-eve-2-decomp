#include "common.h"

#include "actors/actor_101100.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 Actor01100_D00004;

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn000E8);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn00430);
INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", Actor01100_D00004);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn0097C);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn00CF0);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn00F58);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn01B90);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn01D98);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn02960);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn035E4);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn03740);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100", Actor01100_Fn0389C);

INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", Actor01100_D000CC);

INCLUDE_RODATA("actors/nonmatchings/actor_01100/actor_101100", ActorsShared80138404Table);
