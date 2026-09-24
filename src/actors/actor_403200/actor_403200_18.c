#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Whether the actor should keep acting: 1 while its enemy still has HP. Once
/// the enemy is down, 0 if the model carries flag 0x80 or flag 2, and 1
/// otherwise.
s32 func_actor_403200_80141974(Task* task)
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
