#include "common.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132860.h"
#include "main/tmd.h"
#include "actors/actors_shared_80132920.h"

void ActorsShared801327f8(Task* task)
{
    ActorsShared801327f8Work* work;
    TaskFuncTable4            fns;

    work = (ActorsShared801327f8Work*)task->work;
    fns  = ActorsShared801327f8Table;
    fns.funcs[(s16)work->turnCount](task);
}

void ActorsShared80132860(Task* task)
{
    ActorsShared80132860Work*  work;
    ActorsShared80132860Coord* coord;
    VECTOR                     delta;
    SVECTOR                    dir;
    SVECTOR                    rot;

    work  = (ActorsShared80132860Work*)task->work;
    coord = (ActorsShared80132860Coord*)((TmdObject*)task->extra)->coords;

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

void ActorsShared80132920(Task* task)
{
    ActorsShared80132920Work* work;
    GsCOORDINATE2*            coord;
    VECTOR                    vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (ActorsShared80132920Work*)task->work;

    vec = ActorsShared80132920Offset;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->turnCount++;
}
