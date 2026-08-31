#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_105100.h"
#include "main/sound.h"

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_8018294C(Actor105100* arg0);
void func_actor_105100_80132C2C(Actor105100* arg0);
void func_actor_105100_80133134(Actor105100* arg0);
void func_actor_105100_80133CE4(Actor105100* arg0);
void func_actor_105100_80134130(Actor105100* arg0);
void func_actor_105100_80135E54(Actor105100* arg0);
void func_actor_105100_80136408(Actor105100* arg0);
void func_actor_105100_801364CC(Actor105100* arg0);
void func_actor_105100_80136524(Actor105100* arg0);
void func_actor_105100_80136574(Actor105100* arg0, s32* arg1, s16 arg2, s32 arg3);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80131EBC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132414);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801327B4);

void func_actor_105100_80132AA0(Actor105100Ctx* arg0, Actor105100* arg1)
{
    GsCOORDINATE2*    coord;
    Actor105100Obj2C* obj;
    Actor105100Work*  work;
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
    arg0->field_14 = 8;
    if (work->field_5BC != 0) {
        SndEvt_EnqueueType9(0x40000000);
        work->field_5BC = 0;
    }
    goto default_body;
case1:
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
    goto join_12;
case2:
    obj->field_C   = 0x80;
    arg0->field_14 = one;
join_12:
    SOFT_USE_REG(work);
    if (work->field_5BC == 0) {
        SndEvt_EnqueueType8(0x40000000);
    }
    work->field_5BC = state;
    return;
default_body:
    if (arg0->field_4C != 0) {
        func_actor_105100_80135E54(arg1);
    }
    func_actor_105100_80132C2C(arg1);
    func_actor_105100_80133134(arg1);
    if (work->field_5A2 != 0) {
        func_actor_105100_80133CE4(arg1);
    }
    func_actor_105100_80136408(arg1);
    func_actor_105100_80134130(arg1);
    func_actor_105100_80136574(arg1, &work->field_560, work->field_594, 1);
    if (work->field_5A8 != 0) {
        func_8018294C(arg1);
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_105100_801364CC(arg1);
    func_actor_105100_80136524(arg1);
}

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80132C2C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133134);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013329C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013345C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801336B8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133A14);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80133CE4);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134130);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801347D4);

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100", D_actor_105100_80131E90);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80134B00);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135278);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801354E8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135674);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801359B4);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135B40);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135CEC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135DF8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135E54);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135F50);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80135FCC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801360AC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801361C4);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801362A0);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80136318);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80136408);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801364CC);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80136524);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_80136574);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013667C);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_801366D8);

INCLUDE_ASM("actors/nonmatchings/actor_105100/actor_105100", func_actor_105100_8013672C);

void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->idMap);
    Gp_DestroyEnemy(arg0, arg1);
}
