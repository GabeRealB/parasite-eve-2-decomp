#include "common.h"
#include "actors/actors_shared_80137fb0.h"
#include "actors/actors_shared_8013806c.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void ActorsShared80137fb0(ActorShared80137fb0* arg0)
{
    ActorShared80137fb0Work* work;
    GsCOORDINATE2*           obj;
    VECTOR                   vec;
    s16                      r;
    s16                      g;
    s16                      b;

    obj    = (GsCOORDINATE2*)arg0->field_2C->field_8;
    work   = arg0->field_1C;
    vec.vx = obj->workm.t[0];
    vec.vy = obj->workm.t[1];
    vec.vz = obj->workm.t[2];
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

void ActorsShared8013806c(ActorShared8013806c* arg0)
{
    ActorShared8013806cWork* work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           sub;
    VECTOR3                  vec;

    work  = arg0->field_1C;
    coord = &arg0->field_2C->field_8->field_0;
    sub   = &arg0->field_2C->field_8->field_F0;
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}
