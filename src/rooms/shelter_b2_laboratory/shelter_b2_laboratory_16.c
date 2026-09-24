#include "common.h"

#include "main/task.h"

/// The three state handlers `func_shelter_b2_laboratory_801804A4` dispatches
/// to.
extern const TaskFuncTable3 D_shelter_b2_laboratory_8017D6BC;

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_laboratory_8017D6BC`.
void func_shelter_b2_laboratory_801804A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_laboratory_8017D6BC;
    sp.funcs[task->state](task);
}
