#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

s32 func_actor_421600_8013E4EC(Task* task)
{
    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        goto return_one;
    }

    if ((((TmdObject*)task->extra)->flags & 0x80) != 0) {
        return 0;
    }

return_one:
    return 1;
}
