#include "task.h"

#include "main/task.h"
#include "main/task_types.h"

#include "gameplay/effect_tasks.h"
#include "gameplay/world_collision.h"

void func_8071E24C(Task* arg0);

void func_80722624(Task* arg0);

void func_80723944(Task* arg0);

/* A bank of task descriptors with no code of its own. Its data lies between
 * libc's sprintf and libgte's cor_04, and only library code is linked between
 * those two, so it is an object on its own in the link. Reached through
 * gTaskDescBanks.
 */

TaskDesc D_800676A8[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80722624 },
    { { { TASK_BODY_NONE, 0xF0 } }, worldCollisionUpdateTask },
    { { { TASK_BODY_NONE, 0xF0 } }, func_8071E24C },
    { { { TASK_BODY_TMD, 0x70 } }, effectBurstModelPartTask },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80723944 },
};
