#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "actors/actor_405800.h"

void func_8009EA50(s16 arg0);
s32  func_actor_405800_80136B94(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);
void func_actor_405800_80138EF0(Task* arg0);
void func_actor_405800_80138F54(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_405800/actor_405800_5", func_actor_405800_80138698);

void func_actor_405800_80138788(Task* arg0)
{
    Actor405800Work* work                = (Actor405800Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_405800_80138EF0,
        func_actor_405800_80138F54,
    };

    states[(s16)work->field_846](arg0);
}
