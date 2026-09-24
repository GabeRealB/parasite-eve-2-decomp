#include "common.h"

#include "main/task.h"

/// State handlers of the room entry task `func_dryfield_motel_room_6_80181B18`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
extern const TaskFuncTable3 D_dryfield_motel_room_6_8017D6B4;

/// Room entry task: runs the state handler `D_dryfield_motel_room_6_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_motel_room_6_80181B18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_6_8017D6B4;
    sp.funcs[task->state](task);
}
