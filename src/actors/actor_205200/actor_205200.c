#include "common.h"

#include "actors/actor_205200.h"
#include "main/session.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_205200_8014BD4C(Actor205200* arg0);
void func_actor_205200_8014C67C(Actor205200* arg0);
void func_actor_205200_8014C7CC(Actor205200* arg0);
void func_actor_205200_8014C87C(Actor205200* arg0);
void func_actor_205200_8014C8D4(Actor205200* arg0);
void Gp_DestroyEnemy(void* enemy, void* task);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_80149E54);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014A72C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014A958);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014AB98);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014ACD4);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014AE0C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B048);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B484);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B8C0);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B914);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B94C);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B978);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014B9D4);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014BA94);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014BAE8);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014BD4C);

INCLUDE_RODATA("actors/nonmatchings/actor_205200/actor_205200", D_actor_205200_80149E30);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014BF28);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C0C0);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C540);

void func_actor_205200_8014C59C(Actor205200Ctx* arg0, Actor205200* arg1)
{
    GsCOORDINATE2*    coord;
    Actor205200Obj2C* obj;
    Actor205200Work*  work;
    s32               state;

    work  = arg1->field_1C;
    obj   = arg1->field_2C;
    coord = obj->field_8;
    if (Game_Session->field_1 != 0) {
        return;
    }
    if (work->field_594 != 0) {
        arg1->field_30 = 2;
        return;
    }
    state = D_801153F4;
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
    obj->field_C = 0;
    goto default_body;
case2:
    obj->field_C = 0x80;
    return;
default_body:
    func_actor_205200_8014BD4C(arg1);
    func_actor_205200_8014C67C(arg1);
    func_actor_205200_8014C7CC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    func_actor_205200_8014C87C(arg1);
    func_actor_205200_8014C8D4(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C67C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C748);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C7CC);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C87C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C8D4);

void func_actor_205200_8014C924(Actor205200Ctx* arg0, Actor205200* arg1)
{
    Actor205200Work* work;

    work = arg1->field_1C;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_47C);
    Gp_UnlinkObj(&work->field_4E4);
    Gp_DestroyEnemy(arg0, arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C980);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200", func_actor_205200_8014C9A0);
