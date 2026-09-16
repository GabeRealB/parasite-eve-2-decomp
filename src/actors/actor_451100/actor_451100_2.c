#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_451100_8013F700;

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_2", ActorsShared80131f9cSub1);

void func_actor_451100_801323B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_2", func_actor_451100_801323DC);

/// The `Gp_AnimResetSlot` twin of the walk below: marks animation slots
/// 1..0x12 as reset-pending and reseeds each of them from the current animation
/// id, then records that id as the one now playing.
void func_actor_451100_80132428(void)
{
    s32 i;

    i = 1;
    do {
        ActorsShared80131f9cWork->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->animId);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}

/// Reseeds animation slots 1..0x12 from the current animation id and records
/// that id as the one now playing.
void func_actor_451100_801324B8(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&ActorsShared80131f9cWork->anim, i, (s16)ActorsShared80131f9cWork->animId, 0,
                      D_actor_451100_8013F700);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_2", func_actor_451100_80132538);
