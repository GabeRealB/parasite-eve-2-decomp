#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_120400.h"

#include "main/tmd.h"

/// Motion handler 1 of the parent, the walk sequence: copies the step table
/// onto the stack and runs the entry `field_4FA` selects.
void func_actor_120400_801327F8(Task* task)
{
    Actor120400MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor120400MainWork*)task->work;
    fns  = D_actor_120400_80131E3C;
    fns.funcs[work->field_4FA](task);
}

/// Walk step 0: turns the root part to face `target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_120400_80132860(Task* task)
{
    Actor120400MainWork* work;
    Actor120400Coord*    coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor120400MainWork*)task->work;
    coord = (Actor120400Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4FA++;
}

/// Walk step 1: rotates the constant forward offset `D_actor_120400_80131E4C`
/// through the root part's matrix into `step`, opens the arrival threshold to
/// 0x7FFF, which disables it, and advances the step.
void func_actor_120400_80132920(Task* task)
{
    Actor120400MainWork* work;
    GsCOORDINATE2*       coord;
    VECTOR               vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor120400MainWork*)task->work;

    vec = D_actor_120400_80131E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}
