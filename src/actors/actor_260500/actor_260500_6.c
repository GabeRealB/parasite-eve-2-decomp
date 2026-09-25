#include "common.h"

#include "actors/actor_260500.h"

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Blended reseed: reseeds animation slots 1..0x12 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260500_80159D7C`, and records that id as the one now playing.
void func_actor_260500_8014A644(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260500_80159E4C->anim, i, D_actor_260500_80159E4C->field_480, 0,
                      D_actor_260500_80159D7C);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->field_47E = D_actor_260500_80159E4C->field_480;
}

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x24, latching the reset mode -- 1 for the blended
/// `func_actor_260500_8014A644` reseed, 2 for a plain one -- and the reset
/// argument that reseed forwards, then hands the published helper task to the
/// per-frame update. Ids past the range are rejected with -1 and leave the work
/// block untouched.
s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, Actor260500AnimPreset* preset)
{
    if (preset->field_4 < 0x24) {
        D_actor_260500_80159E4C->field_480 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_260500_80159E4C->field_47C = 1;
            D_actor_260500_80159D7C            = preset->field_C;
        } else {
            D_actor_260500_80159E4C->field_47C = 2;
        }
        D_actor_260500_80159E4C->field_482 = 0;
        func_actor_260500_8014A110(D_actor_260500_80159E50);
        return 0;
    }
    return -1;
}
