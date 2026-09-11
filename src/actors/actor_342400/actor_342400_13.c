#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

void func_actor_342400_8016B84C(Task* arg0);
void func_actor_342400_8016B914(Task* arg0);

void func_actor_342400_80169408(Task* arg0)
{
    Actor342400Work* work                = (Actor342400Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_342400_8016B84C,
        func_actor_342400_8016B914,
    };

    states[(s16)work->field_420](arg0);
}
