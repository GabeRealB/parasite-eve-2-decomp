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

void func_actor_342400_80169DA4(Task* arg0)
{
    Actor342400Work* work;

    work            = (Actor342400Work*)arg0->work;
    work->field_44F = D_actor_342400_80173A84[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor342400Work* w = (Actor342400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor342400Work* w = (Actor342400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_29", func_actor_342400_80169E24);
