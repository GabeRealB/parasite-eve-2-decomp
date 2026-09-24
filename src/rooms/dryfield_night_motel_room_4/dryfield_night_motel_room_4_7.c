#include "common.h"

#include "main/task.h"

/// The room task's three states.
extern const TaskFuncTable3 D_dryfield_night_motel_room_4_8017D5C4;

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_room_4_8017D5C4`.
void func_dryfield_night_motel_room_4_8017D6BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_4_8017D5C4;
    sp.funcs[task->state](task);
}
