#include "common.h"

#include "actors/actors_shared_80162540.h"

#include "main/tmd.h"

void ActorsShared80162540(Task* task)
{
    ActorsShared80162540Work*  work;
    ActorsShared80162540Coord* coord;
    VECTOR                     delta;
    SVECTOR                    dir;
    SVECTOR                    rot;

    work  = (ActorsShared80162540Work*)task->idMap;
    coord = (ActorsShared80162540Coord*)((TmdObject*)task->extra)->field_8;

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
    work->state++;
}
