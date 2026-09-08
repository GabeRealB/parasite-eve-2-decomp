#include "common.h"

#include "actors/actor_151000.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s16 D_actor_151000_8013D2AC;

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_151000_801325C4(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132610);

/// Reseeds animation slots 1..0x12 from `field_480` and latches that id into
/// `field_47E` as the one now playing.
void func_actor_151000_801326AC(void)
{
    s32 i;

    ActorsShared80131f9cWork->field_4B8 = NULL;
    i                                   = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->field_480, 0,
                      D_actor_151000_8013D2AC);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->field_480;
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000_3", func_actor_151000_80132738);
