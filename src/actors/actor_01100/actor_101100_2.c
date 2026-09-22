#include "common.h"

#include "actors/actor_101100.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern Actor101100StateFuncTable3 Actor01100_D00004;

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn04410);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn048C8);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn04DB4);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn0516C);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn05678);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn05CFC);

INCLUDE_ASM("actors/nonmatchings/actor_01100/actor_101100_2", Actor01100_Fn05E68);
