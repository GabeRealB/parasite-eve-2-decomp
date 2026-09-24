#include "common.h"

#include "main/task.h"
#include "rooms/acropolis_patio.h"

void func_acropolis_patio_8017DF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_patio_8017D5C4;
    sp.funcs[task->state](task);
}
