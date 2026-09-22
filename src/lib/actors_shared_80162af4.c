#include "common.h"

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

s32 ActorsShared80162af4(Task* task, s32 arg1, Actor350500Msg* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->field_2) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}
