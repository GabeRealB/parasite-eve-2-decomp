#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// Message 0x13F3 handler of the room's script task: raises the one-shot
/// trigger `field_A` in its work block, which the script's wait state consumes.
void func_dryfield_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}
