#include "common.h"

#include "actors/actor_103700.h"

/// Moves the actor forward: remembers the root coordinate's position in
/// `field_22C` (where a collision reset returns it), steps it along the
/// matrix's third column scaled by `field_252`, and moves its height 30 units
/// toward the target's `field_23C.vy`.
void Actor03700_Fn0321C(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor103700Work* work;
    s32              y;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor103700Work*)task->work;

    work->field_22C.vx = coord->coord.t[0];
    work->field_22C.vy = coord->coord.t[1];
    work->field_22C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_252) >> 12;
    y                  = coord->coord.t[1];
    coord->coord.t[1]  = (work->field_23C.vy - y > 0) ? y + 30 : y - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_252) >> 12;
}
