#include "common.h"

#include "actors/actor_102600.h"
#include "actors/actors_shared_801355a4.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_102600_8013548C(Actor102600* arg0, Actor102600Obj2C* arg1, s32 arg2);
void func_actor_102600_80131E8C(Actor102600* arg0);
void func_actor_102600_80133EF4(Actor102600* arg0);
void func_actor_102600_80134034(Actor102600* arg0);
void func_actor_102600_80135730(Actor102600* arg0);
void func_actor_102600_801357C4(Actor102600* arg0);
void func_actor_102600_80135888(Actor102600* arg0);
void func_actor_102600_801358E0(Actor102600* arg0);

extern u8 D_801153F4;

INCLUDE_RODATA("actors/nonmatchings/actor_102600/actor_102600", D_actor_102600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80131E8C);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80132574);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80132734);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_801328B4);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80132DC0);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80133108);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_8013325C);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_8013382C);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80133950);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80133EF4);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80134034);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80134184);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_801345A0);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80134774);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80134AB4);

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_80134E1C);

void func_actor_102600_80135378(Actor102600Ctx* arg0, Actor102600* arg1)
{
    GsCOORDINATE2*    coord;
    Actor102600Obj2C* obj;
    Actor102600Work*  work;
    s32               state;
    s32               one;

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
        func_actor_102600_8013548C(arg1, obj, one);
    }
    func_actor_102600_80131E8C(arg1);
    ActorsShared801355a4((ActorShared801355a4*)arg1);
    if (work->field_3B0 != 0) {
        func_actor_102600_80133EF4(arg1);
    }
    if (work->field_3A6 != 0) {
        func_actor_102600_80134034(arg1);
    }
    func_actor_102600_80135730(arg1);
    func_actor_102600_801357C4(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    func_actor_102600_80135888(arg1);
    func_actor_102600_801358E0(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_102600/actor_102600", func_actor_102600_8013548C);

INCLUDE_RODATA("actors/nonmatchings/actor_102600/actor_102600", D_actor_102600_80131E4C);

INCLUDE_RODATA("actors/nonmatchings/actor_102600/actor_102600", D_actor_102600_80131E58);
