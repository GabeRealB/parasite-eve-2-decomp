#include "common.h"

#include "main/task.h"
#include "rooms/rooms_shared_8017fc38.h"

/// Runs the room monitor's current state. The seven handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void RoomsShared8017fc38(Task* task)
{
    TaskFuncTable7 sp;

    sp = RoomsShared8017fc38Table;
    sp.funcs[task->state](task);
}
