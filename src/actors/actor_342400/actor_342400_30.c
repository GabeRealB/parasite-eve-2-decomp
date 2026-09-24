#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"

void func_actor_342400_8016A370(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;
    s16              angle;
    s16              speed;

    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        speed                                          = func_actor_342400_80169728(arg0, 0x1E);
        angle                                          = work->field_7A + 0x400;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->coords->flg         = 0;
    }
    if (func_actor_342400_8016974C(arg0)) {
        Actor342400Work* next;

        work->field_438 = 0;
        next            = (Actor342400Work*)arg0->work;
        next->field_420 = 3;
        next->field_422 = 0;
    }
}

extern TaskFuncTable4 D_actor_342400_80161F14;

/// Runs the sub-state handler for `field_422` from a four-entry table.
void func_actor_342400_8016A494(Task* arg0)
{
    Actor342400Work* work;
    TaskFuncTable4   sp;

    work = (Actor342400Work*)arg0->work;
    sp   = D_actor_342400_80161F14;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Sets `field_432`, requests animation 7 and advances the sub-state.
void func_actor_342400_8016A4FC(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;

    work             = (Actor342400Work*)arg0->work;
    work->field_432  = 1;
    work2            = (Actor342400Work*)arg0->work;
    work2->field_41C = 0x10;
    work2->field_418 = 7;
    work2->field_414 = 2;
    work->field_422  = work->field_422 + 1;
}
