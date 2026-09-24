#include "common.h"

#include "main/task.h"

/// State handlers of the secondary task this entry spawns: set-up
/// (`Actor01100_Fn05E68`), per-frame tick (`Actor01100_Fn06198`) and the
/// countdown to exit (`Actor01100_Fn0736C`).
extern TaskFuncTable3 Actor01100_D000DC;

/// Runs the task's current state handler out of `Actor01100_D000DC`, copying
/// the table onto the stack before the call.
void Actor01100_Fn065E4(Task* task)
{
    TaskFuncTable3 sp;

    sp = Actor01100_D000DC;
    sp.funcs[task->state](task);
}
