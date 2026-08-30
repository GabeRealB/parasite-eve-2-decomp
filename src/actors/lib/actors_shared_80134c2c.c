#include "common.h"

#include "actors/actors_shared_80134c2c.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void ActorsShared80134c2c_Fn32A28(ActorShared80134c2c* arg0, ActorShared80134c2cObj2C* arg1, s32 arg2);
void ActorsShared80134c2c_Fn33EAC(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn34084(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn34CFC(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn34EB8(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn34F60(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn34FF0(ActorShared80134c2c* arg0);
void ActorsShared80134c2c_Fn35048(ActorShared80134c2c* arg0);

extern u8 D_801153F4;

void ActorsShared80134c2c(ActorShared80134c2cCtx* arg0, ActorShared80134c2c* arg1)
{
    GsCOORDINATE2*            temp_s1;
    ActorShared80134c2cObj2C* temp_a1;
    s32                       state;
    s32                       one;

    temp_a1 = arg1->field_2C;
    temp_s1 = temp_a1->field_8;
    state   = D_801153F4;
    one     = 1;
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
    temp_a1->field_C = 0;
    arg0->field_14   = 0;
    goto default_body;
case2:
    temp_a1->field_C = 0x80;
    arg0->field_14   = one;
    return;
default_body:
    ActorsShared80134c2c_Fn32A28(arg1, temp_a1, one);
    ActorsShared80134c2c_Fn34CFC(arg1);
    ActorsShared80134c2c_Fn34084(arg1);
    ActorsShared80134c2c_Fn34F60(arg1);
    ActorsShared80134c2c_Fn34EB8(arg1);
    ActorsShared80134c2c_Fn33EAC(arg1);
    temp_s1->flg = 0;
    Gp_UpdateCoord(temp_s1);
case1:
    ActorsShared80134c2c_Fn34FF0(arg1);
    ActorsShared80134c2c_Fn35048(arg1);
}
