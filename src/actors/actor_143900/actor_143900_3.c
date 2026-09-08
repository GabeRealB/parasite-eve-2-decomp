#include "common.h"

#include "actors/actor_143900.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_143900_801413B8;

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_8013242C);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_143900_801324C8(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x14);
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132514);

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_143900_801325A4(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, ActorsShared80131f9cWork->field_4B8, 0,
                      D_actor_143900_801413B8);
        i++;
    } while (i < 0x14);
    ActorsShared80131f9cWork->field_4B6 = ActorsShared80131f9cWork->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_3", func_actor_143900_80132624);
