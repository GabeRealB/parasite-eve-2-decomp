#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "actors/actor_405800.h"

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_7", func_actor_405800_8013A0F4);

void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 2;
}

void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->idMap;
    work->field_84A = arg3;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 1;
}
