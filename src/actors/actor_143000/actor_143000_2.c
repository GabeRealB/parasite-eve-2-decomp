#include "common.h"

#include "actors/actor_143000.h"

void func_actor_143000_801335C8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_143000_80131E84;
    fns.funcs[arg0->state](arg0);
    func_actor_143000_80132D10((Actor143000*)arg0);
}
