#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"

/// State handler at index 0 of `D_actor_350700_80161E30`: turns the root part
/// toward `work->target`, taking the yaw of the normalised offset from the
/// part's own translation with `ratan2` -- turned half a revolution away while
/// `field_4C4` is clear -- and rebuilding the local matrix from that yaw alone.
/// Clearing `flg` makes the coordinate tree recompute the world matrix, and
/// bumping `field_4C2` moves on to the next handler.
void func_actor_350700_80162540(Task* task)
{
    Actor350700Work*  work;
    Actor350700Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor350700Work*)task->work;
    coord = (Actor350700Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;
    if (work->field_4C4 == 0) {
        rot.vy += 0x7FF;
    }

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4C2++;
}
