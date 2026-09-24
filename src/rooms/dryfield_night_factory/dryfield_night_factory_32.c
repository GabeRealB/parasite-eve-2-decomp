#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_factory.h"

/// Runs the script task's current state. The seven handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_dryfield_night_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_night_factory_8017D678;
    sp.funcs[task->state](task);
}
