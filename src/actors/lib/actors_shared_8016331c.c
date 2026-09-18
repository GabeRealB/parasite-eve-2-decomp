#include "common.h"

#include "actors/actors_shared_8016331c.h"

void ActorsShared8016331c(Task* task)
{
    GpAnimPose                   pose;
    GpAnimPose                   blendPose;
    GpAnimCtx*                   anim;
    s16                          weight;
    s16                          i;
    ActorShared8016331cAnimWork* work;

    work   = (ActorShared8016331cAnimWork*)task->work;
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
