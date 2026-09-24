#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"

/// After 0x18 frames sets model flag 2, clears the frame counter, sets
/// `field_451` and advances the state.
void func_actor_342400_8016BD3C(Task* arg0)
{
    u16              ticks;
    Actor342400Work* work;
    TmdObject*       model;

    work            = (Actor342400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->flags    = model->flags | 2;
        work->field_412 = 0U;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}
