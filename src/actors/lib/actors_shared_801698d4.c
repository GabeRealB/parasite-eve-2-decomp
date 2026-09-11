#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_801698d4.h"

void ActorsShared801698d4(Task* arg0, s32 step)
{
    ActorsShared80168d3cWork* work = (ActorsShared80168d3cWork*)arg0->idMap;
    SVECTOR                   vec;
    s32                       diff;
    u16                       angle;
    s32                       yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}
