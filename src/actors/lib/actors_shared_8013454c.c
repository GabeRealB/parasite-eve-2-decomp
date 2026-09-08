#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_8013454c.h"

/// Steps the actor's root part one frame along its own facing: saves the
/// current translation into the work block at 0x274 as the previous position,
/// then advances X and Z by the local Z axis (the third column of the rest
/// rotation) scaled by `field_2BE`, and Y by a fixed 0x80.
void ActorsShared8013454c(Task* task)
{
    GsCOORDINATE2*            coord;
    ActorsShared8013454cWork* work;

    coord              = &((TmdObject*)task->extra)->field_8[0];
    work               = (ActorsShared8013454cWork*)task->idMap;
    work->field_274    = coord->coord.t[0];
    work->field_278    = coord->coord.t[1];
    work->field_27C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}
