#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_342400.h"

/// Moves the model: writes `pos` into the root part's translation and marks
/// the coordinate dirty. `part` is accepted but unused.
void func_actor_342400_80169620(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}
