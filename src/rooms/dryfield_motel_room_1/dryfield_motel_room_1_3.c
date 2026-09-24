#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_motel_room_1.h"

/// Room entry task: runs the state handler `D_dryfield_motel_room_1_8017D5C4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_motel_room_1_8017D754(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_1_8017D5C4;
    sp.funcs[task->state](task);
}
