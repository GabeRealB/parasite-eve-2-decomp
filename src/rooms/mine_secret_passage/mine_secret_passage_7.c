#include "common.h"

#include "main/task.h"

/// State handlers of the room task: set-up (`func_mine_secret_passage_8017D8C8`),
/// the one-shot state (`func_mine_secret_passage_8017D914`), the idle
/// state and `taskKill`.
extern TaskFuncTable4 D_mine_secret_passage_8017D5C4;

/// Per-frame entry point of the room task: runs the handler of
/// `D_mine_secret_passage_8017D5C4` its state selects. The table is a local
/// copy, so it is copied from `.rodata` onto the stack every frame.
void func_mine_secret_passage_8017D970(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_secret_passage_8017D5C4;
    states.funcs[task->state](task);
}
