#include "common.h"

#include "actors/actor_401000.h"
#include "gameplay/1BC.h"
#include "main/tmd.h"

/// Returns 1 while the actor's enemy still has HP. Once it is down, returns 0
/// if the model carries bit 0x80 or lacks bit 2, and 1 otherwise.
s32 func_actor_401000_8013D7C4(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}
