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
void ActorsShared8013a2c0(Task* arg0);
void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2);

extern u8 D_801153F4;

/* Splat-owned in the first unit's leading rodata; see `actor_405800.c`. */
extern const TaskFuncTable12 D_actor_405800_80131E24;

void func_actor_405800_80138698(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    TaskFuncTable12  fns   = D_actor_405800_80131E24;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            break;
        case 0:
            fns.funcs[(s16)work->field_846](arg0);
        case 1:
            ActorsShared8013a2c0(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            break;
    }
}

void func_actor_405800_80138788(Task* arg0)
{
    Actor405800Work* work                = (Actor405800Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_405800_80138EF0,
        func_actor_405800_80138F54,
    };

    states[(s16)work->field_846](arg0);
}
