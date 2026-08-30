#include "common.h"

#include "actors/actors_shared_80133468.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void ActorsShared80133468_Fn31E3C(ActorShared80133468* arg0, ActorShared80133468Obj2C* arg1, s32 arg2);
void ActorsShared80133468_Fn3222C(ActorShared80133468* arg0);
void ActorsShared80133468_Fn33530(ActorShared80133468* arg0);
void ActorsShared80133468_Fn335B8(ActorShared80133468* arg0);
void ActorsShared80133468_Fn33610(ActorShared80133468* arg0);

extern u8 D_801153F4;

void ActorsShared80133468(ActorShared80133468Ctx* arg0, ActorShared80133468* arg1)
{
    GsCOORDINATE2*            temp_s1;
    ActorShared80133468Obj2C* temp_a1;
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
    arg0->field_14   = 8;
    goto default_body;
case1:
    ActorsShared80133468_Fn335B8(arg1);
    return;
case2:
    temp_a1->field_C = 0x80;
    arg0->field_14   = one;
    return;
default_body:
    ActorsShared80133468_Fn31E3C(arg1, temp_a1, one);
    ActorsShared80133468_Fn3222C(arg1);
    ActorsShared80133468_Fn33610(arg1);
    temp_s1->flg = 0;
    Gp_UpdateCoord(temp_s1);
    ActorsShared80133468_Fn335B8(arg1);
    ActorsShared80133468_Fn33530(arg1);
}
