#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

extern u8 D_80071075;
extern u8 D_801153F4;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_403600_8013955C(Actor403600* arg0);
void func_actor_403600_801396F8(Actor403600* arg0);
void func_actor_403600_8013D15C(Actor403600* arg0);
void func_actor_403600_8013DC7C(Actor403600* arg0);
void func_actor_403600_8013F0C0(Actor403600* arg0);
void func_actor_403600_801411D4(Actor403600* arg0, s32 arg1);
void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1);
void func_actor_403600_80141338(Actor403600* arg0);
void func_actor_403600_801414FC(Actor403600* arg0);
void func_actor_403600_8014161C(Actor403600* arg0);
void func_actor_403600_80141A34(Actor403600* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801320F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801327A0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013289C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80132A18);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80132E40);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80134288);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80134398);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801353D0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80135C28);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136224);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136500);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013685C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80136C00);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013700C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80137300);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801375F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801379B4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138004);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801386EC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138C34);

void func_actor_403600_80138C68(Task* arg0)
{
    Gp_UnlinkObj(&((Actor403600Work*)arg0->idMap)->obj);
    Task_Kill(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138C9C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138D9C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138DCC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80138EF8);

void func_actor_403600_8013938C(Actor403600Ctx* arg0, Actor403600* arg1)
{
    s16              temp_a1;
    s16              temp_v0;
    s16              temp_v0_2;
    s32              state;
    Actor403600Work* work;
    GsCOORDINATE2*   var_a0;

    state = D_801153F4;
    work  = arg1->field_1C;
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
    if (work->field_7AC != 0) {
        work->field_7AC = 0;
        SndEvt_EnqueueType9(0x50000000);
    }
    goto default_body;
case1:
    func_actor_403600_801412D0(arg0, arg1);
    if (work->field_7AC == 0) {
        work->field_7AC = state;
        SndEvt_EnqueueType8(0x50000000);
    }
    return;
case2:
    arg1->field_2C->field_C = 0x80;
    arg0->field_14          = 9;
    return;
default_body:
    if (((D_80071075 & 0xF0) == 0x40) && (work->field_7AC == 0)) {
        work->field_7AC = 1;
        SndEvt_EnqueueType8(0x50000000);
    }
    func_actor_403600_801396F8(arg1);
    temp_v0 = work->field_730;
    if (temp_v0 != 0) {
        if (temp_v0 < 0xA) {
            func_actor_403600_8013DC7C(arg1);
            func_actor_403600_8013955C(arg1);
            func_actor_403600_8013D15C(arg1);
        }
    }
    func_actor_403600_801411D4(arg1, 0x14);
    temp_v0_2 = work->field_730;
    if (temp_v0_2 != 0) {
        var_a0 = &work->field_4B8;
        if (temp_v0_2 < 0xA) {
            func_actor_403600_80141338(arg1);
            func_actor_403600_8014161C(arg1);
            func_actor_403600_80141A34(arg1);
            goto block_21;
        }
    } else {
    block_21:
        var_a0 = &work->field_4B8;
    }
    work->field_4B8.flg = 0;
    Gp_UpdateCoord(var_a0);
    func_actor_403600_801412D0(arg0, arg1);
    temp_a1 = work->field_77A;
    if (temp_a1 != 0) {
        Gp_SetObjTrans((GpObj20*)arg1->field_2C, temp_a1, temp_a1, temp_a1);
    }
    func_actor_403600_801414FC(arg1);
    func_actor_403600_8013F0C0(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013955C);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_80131E34);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801396F8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013A444);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013C864);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013CCEC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013D15C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013D9A8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DAF4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DC7C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DDF4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013DFE0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E470);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E66C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013E7D4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013EA04);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F0C0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F608);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013F7B8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8013FC2C);

INCLUDE_RODATA("actors/nonmatchings/actor_403600/actor_403600", D_actor_403600_801320A0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801400BC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80140488);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801406A4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80140B4C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141180);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801411D4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801412D0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141338);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801414FC);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141598);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8014161C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_8014174C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801417A8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141840);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141954);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_801419E8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141A34);

void func_actor_403600_80141B24(Actor403600* arg0)
{
    Actor403600Work* work = arg0->field_1C;

    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x54160001, 1);
    work->field_708 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141B60);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141BE0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141C3C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141C7C);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141CD4);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141D30);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141E78);

void func_actor_403600_80141F28(Actor403600* arg0)
{
    arg0->field_2C->field_8->sub = &Gfx_ViewCoord;
    Gp_EnemyTaskExit((Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600", func_actor_403600_80141F58);
