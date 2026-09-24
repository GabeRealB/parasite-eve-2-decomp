#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"

void func_actor_342400_801664C4(Task* arg0);
void func_actor_342400_80169CF8(Task* arg0);
void func_actor_342400_80169D2C(Task* arg0);
void func_actor_342400_8016AA9C(Task* arg0);
void func_actor_342400_8016AAB8(Task* arg0);
void func_actor_342400_8016AB6C(Task* arg0);
void func_actor_342400_8016AC80(Task* arg0);
void func_actor_342400_8016AD94(Task* arg0);

void func_actor_342400_80169968(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_342400_8016997C(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_342400_80169990(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

extern TaskFuncTable3 D_actor_342400_80161EE0;
extern TaskFuncTable5 D_actor_342400_80161EEC;
extern TaskFuncTable5 D_actor_342400_80161F00;

/// Unless `func_actor_342400_80169518` consumes a pending request, runs the
/// sub-state handler for `field_422` from a three-entry table.
void func_actor_342400_801699A4(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161EE0;
    if ((func_actor_342400_80169518(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Runs the sub-state handler for `field_422` from a five-entry table.
void func_actor_342400_80169A2C(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable5   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161EEC;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Runs the sub-state handler for `field_422` from another five-entry table.
void func_actor_342400_80169A98(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable5   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161F00;
    sp.funcs[(s16)work->field_422](arg0);
}

void func_actor_342400_80169B04(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AB6C,
        func_actor_342400_801664C4,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_342400_80169B58(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AC80,
        func_actor_342400_8016AD94,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_342400_80169BAC(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_342400_80169CF8,
        func_actor_342400_80169D2C,
    };

    states[(s16)work->field_422](arg0);
}

extern TaskFuncTable3 D_actor_342400_80161ED4;

void func_actor_342400_80169C00(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable3   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161ED4;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        func_actor_342400_8016BEF0(arg0);
    }
}

extern TaskFuncTable6 D_actor_342400_80162010;

void func_actor_342400_80169C84(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable6   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80162010;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Requests animation 0xC and advances the sub-state.
void func_actor_342400_80169CF8(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 0xC;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}
