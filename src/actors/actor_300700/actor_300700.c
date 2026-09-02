#include "common.h"

#include "actors/actor_300700.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_300700_801637E4(Actor300700* arg0);
void func_actor_300700_80164794(Actor300700* arg0);
void func_actor_300700_80164E38(Actor300700* arg0, Actor300700Obj2C* arg1, s32 arg2);
void func_actor_300700_80164F68(Actor300700* arg0);
void func_actor_300700_801651A0(Actor300700* arg0);
void func_actor_300700_80165230(Actor300700* arg0);
void func_actor_300700_801652F4(Actor300700* arg0);
void func_actor_300700_8016534C(Actor300700* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80161E80);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80162130);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801622B4);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_8016252C);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801626C0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801628C8);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80162EFC);

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_8016335C);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801633B8);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80163410);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80163510);

INCLUDE_RODATA("actors/nonmatchings/actor_300700/actor_300700", D_actor_300700_80161E30);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801637E4);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80163D64);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164070);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801643D0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801645F8);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164794);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801648E4);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164CE0);

void func_actor_300700_80164D3C(Actor300700Ctx* arg0, Actor300700* arg1)
{
    GsCOORDINATE2*    coord;
    Actor300700Obj2C* obj;
    Actor300700Work*  work;
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
        func_actor_300700_80164E38(arg1, obj, one);
    }
    func_actor_300700_801637E4(arg1);
    func_actor_300700_80164F68(arg1);
    SOFT_USE_REG(work);
    if (work->field_386 != 0) {
        func_actor_300700_80164794(arg1);
    }
    func_actor_300700_801651A0(arg1);
    func_actor_300700_80165230(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    func_actor_300700_801652F4(arg1);
    func_actor_300700_8016534C(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164E38);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80164F68);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80165000);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801650C0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801651A0);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_80165230);

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_801652F4);

void func_actor_300700_8016534C(Actor300700* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x1C0, 0x80);
}

INCLUDE_ASM("actors/nonmatchings/actor_300700/actor_300700", func_actor_300700_8016539C);
