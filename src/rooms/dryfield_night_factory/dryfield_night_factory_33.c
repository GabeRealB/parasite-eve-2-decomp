#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// The prompt task the script spawns: resets both action-prompt slots, then
/// moves and draws the cursors every frame.
void func_dryfield_night_factory_80181718(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_factory_80181BB4, func_dryfield_night_factory_801810D8 };

    states[task->state](task);
}
