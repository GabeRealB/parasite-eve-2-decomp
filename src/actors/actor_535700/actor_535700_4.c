#include "common.h"

#include "actors/actor_535700.h"
#include "actors/actors_shared_801326b4.h"

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_4", func_actor_535700_80132730);

s32 func_actor_535700_801327BC(Task* task, s32 arg1, Actor535700AnimPreset* preset, s32 arg3)
{
    if (preset->field_4 < 0x23) {
        ActorsShared80131f9cWork->animId = preset->field_4;
        if (preset->field_8 != 0) {
            ActorsShared80131f9cWork->state = 1;
            D_actor_535700_8013DAA8         = preset->field_C;
        } else {
            ActorsShared80131f9cWork->state = 2;
        }
        ActorsShared80131f9cWork->field_482 = 0;
        func_actor_535700_80132108(ActorsShared801326b4Task);
        return 0;
    }
    return -1;
}
