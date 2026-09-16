#include "common.h"
#include "main/task.h"
#include "actors/actor_420700.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_420700_801325C8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8, 0, 8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_80132644);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_801326F4);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_80132784);

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_3", func_actor_420700_801327EC);
