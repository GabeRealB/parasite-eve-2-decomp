#include "common.h"

#include "main/task.h"

/// State handlers of the room task: `func_neo_ark_submarine_tunnel_8017F3BC`
/// sets it up, `func_neo_ark_submarine_tunnel_8017F414` runs every later tick,
/// and `taskKill` ends it.
extern const TaskFuncTable3 D_neo_ark_submarine_tunnel_8017D614;

/// Room task tick: copies the three-entry state table
/// `D_neo_ark_submarine_tunnel_8017D614` to the stack and calls the entry for
/// the task's state.
void func_neo_ark_submarine_tunnel_8017F434(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_tunnel_8017D614;
    sp.funcs[task->state](task);
}
