#include "common.h"

#include "actors/actor_356100.h"

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162258);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801625A0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162AEC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162C90);

void func_actor_356100_801633DC(Actor356100* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor356100AnimWork* work;

    work   = (Actor356100AnimWork*)arg0->field_1C;
    weight = work->field_98C;
    anim   = &work->anim;
    for (i = 1; i < 0x15; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_98A;
            work->slots[i].field_9      = (u8)(work->field_982 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_982 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163508);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016382C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163CD4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163E2C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164158);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164ACC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801653F4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80165B30);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166018);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801666B4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801668FC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166CF0);

INCLUDE_RODATA("actors/nonmatchings/actor_356100/actor_356100", D_actor_356100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167358);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167584);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167818);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167A7C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016804C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801684F0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80168AFC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80168E44);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169180);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169854);

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}
