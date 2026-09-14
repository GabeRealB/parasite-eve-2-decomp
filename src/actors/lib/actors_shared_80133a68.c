#include "common.h"

#include "actors/actors_shared_80133a68.h"

#include "main/tmd.h"

void ActorsShared80133a68(Task* task)
{
    ActorsShared80133a68Work*  work;
    ActorsShared80133a68Coord* coord;
    VECTOR                     delta;
    SVECTOR                    dir;
    SVECTOR                    rot;

    work  = (ActorsShared80133a68Work*)task->idMap;
    coord = (ActorsShared80133a68Coord*)((TmdObject*)task->extra)->field_8;

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
    work->state++;
}
