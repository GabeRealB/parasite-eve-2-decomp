#include "common.h"

#include "main/task.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E30;
extern TaskFuncTable3 D_actor_213000_80149E3C;
extern TaskFuncTable3 D_actor_213000_80149E48;

void func_actor_213000_8014A158(void)
{
}

void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A1B8);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A2C4);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A35C);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A488);

void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A5D0);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A6AC);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A70C);
