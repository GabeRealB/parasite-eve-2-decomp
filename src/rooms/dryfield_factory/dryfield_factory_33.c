#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_factory.h"
#include "rooms/room_common.h"

void func_dryfield_factory_80181718(Task* task)
{
    TaskFunc states[2] = { func_dryfield_factory_80181BB4, func_dryfield_factory_801810D8 };

    states[task->state](task);
}
