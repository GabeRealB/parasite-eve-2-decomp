#include "common.h"

#include "actors/actors_shared_80137a20.h"
#include "actors/actors_shared_80137b78.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8009EA50(s32 arg0); /* defined (s32) in src/gameplay/gameplay.c */

void ActorsShared80137a20_Fn31F54(ActorShared80137a20* arg0, ActorShared80137a20Obj2C* arg1, s32 arg2);
void ActorsShared80137a20_Fn34968(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn35BE0(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn37E4C(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn37EF0(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn37FB4(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn38070(ActorShared80137a20* arg0);
void ActorsShared80137a20_Fn3820C(GsCOORDINATE2* arg0, s32 arg1);

extern u8 D_801153F4;

void ActorsShared80137a20(ActorShared80137a20Ctx* arg0, ActorShared80137a20* arg1)
{
    ActorShared80137a20Work*   temp_s1;
    ActorShared80137a20Obj2C*  temp_a1;
    ActorShared80137a20Coords* temp_s2;
    s32                        state;
    s32                        one;

    temp_s1 = arg1->field_1C;
    temp_a1 = arg1->field_2C;
    temp_s2 = temp_a1->field_8;
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
    if (temp_s1->field_6DA != 0) {
        temp_a1->field_C = 0;
    }
    arg0->field_14 = (temp_s1->field_49A >> 0xF) ^ 1;
    goto default_body;
case1:
    ActorsShared80137a20_Fn37FB4(arg1);
    ActorsShared80137a20_Fn38070(arg1);
    return;
case2:
    temp_a1->field_C = 0x80;
    arg0->field_14   = one;
    return;
default_body:
    if (temp_s1->field_6B4 != 0) {
        ActorsShared80137a20_Fn31F54(arg1, temp_a1, one);
        ActorsShared80137b78((ActorShared80137b78*)arg1);
        ActorsShared80137a20_Fn37E4C(arg1);
        ActorsShared80137a20_Fn37EF0(arg1);
        ActorsShared80137a20_Fn35BE0(arg1);
        temp_s2->field_0.flg                  = 0;
        arg1->field_2C->field_8->field_F0.flg = 0;
        Gp_UpdateCoord(&temp_s2->field_0);
        ActorsShared80137a20_Fn37FB4(arg1);
        ActorsShared80137a20_Fn38070(arg1);
        ActorsShared80137a20_Fn3820C(&arg1->field_2C->field_8->field_F0, 0xC);
        ActorsShared80137a20_Fn34968(arg1);
        func_8009EA50(temp_s1->field_6D8);
    }
}
