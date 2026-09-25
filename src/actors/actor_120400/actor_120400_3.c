#include "common.h"

#include "actors/actor_120400.h"

#include "main/task.h"

/// Global freeze byte in the main executable; the dispatcher runs nothing
/// while it is non-zero.
extern u8 D_801153F4;

/// State dispatcher of the parent task: copies its spawn/tick/teardown table
/// onto the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_120400_80132748(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_120400_80131E30;
    if (D_801153F4 == 0) {
        sp.funcs[task->state](task);
    }
}
