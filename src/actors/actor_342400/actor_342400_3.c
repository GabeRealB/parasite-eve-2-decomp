#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

void func_actor_342400_801664C4(Task* arg0);
void func_actor_342400_80169CF8(Task* arg0);
void func_actor_342400_80169D2C(Task* arg0);
void func_actor_342400_8016AA9C(Task* arg0);
void func_actor_342400_8016AAB8(Task* arg0);
void func_actor_342400_8016AB6C(Task* arg0);
void func_actor_342400_8016AC80(Task* arg0);
void func_actor_342400_8016AD94(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169654);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169728);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016974C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_8016978C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169810);

void func_actor_342400_80169880(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AA9C,
        func_actor_342400_8016AAB8,
    };

    states[(s16)work->field_420](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_801698D4);

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

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_801699A4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169A2C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169A98);

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

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169C00);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169C84);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169CF8);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169D2C);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169DA4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169E24);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169EC4);

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_3", func_actor_342400_80169F30);
