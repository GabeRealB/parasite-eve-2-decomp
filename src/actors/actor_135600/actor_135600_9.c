#include "common.h"

#include "actors/actor_135600.h"

#include "main/task.h"
#include "main/tmd.h"

/// Step handlers of the motion sequence, indexed by
/// `Actor135600Work::field_4FA`: turn to face `target`, start walking forward,
/// walk until arrival, then turn to the placement yaw.
extern TaskFuncTable4 D_actor_135600_80131E48;

/// The constant local-space offset `func_actor_135600_80132F28` rotates:
/// straight ahead along the part's own +Z.
extern VECTOR D_actor_135600_80131E58;

void func_actor_135600_80132E00(Task* task)
{
    Actor135600Work* work;
    TaskFuncTable4   fns;

    work = (Actor135600Work*)task->work;
    fns  = D_actor_135600_80131E48;
    fns.funcs[(s16)work->field_4FA](task);
}

/// Step 0: turns the root part to face `work->target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_135600_80132E68(Task* task)
{
    Actor135600Work*  work;
    Actor135600Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor135600Work*)task->work;
    coord = (Actor135600Coord*)((TmdObject*)task->extra)->coords;

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

/// Step 1: rotates the forward offset `D_actor_135600_80131E58` through the
/// root part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it, and advances the step.
void func_actor_135600_80132F28(Task* task)
{
    Actor135600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor135600Work*)task->work;

    vec = D_actor_135600_80131E58;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}
