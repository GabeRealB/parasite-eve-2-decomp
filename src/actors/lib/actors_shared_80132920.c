#include "common.h"

#include "actors/actors_shared_80132920.h"

#include "main/tmd.h"

void ActorsShared80132920(Task* task)
{
    ActorsShared80132920Work* work;
    GsCOORDINATE2*            coord;
    VECTOR                    vec;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = (ActorsShared80132920Work*)task->idMap;

    vec = ActorsShared80132920Offset;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->turnCount++;
}
