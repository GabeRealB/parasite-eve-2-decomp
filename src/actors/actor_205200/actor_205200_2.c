#include "common.h"

#include "actors/actor_205200.h"
#include "actors/actors_shared_80134ff0.h"
#include "main/session.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_205200_8014BD4C(Actor205200* arg0);
void func_actor_205200_8014C67C(Actor205200* arg0);
void func_actor_205200_8014C7CC(Actor205200* arg0);
void func_actor_205200_8014C8D4(Actor205200* arg0);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014B914);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014B94C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014B978);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014B9D4);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BA94);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BAE8);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BD4C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014BF28);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C0C0);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C540);

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
    ActorsShared80134ff0((ActorShared80134ff0*)arg1);
    func_actor_205200_8014C8D4(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C67C);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C748);

INCLUDE_ASM("actors/nonmatchings/actor_205200/actor_205200_2", func_actor_205200_8014C7CC);
