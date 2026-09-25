#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

/// State 0 of the main-body table `D_actor_141000_80131E58`: turns the root
/// part to face `work->target`, the position the placement handler stored.
/// Normalises the offset from the part's own translation, takes its yaw with
/// `ratan2` and rebuilds the local matrix from that yaw alone, then clears
/// `flg` so the world matrix is recomputed and advances the state.
void func_actor_141000_80133A68(Task* task)
{
    Actor141000Work*  work;
    Actor141000Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor141000Work*)task->work;
    coord = (Actor141000Coord*)((TmdObject*)task->extra)->coords;

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
