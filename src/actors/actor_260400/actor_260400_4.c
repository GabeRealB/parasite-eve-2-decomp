#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/268.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014A888);

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x10, latching the reset mode -- 1 for the blended
/// `func_actor_260400_8014A888` reseed, 2 for a plain one -- and the reset
/// argument that reseed forwards, then hands the published helper task to the
/// per-frame update. Ids past the range are rejected with -1 and leave the
/// work block untouched.
s32 func_actor_260400_8014A908(Task* task, s32 arg1, Actor260400AnimPreset* preset)
{
    if (preset->field_4 < 0x10) {
        ActorsShared80131f9cWork->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            ActorsShared80131f9cWork->field_4B4 = 1;
            D_actor_260400_80154BE4             = preset->field_C;
        } else {
            ActorsShared80131f9cWork->field_4B4 = 2;
        }
        ActorsShared80131f9cWork->field_4BA = 0;
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
    helperObj = (TmdObject*)ActorsShared80131f9cWork->field_4F0->extra;

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
    if ((u8)ActorsShared80131f9cWork->field_4F4 == 0) {
        helperObj->flags = 0x84;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014AA28);

/// Message handler: the payload's halfword at 0x2 selects one of three
/// latches. Case 0 latches the work block's animation reset argument; case 1
/// mirrors the step back out and clears the helper task's model field, but
/// only while `func_800B7420(0x88)` still reports the item unheld; case 2
/// clears the mirror and sets the model field to 0x84.
s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, Actor260400Msg* msg)
{
    TmdObject* obj;
    s32        mode;

    obj  = (TmdObject*)ActorsShared80131f9cWork->field_4F0->extra;
    mode = msg->field_2;

    switch (mode) {
        case 0:
            ActorsShared80131f9cWork->field_4EC = 0x14;
            break;
        case 1:
            if (func_800B7420(0x88) == 0) {
                ActorsShared80131f9cWork->field_4F4 = mode;
                obj->flags                          = 0;
            }
            break;
        case 2:
            ActorsShared80131f9cWork->field_4F4 = 0;
            obj->flags                          = 0x84;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014AB50);
