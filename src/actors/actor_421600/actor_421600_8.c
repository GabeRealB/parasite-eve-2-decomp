#include "common.h"

#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
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
