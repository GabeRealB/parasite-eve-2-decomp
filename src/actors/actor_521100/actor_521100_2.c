#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_80134934.h"

void      Gp_UpdateCoord(Actor521100Coord* arg0);
void      func_actor_521100_801322F8(Actor521100* arg0, Actor521100Obj2C* arg1, s32 arg2);
void      func_actor_521100_80134C38(Actor521100* arg0);
void      func_actor_521100_80134D88(Actor521100* arg0);
void      func_actor_521100_80134EDC(Actor521100* arg0);
void      func_actor_521100_80135024(Actor521100* arg0);
void      func_actor_521100_801358D4(Actor521100* arg0);
void      func_actor_521100_80135964(Actor521100* arg0);
void      func_actor_521100_80135A90(Actor521100* arg0);
extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", ActorsShared80131e24Sub1);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_2", func_actor_521100_80135414);

void func_actor_521100_80135478(Actor521100Ctx* arg0, Actor521100* arg1)
{
    Actor521100Coord* temp_s2;
    Actor521100Obj2C* temp_a1;
    Actor521100Work*  temp_s1;
    s32               state;
    s32               one;

    temp_a1 = arg1->field_2C;
    state   = D_801153F4;
    temp_s1 = arg1->field_1C;
    temp_s2 = temp_a1->field_8;
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
    temp_a1->field_C                      = 0;
    temp_s1->field_654->field_2C->field_C = 0;
    arg0->field_14                        = 8;
    goto default_body;
case2:
    temp_a1->field_C                      = 0x80;
    temp_s1->field_654->field_2C->field_C = 0x80;
    arg0->field_14                        = one;
    return;
default_body:
    if (temp_s1->field_6B0 == 0) {
        arg0->field_14 = 1;
        return;
    }
    func_actor_521100_801322F8(arg1, temp_a1, one);
    func_actor_521100_801355C8(arg1);
    func_actor_521100_80134C38(arg1);
    func_actor_521100_801358D4(arg1);
    func_actor_521100_80134D88(arg1);
    func_actor_521100_80135964(arg1);
    func_actor_521100_80134EDC(arg1);
    if (temp_s1->field_680 != 0) {
        func_actor_521100_80135024(arg1);
    }
    temp_s2->field_0                  = 0;
    arg1->field_2C->field_8->field_50 = 0;
    Gp_UpdateCoord(temp_s2);
case1:
    ActorsShared80134934((ActorShared80134934*)arg1);
    func_actor_521100_80135A90(arg1);
}
