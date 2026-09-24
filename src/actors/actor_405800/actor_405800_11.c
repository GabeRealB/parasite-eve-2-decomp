#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_8013a0b0.h"
#include "actors/actor_405800.h"

/// Returns 1 when any of bits 0, 1 or 8 of the second animation slot's
/// `slots[1].flags` is set - the walk took the clip's end, followed a control
/// entry, or the clip has settled on its last pose - and 0 otherwise. Bit 0 is
/// read as a halfword and the other two through the word starting there,
/// which is why the work block is seen through `ActorsShared8013a0b0Work`.
s32 func_actor_405800_801385F4(Task* arg0)
{
    ActorsShared8013a0b0Work* work = (ActorsShared8013a0b0Work*)arg0->work;

    if ((work->flags_FC.half & 1) || (work->flags_FC.word & 0x102)) {
        return 1;
    }
    return 0;
}
