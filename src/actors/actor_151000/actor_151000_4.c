#include "common.h"

#include "actors/actor_151000.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Starts animation slots 1..0x12 on clip `animId`, forwarding
/// `D_actor_151000_8013D2AC` as the reset argument, and latches the clip into
/// `field_47E`. Clears the footstep check's record first.
void func_actor_151000_801326AC(void)
{
    s32 i;

    D_actor_151000_8013D37C->stepRec = NULL;
    i                                = 1;
    do {
        func_800B4114(&D_actor_151000_8013D37C->anim, i, (s16)D_actor_151000_8013D37C->animId, 0,
                      D_actor_151000_8013D2AC);
        i++;
    } while (i < 0x13);
    D_actor_151000_8013D37C->field_47E = D_actor_151000_8013D37C->animId;
}

/// "Start animation" opcode: `withArg` selects between the two start paths the
/// runner `func_actor_151000_80132084` dispatches on, and only the first carries
/// `animArg`, which it leaves in `D_actor_151000_8013D2AC`. The runner is then
/// run once on the task published in `D_actor_151000_8013D380`. Returns -1,
/// without touching the work block, when the clip id is 0x23 or more.
s32 func_actor_151000_80132738(Task* task, s32 arg1, Actor151000AnimArgs* args, s32 arg3)
{
    if (args->animId < 0x23) {
        D_actor_151000_8013D37C->animId = args->animId;
        if (args->withArg != 0) {
            D_actor_151000_8013D37C->state = 1;
            D_actor_151000_8013D2AC        = args->animArg;
        } else {
            D_actor_151000_8013D37C->state = 2;
        }
        D_actor_151000_8013D37C->field_482 = 0;
        func_actor_151000_80132084(D_actor_151000_8013D380);
        return 0;
    }
    return -1;
}
