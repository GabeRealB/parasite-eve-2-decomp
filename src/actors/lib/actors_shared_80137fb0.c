#include "common.h"

#include "actors/actors_shared_80137fb0.h"

void ActorsShared80137fb0(ActorShared80137fb0* arg0)
{
    ActorShared80137fb0Work* work;
    GpObj38*                 obj;
    VECTOR                   vec;
    s16                      r;
    s16                      g;
    s16                      b;

    obj    = (GpObj38*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->field_24.t[0];
    vec.vy = obj->field_24.t[1];
    vec.vz = obj->field_24.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
    switch (work->field_6EA) {
        case 1:
            r = 0;
            g = 0;
            b = 0x400;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 2:
            r = 0xFFF;
            g = 0xFFF;
            b = 0xFFF;
            Gp_SetObjTrans((GpObj20*)arg0->field_2C, r, g, b);
            work->field_6EA = 0;
            break;
        case 0:
        default:
            return;
    }
}
