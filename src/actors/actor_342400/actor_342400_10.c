#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016974c.h"
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

extern TaskFuncTable10 D_actor_342400_80161E80;

void func_actor_342400_8016978C(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = D_actor_342400_80161E80;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_10", func_actor_342400_80169810);

void func_actor_342400_80169880(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016AA9C,
        func_actor_342400_8016AAB8,
    };

    states[(s16)work->field_420](arg0);
}
