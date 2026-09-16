#include "common.h"

#include "actors/actor_260500.h"

INCLUDE_ASM("actors/nonmatchings/actor_260500/actor_260500_6", func_actor_260500_8014A644);

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x24, latching the reset mode -- 1 for the blended
/// `func_actor_260500_8014A644` reseed, 2 for a plain one -- and the reset
/// argument that reseed forwards, then hands the published helper task to the
/// per-frame update. Ids past the range are rejected with -1 and leave the work
/// block untouched.
s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, Actor260500AnimPreset* preset)
{
    if (preset->field_4 < 0x24) {
        ActorsShared80131f9cWork->field_480 = preset->field_4;
        if (preset->field_8 != 0) {
            ActorsShared80131f9cWork->field_47C = 1;
            D_actor_260500_80159D7C             = preset->field_C;
        } else {
            ActorsShared80131f9cWork->field_47C = 2;
        }
        ActorsShared80131f9cWork->field_482 = 0;
        func_actor_260500_8014A110(ActorsShared801326b4Task);
        return 0;
    }
    return -1;
}
