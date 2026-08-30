#include "common.h"

#include "actors/actors_shared_8014d378.h"
#include "actors/actors_shared_801355a4.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

void ActorsShared8014d378_Fn49E8C(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4BEF4(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4C034(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4D48C(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4D730(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4D7C4(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4D888(ActorShared8014d378* arg0);
void ActorsShared8014d378_Fn4D8E0(ActorShared8014d378* arg0);

extern u8 D_801153F4;

void ActorsShared8014d378(ActorShared8014d378Ctx* arg0, ActorShared8014d378* arg1)
{
    s32                       state;
    ActorShared8014d378Obj2C* obj;
    ActorShared8014d378Work*  work;
    GsCOORDINATE2*            coord;

    obj   = arg1->field_2C;
    state = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    if (state == 1) {
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
    arg0->field_14 = 1;
    return;
default_body:
    if (arg0->field_4C != 0) {
        ActorsShared8014d378_Fn4D48C(arg1);
    }
    ActorsShared8014d378_Fn49E8C(arg1);
    ActorsShared801355a4((ActorShared801355a4*)arg1);
    if (work->field_3B0 != 0) {
        ActorsShared8014d378_Fn4BEF4(arg1);
    }
    if (work->field_3A6 != 0) {
        ActorsShared8014d378_Fn4C034(arg1);
    }
    ActorsShared8014d378_Fn4D730(arg1);
    ActorsShared8014d378_Fn4D7C4(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    ActorsShared8014d378_Fn4D888(arg1);
    ActorsShared8014d378_Fn4D8E0(arg1);
}
