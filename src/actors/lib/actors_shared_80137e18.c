#include "common.h"

#include "actors/actors_shared_80137e18.h"

#include "gameplay/3CD8.h"

void ActorsShared80137e18(ActorShared80137e18* arg0)
{
    GsCOORDINATE2*           parts;
    GsCOORDINATE2*           coord;
    ActorShared80137e18Mat*  mat;
    ActorShared80137e18Work* work;

    work               = arg0->field_1C;
    parts              = arg0->field_2C->field_8;
    coord              = &work->coord;
    coord->sub         = parts;
    parts[1].sub       = coord;
    mat                = (ActorShared80137e18Mat*)&coord->coord;
    mat->ident.m00_m01 = 0x1000;
    mat->ident.m02_m10 = 0;
    mat->ident.m11_m12 = 0x1000;
    mat->ident.m20_m21 = 0;
    mat->ident.m22     = 0x1000;
    coord->flg         = 0;
    parts[1].flg       = 0;
    work->scale.vx     = 0x1000;
    work->scale.vy     = 0x1000;
    work->scale.vz     = 0x1000;
    work->field_36E    = 0;
    if (work->field_36A != 5) {
        Gp_SpawnEff(0x600A5, arg0->field_2C->field_8, 2, NULL);
    }
}
