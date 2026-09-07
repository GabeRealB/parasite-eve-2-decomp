#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/gameplay.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E48;
extern TaskFuncTable3 D_actor_511000_80131E54;
extern TaskFuncTable3 D_actor_511000_80131E60;

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801327A0);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_8013287C);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132904);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801329C4);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80132E6C);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133034);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801330F0);

void func_actor_511000_80133220(Task* task)
{
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133240);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801332E4);

void func_actor_511000_801333A4(Task* task)
{
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801333C4);

void func_actor_511000_80133498(Task* task)
{
    Task_Kill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801334C4);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133554);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801336E0);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133760);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_801337F0);

void func_actor_511000_80133850(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E48;
    sp.funcs[task->state](task);
}

void func_actor_511000_801338A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E54;
    sp.funcs[task->state](task);
}

void func_actor_511000_80133900(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E60;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133958);

INCLUDE_ASM("actors/nonmatchings/actor_511000/actor_511000_3", func_actor_511000_80133B80);
