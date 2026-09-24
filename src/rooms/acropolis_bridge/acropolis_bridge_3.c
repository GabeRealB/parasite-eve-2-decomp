#include "common.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
extern s8 D_8007216C;

extern s32 D_acropolis_bridge_801917A0;

extern TaskFuncTable3 D_acropolis_bridge_8017D5C4;

/// Three-state dispatcher of the room's own task: setup, an empty idle state,
/// then `taskKill`. The table is copied onto the stack before the call.
void func_acropolis_bridge_8017DA0C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D5C4;
    sp.funcs[task->state](task);
}
