#include "common.h"

#include "main/task.h"

/// The event task's three states: set-up, the per-frame state, and the kill.
extern const TaskFuncTable3 D_dryfield_night_motel_room_2_8017D5C4;

/// Runs the room's event task through its three-state table, which it copies
/// onto the stack before the call.
void func_dryfield_night_motel_room_2_8017D6BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_2_8017D5C4;
    sp.funcs[task->state](task);
}
