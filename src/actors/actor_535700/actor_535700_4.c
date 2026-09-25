#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_535700_8013DAA8` as the reset argument, and latches the clip into
/// `field_47E`. Clears the footstep check's record first.
void func_actor_535700_80132730(void)
{
    s32 i;

    D_actor_535700_80146844->field_4B8 = 0;
    i                                  = 1;
    do {
        func_800B4114(&D_actor_535700_80146844->anim, i, (s16)D_actor_535700_80146844->animId, 0,
                      D_actor_535700_8013DAA8);
        i++;
    } while (i < 0x13);
    D_actor_535700_80146844->field_47E = D_actor_535700_80146844->animId;
}

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
