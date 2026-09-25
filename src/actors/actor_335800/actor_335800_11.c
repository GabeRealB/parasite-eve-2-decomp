#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_335800.h"

#include "main/task.h"
#include "main/tmd.h"

/// Step handlers of the parent block's motion sequence, indexed by
/// `Actor335800MainWork::field_4FA`.
extern TaskFuncTable4 D_actor_335800_80161E3C;

/// Motion handler 1 of the parent block: copies the step-handler table onto
/// the stack and runs the entry `field_4FA` selects.
void func_actor_335800_80162FFC(Task* task)
{
    Actor335800MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor335800MainWork*)task->work;
    fns  = D_actor_335800_80161E3C;
    fns.funcs[work->field_4FA](task);
}

/// Step 0: turns the root part to face `work->target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_335800_80163064(Task* task)
{
    Actor335800MainWork* work;
    Actor335800Coord*    coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor335800MainWork*)task->work;
    coord = (Actor335800Coord*)((TmdObject*)task->extra)->coords;

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

/// Step 1: rotates the constant forward offset `D_actor_335800_80161E4C`
/// through the root part's matrix into `work->step`, opens the per-axis stop
/// threshold to 0x7FFF, which disables it, and advances the step.
void func_actor_335800_80163124(Task* task)
{
    Actor335800MainWork* work;
    GsCOORDINATE2*       coord;
    VECTOR               vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor335800MainWork*)task->work;

    vec = D_actor_335800_80161E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}
