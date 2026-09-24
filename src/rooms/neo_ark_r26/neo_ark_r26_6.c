#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_neo_ark_r26_8017D5C4;

void func_neo_ark_r26_8017D720(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_r26_8017D5C4;
    sp.funcs[task->state](task);
}
