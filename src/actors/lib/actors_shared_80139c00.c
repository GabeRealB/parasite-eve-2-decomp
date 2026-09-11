#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80139c00.h"

void ActorsShared80139c00(Task* arg0, SVECTOR* target, s32 step)
{
    ActorsShared80139c00Work* work = (ActorsShared80139c00Work*)arg0->idMap;
    GsCOORDINATE2*            coords;
    SVECTOR                   vec;
    s32                       diff;
    s32                       yaw;
    u16                       angle;

    coords      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->yaw;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->yaw = angle - step;
    } else if (diff < -0x100) {
        work->yaw = angle + step;
    }
}
