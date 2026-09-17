#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// Runs the current state of the room's cutscene sequence, copying the room's
/// three handlers onto the stack first so the call goes through a local table
/// rather than through `.rodata`. A handler returning non-zero has finished its
/// part of the scene, which drops the sequence back to the shared state 0.
void func_dryfield_night_factory_8017FD5C(Task* task)
{
    NightFactoryCutsceneWork*  work = (NightFactoryCutsceneWork*)task->idMap;
    NightFactoryCutsceneTable3 sp;

    sp = D_dryfield_night_factory_8017D5DC;
    if (sp.funcs[work->state](task) != 0) {
        work->state = 0;
    }
}
