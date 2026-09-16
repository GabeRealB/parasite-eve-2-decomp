#include "common.h"

#include "actors/actor_143900.h"
#include "actors/actors_shared_80133468.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_80132F14);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_80132FB0);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_80133068);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_801330B4);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_80133144);

/// Play-animation handler of the overlay's own variant, the twin of
/// `func_actor_143900_80132624` for the work block published in
/// `D_actor_143900_801496C4`: adopts `preset`'s animation id when it is one of
/// the first 0xC, latches the reset mode and the reset argument the reseed
/// uses, then hands the published task to the per-frame update. Ids past the
/// range are rejected with -1 and leave the work block untouched.
s32 func_actor_143900_801331C4(Task* task, s32 arg1, Actor143900AnimPreset* preset)
{
    if (preset->field_4 < 0xC) {
        D_actor_143900_801496C4->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_143900_801496C4->field_4B4 = 1;
            D_actor_143900_80149630            = preset->field_C;
        } else {
            D_actor_143900_801496C4->field_4B4 = 2;
        }
        D_actor_143900_801496C4->field_4BA = 0;
        func_actor_143900_80132A9C(D_actor_143900_801496C8);
        return 0;
    }
    return -1;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_80133254);

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_801332E4);

/// Message 0x7DB handler from the overlay's handler table (`D_actor_143900_80149634`,
/// entry 0x7DB). The payload's halfword at 0x2 picks which of the two helper
/// tasks the spawn routine parked at 0x4F0 / 0x4F4 keeps its model visible:
/// case 0 hides the 0x4F4 one and shows the 0x4F0 one, case 1 the reverse.
/// `field_C` is the same visibility word `ActorsShared80133468` drives, 0x80
/// shown and 0 hidden.
s32 func_actor_143900_80133360(Task* task, s32 arg1, Actor143900Msg* msg)
{
    ActorShared80133468Obj2C* model4F0;
    ActorShared80133468Obj2C* model4F4;

    model4F0 = ((ActorShared80133468*)D_actor_143900_801496C4->field_4F0)->field_2C;
    model4F4 = ((ActorShared80133468*)D_actor_143900_801496C4->field_4F4)->field_2C;
    switch (msg->field_2) {
        case 0:
            model4F4->field_C = 0;
            model4F0->field_C = 0x80;
            break;
        case 1:
            model4F0->field_C = 0;
            model4F4->field_C = 0x80;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_6", func_actor_143900_801333C4);
