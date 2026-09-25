#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"

/// Motion handler 1 of the parent block: copies the step-handler table
/// `D_actor_350700_80161E68` onto the stack and runs the entry `field_4FA`
/// selects.
void func_actor_350700_80163400(Task* task)
{
    Actor350700MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor350700MainWork*)task->work;
    fns  = D_actor_350700_80161E68;
    fns.funcs[(s16)work->field_4FA](task);
}

/// Step 0 of the parent: turns the root part to face `work->target`, taking
/// the yaw of the normalised offset from the part's own translation with
/// `ratan2` and rebuilding the local matrix from that yaw alone, then advances
/// the step.
void func_actor_350700_80163468(Task* task)
{
    Actor350700MainWork* work;
    Actor350700Coord*    coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor350700MainWork*)task->work;
    coord = (Actor350700Coord*)((TmdObject*)task->extra)->coords;

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

/// Step 1 of the parent: rotates the constant forward offset
/// `D_actor_350700_80161E78` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it, and
/// advances the step.
void func_actor_350700_80163528(Task* task)
{
    Actor350700MainWork* work;
    GsCOORDINATE2*       coord;
    VECTOR               vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor350700MainWork*)task->work;

    vec = D_actor_350700_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}
