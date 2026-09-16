#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/268.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014A888);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014A908);

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014A998);

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
                obj->field_C                        = 0;
            }
            break;
        case 2:
            ActorsShared80131f9cWork->field_4F4 = 0;
            obj->field_C                        = 0x84;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_260400/actor_260400_4", func_actor_260400_8014AB50);
