#include "common.h"

#include "main/task.h"

extern const TaskFuncTable3 D_mist_r21_8017D5C4;

void func_mist_r21_8017D708(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r21_8017D5C4;
    sp.funcs[task->state](task);
}
