#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/268.h"
#include "main/tmd.h"

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Blended reseed: reseeds animation slots 1..0x13 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260400_80154BE4`, and records that id as the one now playing.
void func_actor_260400_8014A888(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260400_80154C70->anim, i, D_actor_260400_80154C70->field_4B8, 0,
                      D_actor_260400_80154BE4);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->field_4B6 = D_actor_260400_80154C70->field_4B8;
}

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x10, latching the reset mode -- 1 for the blended
/// `func_actor_260400_8014A888` reseed, 2 for a plain one -- and the reset
/// argument that reseed forwards, then hands the published helper task to the
/// per-frame update. Ids past the range are rejected with -1 and leave the
/// work block untouched.
s32 func_actor_260400_8014A908(Task* task, s32 arg1, Actor260400AnimPreset* preset)
{
    if (preset->field_4 < 0x10) {
        D_actor_260400_80154C70->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_260400_80154C70->field_4B4 = 1;
            D_actor_260400_80154BE4            = preset->field_C;
        } else {
            D_actor_260400_80154C70->field_4B4 = 2;
        }
        D_actor_260400_80154C70->field_4BA = 0;
        func_actor_260400_8014A200(D_actor_260400_80154C74);
        return 0;
    }
    return -1;
}

/// Step handler: bit 0 of the step mask parks both models' state field at zero
/// when set and at 0x80 when clear, bit 1 ORs in the 0x4 running flag. While
/// the work block's case selector is still 0 the helper task's model is forced
/// to 0x84, which is the animation the actor holds until a message moves it.
s32 func_actor_260400_8014A998(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;
    TmdObject* helperObj;

    obj       = (TmdObject*)D_actor_260400_80154C74->extra;
    helperObj = (TmdObject*)D_actor_260400_80154C70->field_4F0->extra;

    if (arg2 & 1) {
        obj->flags       = 0;
        helperObj->flags = 0;
    } else {
        obj->flags       = 0x80;
        helperObj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags       |= 4;
        helperObj->flags |= 4;
    }
    if ((u8)D_actor_260400_80154C70->field_4F4 == 0) {
        helperObj->flags = 0x84;
    }
    return 0;
}
