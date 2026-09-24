#include "common.h"

#include "main/task.h"
#include "rooms/acropolis_security_room.h"

extern const AsrMonitorStateTable D_acropolis_security_room_8017D5EC;

/// Runs the security-monitor task's current state. The seven handlers are
/// copied onto the stack first, so the call goes through a local table rather
/// than through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_acropolis_security_room_8017D5EC.states;
    sp.funcs[task->state](task);
}
