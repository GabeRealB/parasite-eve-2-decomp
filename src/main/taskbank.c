#include "main/task.h"
#include "main/task_types.h"

#include "gameplay/effect_tasks.h"
#include "gameplay/world_collision.h"

void func_8071E24C(Task* arg0);

void func_80722624(Task* arg0);

void func_80723944(Task* arg0);

TaskDesc gTaskBank4Descs[9] = {
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
