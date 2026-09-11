#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_400600.h"

/* Defined in a later unit of this overlay, or still `INCLUDE_ASM`. */
void func_actor_400600_8013AAD8(Task* arg0);
void func_actor_400600_8013AB44(Task* arg0);

/* Defined in `actor_400600.c`. */
void ActorsShared8013a2c0(Task* arg0);
void func_actor_400600_80138224(Task* arg0, s16 arg1, u8 arg2);
void func_actor_400600_801387DC(Task* arg0, s32 arg1);

extern u8 D_801153F4;

/* Splat-owned in the first unit's leading rodata; see `actor_400600.c`. */
extern const TaskFuncTable12 D_actor_400600_80131E24;
extern const TaskFuncTable9  D_actor_400600_80131EAC;

void func_actor_400600_8013A0F0(Task* arg0)
{
    TaskFuncTable9 states = D_actor_400600_80131EAC;

    states.funcs[arg0->state](arg0);
}

void func_actor_400600_8013A170(Task* arg0)
{
    TmdObject*       model = (TmdObject*)arg0->extra;
    Actor400600Work* work  = (Actor400600Work*)arg0->idMap;
    TaskFuncTable12  fns   = D_actor_400600_80131E24;

    switch (D_801153F4) {
        case 2:
            model->field_C |= 0x80;
            func_actor_400600_801387DC(arg0, -1);
            break;
        case 0:
            fns.funcs[(s16)work->field_71C](arg0);
        case 1:
            ActorsShared8013a2c0(arg0);
            func_actor_400600_80138224(arg0, work->field_73E, work->field_73A);
            break;
    }
}

void func_actor_400600_8013A26C(Task* arg0)
{
    Actor400600Work* work                = (Actor400600Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_400600_8013AAD8,
        func_actor_400600_8013AB44,
    };

    states[(s16)work->field_71C](arg0);
}
