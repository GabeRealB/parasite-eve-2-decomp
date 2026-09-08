#include "common.h"

#include "actors/actors_shared_80164904.h"

#include "gameplay/1BC.h"
#include "main/tmd.h"

s32 ActorsShared80164904(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->field_40 > 0) {
        return 1;
    }

    flags   = ((TmdObject*)task->extra)->field_C;
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
