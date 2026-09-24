#include "common.h"

#include "actors/actor_444000.h"

#include "gameplay/1BC.h"

/// Advance the three animation pairs with the second context of each blended
/// in at weight `field_7C0`: every slot of the second context ticks at
/// `field_7BE`, every slot of the first at `field_7B6` less 3, and the pose
/// written to the first is the mix of the two.
void func_actor_444000_801341C4(Actor444000* arg0)
{
    GpAnimPose       pose0;
    GpAnimPose       pose1;
    Actor444000Work* work     = arg0->field_1C;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].rate = work->field_7BE;
            work->slots0[i].rate = work->field_7B6 - 3;
            func_800B3448(&work->anim0, i, (s32)&pose0, 0);
            func_800B3448(&work->anim1, i, (s32)&pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].rate = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].rate = work->field_7BE;
        work->slots2[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim2, i, (s32)&pose0, 0);
        func_800B3448(&work->anim3, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].rate = work->field_7BE;
        work->slots4[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim4, i, (s32)&pose0, 0);
        func_800B3448(&work->anim5, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}
