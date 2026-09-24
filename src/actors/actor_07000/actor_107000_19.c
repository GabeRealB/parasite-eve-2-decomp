#include "common.h"

#include "actors/actor_107000.h"
#include "main/task.h"
#include "main/tmd.h"

/// Steps the dropping specimen's root part one frame: saves the current
/// translation in `field_274`, advances X and Z along the rotation's Z column
/// scaled by the step length `field_2BE`, and Y by the fall speed `field_2DE`.
void Actor07000_Fn02D78(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor107000Work* work;

    coord              = ((TmdObject*)task->extra)->coords;
    work               = (Actor107000Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += work->field_2DE;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
}
