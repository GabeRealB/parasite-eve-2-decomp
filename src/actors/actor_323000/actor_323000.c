#include "common.h"

#include "actors/actor_323000.h"

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80162198);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_801624E0);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80162A2C);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80162BD0);

void func_actor_323000_8016331C(Task* task)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor323000AnimWork* work;

    work   = (Actor323000AnimWork*)task->idMap;
    weight = work->field_83C;
    anim   = &work->anim;
    for (i = 1; i < 0x12; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = work->field_83A;
            work->slots[i].field_9      = (u8)(work->field_832 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80163448);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80163A30);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_80163EA0);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_8016409C);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_8016420C);

INCLUDE_RODATA("actors/nonmatchings/actor_323000/actor_323000", D_actor_323000_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_323000/actor_323000", func_actor_323000_801645A4);

void func_actor_323000_8016483C(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_323000/actor_323000", ActorsShared80135df4Table);
