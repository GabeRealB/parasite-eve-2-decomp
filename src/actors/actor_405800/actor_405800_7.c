#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "actors/actor_405800.h"

void func_actor_405800_80138224(Task* arg0);
void func_actor_405800_80138294(Task* arg0);
s16  func_actor_405800_8013836C(Task* arg0, s16 arg1);

void func_actor_405800_8013A0F4(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    s32              i;

    if (work->field_86E == 1) {
        if (work->field_870 != work->field_872) {
            work->field_874 = 0;
        } else {
            work->field_874 = func_actor_405800_8013836C(arg0, work->field_874);
        }
        func_actor_405800_80138294(arg0);
        work->field_86E = 3;
    } else if (work->field_86E == 2) {
        func_actor_405800_80138224(arg0);
        work->field_86E = 3;
        work->field_874 = 0;
    } else if (work->field_86E == 3) {
        work->field_874++;
    }
    i = 1;
    do {
        work->slots[i].field_9 = work->field_850;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 2;
}

void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_84A = arg3;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 1;
}
