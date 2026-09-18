#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

extern void func_dryfield_night_factory_80180DE8(Task*, s16);

/// Runs the prompt state of the night factory script: drops the highlight the
/// previous state left in `D_80114D28` and, while `func_800D4EC0` still reports
/// a prompt on screen, hands the task to the cap step `field_C` names. Once the
/// prompt is gone the task advances to state 2 instead, and either way the work
/// block's `field_8` is set to 0xA.
void func_dryfield_night_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_night_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}
