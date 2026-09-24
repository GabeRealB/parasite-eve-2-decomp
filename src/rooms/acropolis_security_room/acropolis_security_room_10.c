#include "common.h"

#include "main/task.h"

/// The sixteen state handlers of the room's cap script.
extern const TaskFuncTable16 D_acropolis_security_room_8017D63C;

/// Runs the cap script's current state. The sixteen handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_acropolis_security_room_80180294(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_acropolis_security_room_8017D63C;
    sp.funcs[task->state](task);
}
