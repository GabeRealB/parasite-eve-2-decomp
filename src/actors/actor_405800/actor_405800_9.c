#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_405800.h"

/// Turns the actor's yaw (`field_82`) by `step` toward the world point
/// `target`, of which only `vx` and `vz` are read, leaving it alone while the
/// heading error is within 0x100. Clears the model root's `flg` first so the
/// root is recomputed.
void func_actor_405800_801383CC(Task* arg0, SVECTOR* target, s32 step)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    GsCOORDINATE2*   coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_82;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_82 = angle - step;
    } else if (diff < -0x100) {
        work->field_82 = angle + step;
    }
}
