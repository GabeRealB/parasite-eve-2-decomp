#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

/// Once the hit flags are set, marks the enemy busy (`field_438`), requests
/// animation 4 and advances the sub-state.
void func_actor_342400_8016B414(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

/// Unless `func_actor_342400_8016945C` takes over, requests animation 0xF
/// and advances the sub-state.
void func_actor_342400_8016B48C(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;

    work = (Actor342400Work*)arg0->work;
    if (func_actor_342400_8016945C(arg0) == 0) {
        work2            = (Actor342400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Unless `func_actor_342400_8016945C` takes over, waits for the hit flags,
/// then marks the enemy busy, requests animation 4 and advances the
/// sub-state.
void func_actor_342400_8016B500(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    Actor342400Work* work3;
    s32              cond;

    work = (Actor342400Work*)arg0->work;
    if ((func_actor_342400_8016945C(arg0) << 0x10) == 0) {
        work2 = (Actor342400Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor342400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}
