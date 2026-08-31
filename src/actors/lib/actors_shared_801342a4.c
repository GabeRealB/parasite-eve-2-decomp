#include "common.h"

#include "actors/actors_shared_801342a4.h"
#include "actors/actors_shared_801344f8.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void ActorsShared801342a4_Fn3230C(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn33658(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn337A8(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn343E8(ActorShared801342a4* arg0, ActorShared801342a4Obj* arg1, s32 arg2);
void ActorsShared801342a4_Fn34778(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn3483C(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn34934(ActorShared801342a4* arg0);
void ActorsShared801342a4_Fn34990(ActorShared801342a4* arg0);

extern u8 D_801153F4;

void ActorsShared801342a4(ActorShared801342a4Ctx* arg0, ActorShared801342a4* arg1)
{
    GsCOORDINATE2*           coord;
    ActorShared801342a4Obj*  obj;
    ActorShared801342a4Work* work;
    s32                      state;
    s32                      one;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        ActorsShared801342a4_Fn343E8(arg1, obj, one);
    }
    ActorsShared801342a4_Fn3230C(arg1);
    if (work->field_378 != 0) {
        if ((work->field_35A == 5) || (work->field_37E != 0)) {
            work->field_35A = 8;
            work->field_35C = 0;
            arg1->field_30  = 2;
        }
    }
    ActorsShared801344f8((ActorShared801344f8*)arg1);
    if (work->field_376 != 0) {
        ActorsShared801342a4_Fn33658(arg1);
    }
    ActorsShared801342a4_Fn337A8(arg1);
    ActorsShared801342a4_Fn34778(arg1);
    ActorsShared801342a4_Fn3483C(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    ActorsShared801342a4_Fn34934(arg1);
    ActorsShared801342a4_Fn34990(arg1);
}
