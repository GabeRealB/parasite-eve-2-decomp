#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/dryfield_factory.h"
#include "rooms/dryfield_night_factory.h"
#include "rooms/room_common.h"

/// Prompt state of the room's script task: clears the cursor highlight and,
/// while `func_800D4EC0` still reports a prompt on screen, runs the cap step
/// the work block names; otherwise it returns to the idle state 2. Either way
/// it re-arms the idle state's delay.
void func_dryfield_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}
