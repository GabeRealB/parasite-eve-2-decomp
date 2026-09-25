#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_335800.h"

#include "main/task.h"
#include "main/tmd.h"

/// State handler 0 of the child block's table `D_actor_335800_80161E68`:
/// turns the root part to face `work->target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances
/// `field_4C2` to the next handler.
void func_actor_335800_80163BE0(Task* task)
{
    Actor335800Work*  work;
    Actor335800Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor335800Work*)task->work;
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
    work->field_4C2++;
}
