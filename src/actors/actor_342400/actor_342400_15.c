#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_801698d4.h"
#include "actors/actors_shared_8016bef0.h"

void func_actor_342400_801664C4(Task* arg0);
void func_actor_342400_80169CF8(Task* arg0);
void func_actor_342400_80169D2C(Task* arg0);
void func_actor_342400_8016AA9C(Task* arg0);
void func_actor_342400_8016AAB8(Task* arg0);
void func_actor_342400_8016AB6C(Task* arg0);
void func_actor_342400_8016AC80(Task* arg0);
void func_actor_342400_8016AD94(Task* arg0);
s16  func_actor_342400_80169728(Task* arg0, s32 arg1);

void func_actor_342400_80169968(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_342400_8016997C(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

void func_actor_342400_80169990(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;

    work->field_420 = 5;
    work->field_422 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_801699A4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_80169A2C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_80169A98);

void func_actor_342400_80169B04(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AB6C,
        func_actor_342400_801664C4,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_342400_80169B58(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AC80,
        func_actor_342400_8016AD94,
    };

    states[(s16)work->field_422](arg0);
}

void func_actor_342400_80169BAC(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
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

    work = (Actor342400Work*)arg0->idMap;
    sp   = D_actor_342400_80161ED4;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        ActorsShared8016bef0(arg0);
    }
}

extern TaskFuncTable6 D_actor_342400_80162010;

void func_actor_342400_80169C84(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable6   sp;

    work = (Actor342400Work*)arg0->idMap;
    sp   = D_actor_342400_80162010;
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_80169CF8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_80169D2C);

void func_actor_342400_80169DA4(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->idMap;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor342400Work* w = (Actor342400Work*)arg0->idMap;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor342400Work* w = (Actor342400Work*)arg0->idMap;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_15", func_actor_342400_80169E24);

void func_actor_342400_80169EC4(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;

    if (ActorsShared8016974c(arg0)) {
        if (work->field_44F == 1) {
            Actor342400Work* w = (Actor342400Work*)arg0->idMap;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor342400Work* w = (Actor342400Work*)arg0->idMap;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

void func_actor_342400_80169F30(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->idMap;
    s16              angle;
    s16              speed;

    ActorsShared801698d4(arg0, 0x10);
    speed                                           = func_actor_342400_80169728(arg0, -0x10);
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    if (ActorsShared8016974c(arg0)) {
        Actor342400Work* next = (Actor342400Work*)arg0->idMap;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}
