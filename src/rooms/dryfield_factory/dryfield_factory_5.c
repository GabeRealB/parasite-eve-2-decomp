#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

extern const NightFactoryCutsceneTable3 D_dryfield_factory_8017D634;

/// Runs the current state of the room's cutscene sequence out of its three
/// handlers, copied onto the stack. A handler returning non-zero has finished
/// its part of the scene, which drops the sequence back to state 0.
void func_dryfield_factory_80180644(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->work;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_factory_8017D634;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}
