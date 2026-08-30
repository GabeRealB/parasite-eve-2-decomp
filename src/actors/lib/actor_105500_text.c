#include "common.h"

#include "actors/actor_105500.h"

void Actor05500_Fn0006C(Actor105500* arg0);
void Actor05500_Fn020D4(Actor105500* arg0);
void Actor05500_Fn02214(Actor105500* arg0);
void Actor05500_Fn03674(Actor105500* arg0, Actor105500Obj2C* arg1, s32 arg2);
void Actor05500_Fn0378C(Actor105500* arg0);
void Actor05500_Fn03918(Actor105500* arg0);
void Actor05500_Fn039AC(Actor105500* arg0);
void Actor05500_Fn03A70(Actor105500* arg0);
void Actor05500_Fn03AC8(Actor105500* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0006C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L004A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L004C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L004D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L004F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0052C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00754);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00A94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn00FA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn012E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0143C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01A0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn01B30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn020D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02214);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02364);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02780);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02954);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02C94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn02FFC);

void Actor05500_Fn03560(Actor105500Ctx* arg0, Actor105500* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105500Obj2C* obj;
    Actor105500Work*  work;
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
        Actor05500_Fn03674(arg1, obj, one);
    }
    Actor05500_Fn0006C(arg1);
    Actor05500_Fn0378C(arg1);
    if (work->field_3B0 != 0) {
        Actor05500_Fn020D4(arg1);
    }
    if (work->field_3A6 != 0) {
        Actor05500_Fn02214(arg1);
    }
    Actor05500_Fn03918(arg1);
    Actor05500_Fn039AC(arg1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor05500_Fn03A70(arg1);
    Actor05500_Fn03AC8(arg1);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03674);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn0378C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L037CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L037DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L037EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L037FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0380C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0381C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0382C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0383C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L0384C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_L03854);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03864);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03918);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn039AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03A70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03AC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03B60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03C54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03D40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03DD8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03E34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_105500_text", Actor05500_Fn03F88);
