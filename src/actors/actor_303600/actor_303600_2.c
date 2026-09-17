#include "common.h"

#include "actors/actor_303600.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_303600/actor_303600_2", func_actor_303600_801626C0);

/// Per-frame rig motion, run on the work block `func_actor_303600_801626C0`
/// fills in: ramp the 16.16 speed `field_28` toward the limit `field_38` at
/// `field_34` a frame, drop the ramp once the speed passes the limit in the
/// ramp's own direction, integrate the speed into the angle accumulator
/// `field_18`, fold that back into +/-4000, and publish its integer half as the
/// model coordinate's Y.  The accel is read once for the sum and once for the
/// limit test -- the second read is the branch's own copy of it in the target.
void func_actor_303600_801627B8(Task* task)
{
    Actor303600RigWork* work  = (Actor303600RigWork*)task->idMap;
    GsCOORDINATE2*      coord = ((TmdObject*)task->extra)->field_8;
    s32                 speed;
    s32                 angle;
    s32                 var;

    speed          = work->field_28 + work->field_34;
    work->field_28 = speed;
    if (work->field_34 > 0) {
        var = speed > work->field_38;
    } else {
        var = speed < work->field_38;
    }
    if (var != 0) {
        work->field_34 = 0;
    }
    angle            = work->field_18.w + work->field_28;
    work->field_18.w = angle;
    if (angle > 0x0FA00000) {
        work->field_18.w = angle - 0x1F400000;
    } else if (angle < -0x0FA00000) {
        work->field_18.w = angle + 0x1F400000;
    }
    coord->coord.t[1] = work->field_18.half.hi;
    coord->flg        = 0;
}
