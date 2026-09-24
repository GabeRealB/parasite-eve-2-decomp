#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// Script message handler: raises the one-shot trigger the cursor state
/// consumes.
void func_dryfield_night_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}
