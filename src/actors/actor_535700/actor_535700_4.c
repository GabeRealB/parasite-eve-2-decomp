#include "common.h"

#include "actors/actor_535700.h"

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_4", func_actor_535700_80132730);

s32 func_actor_535700_801327BC(Task* task, s32 arg1, Actor535700AnimPreset* preset, s32 arg3)
{
    if (preset->field_4 < 0x23) {
        D_actor_535700_80146844->animId = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_535700_80146844->state = 1;
            D_actor_535700_8013DAA8        = preset->field_C;
        } else {
            D_actor_535700_80146844->state = 2;
        }
        D_actor_535700_80146844->field_482 = 0;
        func_actor_535700_80132108(D_actor_535700_80146848);
        return 0;
    }
    return -1;
}
