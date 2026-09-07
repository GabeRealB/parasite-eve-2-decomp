#include "common.h"

#include "actors/actors_shared_80132860.h"

#include "main/tmd.h"

void ActorsShared80132860(Task* task)
{
    ActorsShared80132860Work*  work;
    ActorsShared80132860Coord* coord;
    VECTOR                     delta;
    SVECTOR                    dir;
    SVECTOR                    rot;

    work  = (ActorsShared80132860Work*)task->idMap;
    coord = (ActorsShared80132860Coord*)((TmdObject*)task->extra)->field_8;

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
    work->turnCount++;
}
