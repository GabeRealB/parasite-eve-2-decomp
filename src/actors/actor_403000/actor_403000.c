#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", D_actor_403000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801327B0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801330D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801332E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133444);

void func_actor_403000_801336B4(Actor403000* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor403000AnimWork* work;

    work   = (Actor403000AnimWork*)arg0->field_1C;
    weight = work->field_AD4;
    anim   = &work->anim;
    for (i = 1; i < 0x18; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_AD2;
            work->slots[i].field_9      = (u8)(work->field_ACA - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_ACA - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801337E0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133AF8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133FC0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134204);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801343B8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134E00);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80135F08);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013603C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801365D0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136B14);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136D68);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801384E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801399A0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013ACBC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B238);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013BDE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C050);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}
