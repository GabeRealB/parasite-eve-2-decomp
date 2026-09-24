#include "common.h"

#include "main/task.h"

#include "actors/actor_341700.h"

/// Turns the heading `field_7A` by `step` toward the nearer player actor
/// (the offset in `field_88` / `field_8C`), leaving it alone within 0x100.
void func_actor_341700_801685F0(Task* arg0, s32 step)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}
